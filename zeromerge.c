/* Zero Merge - combine files with empty (zero) blocks
 * Copyright (C) 2020-2026 by Jody Bruchon <jody@jodybruchon.com>
 * Distributed under the GNU General Public License v2 */

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <libjodycode.h>
#include "libjodycode_check.h"
#include "zeromerge_simd.h"
#include "helptext.h"
#include "version.h"


/* Default file read size 16 MiB */
#ifndef DEFAULT_READSIZE
 #define DEFAULT_READSIZE (1048576 * 16)
#endif

static FILE *fp1, *fp2, *fp3;
static size_t active_readsize = DEFAULT_READSIZE;

/* Progress tracking context for recursive mode */
struct progress_context {
	long merged_count;
	int64_t bytes_merged_away;
	int64_t io_byte_total;
	int64_t session_seconds;
	int64_t window_bytes[4];
	int window_seconds[4];
	int window_head;
	int window_size;
};


void clean_exit(void)
{
	if (fp1) jc_fclose(fp1);
	if (fp2) jc_fclose(fp2);
	if (fp3) jc_fclose(fp3);
	return;
}


/* Helper to format bytes using libjodycode size suffixes[cite: 2] (pure whole integers, no decimals) */
static void format_bytes_scaled(int64_t bytes, char *outbuf, size_t outlen)
{
	int i = 0, best_i = 0;
	while (jc_size_suffix[i].suffix != NULL) {
		if (jc_size_suffix[i].shift >= 10 && (int64_t)jc_size_suffix[i].multiplier <= bytes) best_i = i;
		i++;
	}
	if (jc_size_suffix[best_i].shift > 0) {
		uint64_t divisor = 1ULL << jc_size_suffix[best_i].shift;
		int64_t whole = bytes / (int64_t)divisor;
		snprintf(outbuf, outlen, "%" PRId64 " %s", whole, jc_size_suffix[best_i].suffix);
	} else {
		snprintf(outbuf, outlen, "%" PRId64 " B", bytes);
	}
}


/* Core merge execution between two open files into a third open file */
static int execute_merge(FILE *source1, FILE *source2, FILE *dest, int64_t size, size_t readsize, struct progress_context *pctx, int stdout_tty, int quiet_mode)
{
	char *buf1 = NULL, *buf2 = NULL;
	int64_t remain = size;
	size_t read1, read2, written, idx;
	int retval = -1;

	buf1 = (char *)aligned_alloc(32, readsize);
	buf2 = (char *)aligned_alloc(32, readsize);
	if (!buf1 || !buf2) jc_oom("execute_merge() buffer allocation");

	while (remain > 0) {
		size_t to_read = (remain < (int64_t)readsize) ? (size_t)remain : readsize;

		read1 = jc_fread(buf1, 1, to_read, source1);
		if (jc_ferror(source1)) goto cleanup;

		read2 = jc_fread(buf2, 1, to_read, source2);
		if (jc_ferror(source2)) goto cleanup;

		if (read1 != read2) goto cleanup;
		if (read2 == 0) break;

		for (idx = 0; idx < read2; idx++) {
			if (buf1[idx] != buf2[idx] && buf1[idx] != 0 && buf2[idx] != 0) {
				retval = -2;
				goto cleanup;
			}
			buf1[idx] |= buf2[idx];
		}

		written = jc_fwrite(buf1, 1, read2, dest);
		if (written != read2 || jc_ferror(dest)) goto cleanup;

		remain -= (int64_t)read2;
		if (pctx) pctx->io_byte_total += (int64_t)read2;

		if (!quiet_mode && pctx && jc_alarm_ring != 0) {
			pctx->session_seconds += (int64_t)jc_alarm_ring;
			jc_alarm_ring = 0;

			/* Update sliding window */
			pctx->window_head = (pctx->window_head + 1) % 4;
			pctx->window_bytes[pctx->window_head] = pctx->io_byte_total;
			pctx->window_seconds[pctx->window_head] = (int)pctx->session_seconds;
			if (pctx->window_size < 4) pctx->window_size++;

			int oldest_idx = (pctx->window_head - pctx->window_size + 1 + 4) % 4;
			int64_t delta_bytes = pctx->window_bytes[pctx->window_head] - pctx->window_bytes[oldest_idx];
			int64_t delta_secs = (int64_t)(pctx->window_seconds[pctx->window_head] - pctx->window_seconds[oldest_idx]);
			int64_t window_bps = (delta_secs > 0) ? (delta_bytes / delta_secs) : 0;
			int64_t window_mib = window_bps / 1048576;

			int64_t global_bps = (pctx->session_seconds > 0) ? (pctx->io_byte_total / pctx->session_seconds) : 0;
			int64_t global_mib = global_bps / 1048576;

			char freed_str[32], total_io_str[32];
			format_bytes_scaled(pctx->bytes_merged_away, freed_str, sizeof(freed_str));
			format_bytes_scaled(pctx->io_byte_total, total_io_str, sizeof(total_io_str));

			int int_pct = (int)(((size - remain) * 100) / size);

			if (stdout_tty == 1) printf("\r");
			printf("zeromerge: %ld merged, %s/%s freed (%" PRId64 " MiB/sec). Current merge: %d%% done (%" PRId64 " MiB/sec)",
					pctx->merged_count, freed_str, total_io_str, global_mib, int_pct, window_mib);
			if (stdout_tty == 0) printf("\n");
			fflush(stdout);
		}
	}
	retval = 0;

cleanup:
	ALIGNED_FREE(buf1);
	ALIGNED_FREE(buf2);
	return retval;
}


/* File list node for recursive batch mode */
struct file_entry {
	char *path;
	int64_t size;
	uint64_t ino;
	uint32_t dev;
	uint32_t mode;
	struct file_entry *next;
#ifndef ON_WINDOWS
	uid_t uid;
	gid_t gid;
#endif
};


static int compare_file_entries(const void *a, const void *b)
{
	const struct file_entry *fa = *(const struct file_entry * const *)a;
	const struct file_entry *fb = *(const struct file_entry * const *)b;
	if (fa->size < fb->size) return -1;
	if (fa->size > fb->size) return 1;
	return 0;
}


/* Helper to collect files recursively */
static struct file_entry *collect_files(const char *path, struct file_entry *head)
{
	JC_DIR *dir;
	struct JC_DIRENT *entry;
	char fullpath[JC_PATHBUF_SIZE];
	struct JC_STAT st;

	if (jc_stat(path, &st) != 0) return head;
	if (strstr(path, "__zeromerge__.tmp") != NULL) return head;

	if (JC_S_ISDIR(st.st_mode)) {
		dir = jc_opendir(path);
		if (!dir) return head;
		while ((entry = jc_readdir(dir)) != NULL) {
			if (!jc_streq(entry->d_name, ".") || !jc_streq(entry->d_name, "..")) continue;
			if (snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name) >= (int)sizeof(fullpath)) continue;
			head = collect_files(fullpath, head);
		}
		jc_closedir(dir);
	} else if (JC_S_ISREG(st.st_mode)) {
		if (st.st_size <= 0) return head;
		struct file_entry *fe = (struct file_entry *)malloc(sizeof(struct file_entry));
		if (!fe) jc_oom("collect_files entry");
		fe->path = strdup(path);
		if (!fe->path) jc_oom("collect_files path strdup");
		fe->size = st.st_size;
		fe->ino = st.st_ino;
		fe->dev = st.st_dev;
#ifndef ON_WINDOWS
		fe->uid = st.st_uid;
		fe->gid = st.st_gid;
#endif
		fe->mode = st.st_mode;
		fe->next = head;
		head = fe;
	}
	return head;
}


/* Recursive batch merge handler (-r) with unified cleanup and advanced progress */
static int run_recursive_merge(int argc, char **argv, int optind, int stdout_tty, int quiet_mode)
{
	struct file_entry *head = NULL, *curr, *nxt;
	int count = 0, i, retval = 0;
	struct file_entry **arr = NULL;
	struct progress_context pctx;

	memset(&pctx, 0, sizeof(pctx));

	if (optind >= argc) head = collect_files(".", NULL);
	else for (i = optind; i < argc; i++) head = collect_files(argv[i], head);

	for (curr = head; curr != NULL; curr = curr->next) count++;
	if (count < 2) {
		fprintf(stderr, "Not enough files found for merging.\n");
		goto cleanup;
	}

	arr = (struct file_entry **)malloc(sizeof(struct file_entry *) * (size_t)count);
	if (!arr) jc_oom("recursive merge array");
	i = 0;
	for (curr = head; curr != NULL; curr = curr->next) arr[i++] = curr;

	qsort(arr, (size_t)count, sizeof(struct file_entry *), compare_file_entries);

	if (!quiet_mode) {
		jc_start_alarm(1, 1);
		jc_alarm_ring = 1;
	}

	for (i = 0; i < count - 1; i++) {
		if (arr[i]->size == arr[i+1]->size) {
			struct file_entry *f1 = arr[i];
			struct file_entry *f2 = arr[i+1];
			char tmpname[JC_PATHBUF_SIZE];
			FILE *sf1 = NULL, *sf2 = NULL, *sout = NULL;
			int mres;

			if (f1->dev == f2->dev && f1->ino == f2->ino) continue;

			snprintf(tmpname, sizeof(tmpname), "%s.__zeromerge__.tmp", f1->path);
			jc_remove(tmpname);

			sf1 = jc_fopen(f1->path, JC_FILE_MODE_RDONLY_SEQ);
			sf2 = jc_fopen(f2->path, JC_FILE_MODE_RDONLY_SEQ);
			sout = jc_fopen(tmpname, JC_FILE_MODE_WRONLY_SEQ);

			if (!sf1 || !sf2 || !sout) {
				if (sf1) jc_fclose(sf1);
				if (sf2) jc_fclose(sf2);
				if (sout) jc_fclose(sout);
				jc_remove(tmpname);
				continue;
			}

			mres = execute_merge(sf1, sf2, sout, f1->size, active_readsize, &pctx, stdout_tty, quiet_mode);
			jc_fclose(sf1);
			jc_fclose(sf2);
			jc_fclose(sout);

			if (mres == 0) {
				int same_dev = (f1->dev == f2->dev);
				int success = 0;
				if (same_dev) {
					jc_remove(f1->path);
					jc_remove(f2->path);
					if (jc_link(tmpname, f1->path) == 0 && jc_link(tmpname, f2->path) == 0) success = 1;
				} else {
					FILE *tsrc = jc_fopen(tmpname, JC_FILE_MODE_RDONLY_SEQ);
					FILE *tdest1 = jc_fopen(f1->path, JC_FILE_MODE_WRONLY_SEQ);
					FILE *tdest2 = jc_fopen(f2->path, JC_FILE_MODE_WRONLY_SEQ);
					if (tsrc && tdest1 && tdest2) {
						char copybuf[32768];
						size_t r;
						int copy_ok = 1;
						while ((r = jc_fread(copybuf, 1, sizeof(copybuf), tsrc)) > 0) {
							if (jc_fwrite(copybuf, 1, r, tdest1) != r || jc_fwrite(copybuf, 1, r, tdest2) != r) {
								copy_ok = 0;
								break;
							}
						}
						if (copy_ok) success = 1;
					}
					if (tsrc) jc_fclose(tsrc);
					if (tdest1) jc_fclose(tdest1);
					if (tdest2) jc_fclose(tdest2);
				}

				if (success) {
					jc_chmod(f1->path, f1->mode);
					jc_chmod(f2->path, f2->mode);
#ifndef ON_WINDOWS
					chown(f1->path, f1->uid, f1->gid);
					chown(f2->path, f2->uid, f2->gid);
#endif
					pctx.merged_count++;
					pctx.bytes_merged_away += f1->size;

					struct JC_STAT st_new;
					if (jc_stat(f1->path, &st_new) == 0) {
						f2->ino = st_new.st_ino;
						f2->dev = st_new.st_dev;
					}
				}
			}
			jc_remove(tmpname);
		}
	}

	if (!quiet_mode) {
		jc_stop_alarm();
		if (stdout_tty == 1) printf("\r");
		printf("zeromerge: %ld merged successfully.                                           \n", pctx.merged_count);
	}

cleanup:
	curr = head;
	while (curr) {
		nxt = curr->next;
		free(curr->path);
		free(curr);
		curr = nxt;
	}
	free(arr);
	return retval;
}


#ifdef UNICODE
int wmain(int argc, wchar_t **wargv)
#else
int main(int argc, char **argv)
#endif
{
	const char *file1, *file2, *file3;
	struct JC_STAT stat1, stat2;
	int64_t remain, progress;
	size_t read1, read2, written, idx;
	int32_t err1 = 0, err2 = 0, err3 = 0;
	int seconds = 0;
	int stdout_tty = 0;
	int hide_progress = 0;
	int quiet_mode = 0;
	const char *prog_suffix = "B";
	int prog_shift = 0;
	int64_t processed = 0;
	int recursive_mode = 0;
	int arg_index = 1;
#ifdef UNICODE
	char **argv;
#endif

	atexit(clean_exit);

	if (libjodycode_version_check(1, 0) != 0) exit(EXIT_FAILURE);

#ifdef UNICODE
	if (jc_setup_unicode_terminal(argc, wargv, &argv, &stdout_tty) != 0) exit(EXIT_FAILURE);
#else
	if (jc_setup_unicode_terminal(argc, argv, &argv, &stdout_tty) != 0) exit(EXIT_FAILURE);
#endif

	set_program_name(argv[0]);

	/* Parse command-line options */
	while (arg_index < argc) {
		if (!jc_streq(argv[arg_index], "-h") || !jc_streq(argv[arg_index], "--help")) {
			usage();
			exit(EXIT_SUCCESS);
		}
		if (!jc_streq(argv[arg_index], "-v") || !jc_streq(argv[arg_index], "-V") || !jc_streq(argv[arg_index], "--version")) {
			version();
			exit(EXIT_SUCCESS);
		}
		if (!jc_streq(argv[arg_index], "-r") || !jc_streq(argv[arg_index], "--recursive")) {
			recursive_mode = 1;
			arg_index++;
		} else if (!jc_streq(argv[arg_index], "-q") || !jc_streq(argv[arg_index], "--quiet")) {
			quiet_mode = 1;
			hide_progress = 1;
			arg_index++;
		} else if (!jc_streq(argv[arg_index], "-C")) {
			if (arg_index + 1 >= argc) {
				fprintf(stderr, "Error: -C requires a numeric argument (0-18)\n");
				exit(EXIT_FAILURE);
			}
			char *endptr = NULL;
			long val = strtol(argv[arg_index + 1], &endptr, 10);
			if (*endptr != '\0' || val < 0 || val > 18) {
				fprintf(stderr, "Error: invalid chunk size index '%s' (must be 0-18)\n", argv[arg_index + 1]);
				exit(EXIT_FAILURE);
			}
			active_readsize = (size_t)4096 << val;
			arg_index += 2;
		} else break;
	}

	if (recursive_mode) return run_recursive_merge(argc, argv, arg_index, stdout_tty, quiet_mode);

	if (argc - arg_index != 3) goto error_argcount;

	file1 = argv[arg_index];
	file2 = argv[arg_index + 1];
	file3 = argv[arg_index + 2];

	fp1 = jc_fopen(file1, JC_FILE_MODE_RDONLY_SEQ);
	if (!fp1) goto error_file1;
	fp2 = jc_fopen(file2, JC_FILE_MODE_RDONLY_SEQ);
	if (!fp2) goto error_file2;

	if (jc_stat(file1, &stat1) != 0) goto error_file1;
	if (jc_stat(file2, &stat2) != 0) goto error_file2;

	if (stat1.st_ino == stat2.st_ino && stat1.st_dev == stat2.st_dev) goto error_same_file;
	if (stat1.st_size != stat2.st_size) goto error_file_sizes;
	if (stat1.st_size <= 0) goto error_empty_file;
	remain = stat1.st_size;

	for (int i = 0; jc_size_suffix[i].suffix != NULL; i++) {
		if ((uint64_t)jc_size_suffix[i].multiplier < (uint64_t)stat1.st_size || jc_size_suffix[i].shift == 20) {
			prog_shift = jc_size_suffix[i].shift;
			prog_suffix = jc_size_suffix[i].suffix;
		}
		if (prog_shift == 20) break;
	}

	fp3 = jc_fopen(file3, JC_FILE_MODE_WRONLY_SEQ);
	if (!fp3) goto error_file3;

	char *buf1 = (char *)aligned_alloc(32, active_readsize);
	char *buf2 = (char *)aligned_alloc(32, active_readsize);
	if (!buf1 || !buf2) jc_oom("main() buffer allocation");

	if (hide_progress == 0) {
		jc_start_alarm(1, 1);
		jc_alarm_ring = 1;
	}

	while (remain > 0) {
		size_t to_read = (remain < (int64_t)active_readsize) ? (size_t)remain : active_readsize;

		jc_errno = 0;
		read1 = jc_fread(buf1, 1, to_read, fp1);
		if (jc_ferror(fp1)) { err1 = jc_errno ? jc_errno : errno; goto error_short_read; }

		jc_errno = 0;
		read2 = jc_fread(buf2, 1, to_read, fp2);
		if (jc_ferror(fp2)) { err2 = jc_errno ? jc_errno : errno; goto error_short_read; }

		if (read1 != read2) goto error_short_read;
		if (read2 == 0) break;

		idx = 0;
		for (; idx < read2; idx++) {
			if (buf1[idx] != buf2[idx] && buf1[idx] != 0 && buf2[idx] != 0) goto error_different;
			buf1[idx] |= buf2[idx];
		}

		written = jc_fwrite(buf1, 1, read2, fp3);
		if (written != read2 || jc_ferror(fp3)) {
			err3 = jc_errno ? jc_errno : errno;
			goto error_short_write;
		}
		remain -= (int64_t)read2;
		processed += (int64_t)read2;

		if (hide_progress == 0 && jc_alarm_ring != 0) {
			seconds += jc_alarm_ring;
			jc_alarm_ring = 0;
			progress = stat1.st_size - remain;
			if (stdout_tty == 1) printf("\r");
			printf("[zeromerge] Progress: %" PRId64 "%%, %" PRId64 " of %" PRId64 " %s (%" PRId64 " %s/sec)",
					(progress * 100) / stat1.st_size,
					progress >> prog_shift,
					stat1.st_size >> prog_shift,
					prog_suffix,
					(seconds > 0) ? (processed / seconds) >> prog_shift : 0,
					prog_suffix);
			if (stdout_tty == 0) printf("\n");
			fflush(stdout);
		}
		if (jc_feof(fp1) && jc_feof(fp2)) break;
	}
	ALIGNED_FREE(buf1);
	ALIGNED_FREE(buf2);

	if (hide_progress == 0) {
		jc_stop_alarm();
		if (stdout_tty == 1) printf("\r");
		printf("[zeromerge] merge complete.");
		if (stdout_tty == 0) printf("\n");
		else printf("                                            \n");
	}
	exit(EXIT_SUCCESS);

error_same_file:
	fprintf(stderr, "\nError: the input files appear to be hard linked\n");
	exit(EXIT_FAILURE);
error_different:
	fprintf(stderr, "\nError: files contain different non-zero data\n");
	exit(EXIT_FAILURE);
error_argcount:
	usage();
	exit(EXIT_FAILURE);
error_file1:
	fprintf(stderr, "\nError opening/reading "); jc_fwprint(stderr, file1, 1);
	exit(EXIT_FAILURE);
error_file2:
	fprintf(stderr, "\nError opening/reading "); jc_fwprint(stderr, file2, 1);
	exit(EXIT_FAILURE);
error_file3:
	fprintf(stderr, "\nError opening "); jc_fwprint(stderr, file3, 1);
	exit(EXIT_FAILURE);
error_file_sizes:
	fprintf(stderr, "\nError: file sizes are not identical\n");
	exit(EXIT_FAILURE);
error_short_read:
	fprintf(stderr, "\nError: short read: file 1: %s, file 2: %s\n",
			err1 ? jc_strerror(err1) : "unexpected EOF",
			err2 ? jc_strerror(err2) : "unexpected EOF");
	exit(EXIT_FAILURE);
error_short_write:
	fprintf(stderr, "\nError: short write (%zu != %zu): %s\n",
			written, read2, err3 ? jc_strerror(err3) : "unknown error");
	exit(EXIT_FAILURE);
error_empty_file:
	fprintf(stderr, "\nError: cannot merge files that are 0 bytes in size\n");
	exit(EXIT_FAILURE);
}
