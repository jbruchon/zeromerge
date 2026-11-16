/* Zero Merge - combine files with empty (zero) blocks
 * Copyright (C) 2020-2026 by Jody Bruchon <jody@jodybruchon.com>
 * Distributed under The MIT License
 */

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libjodycode.h>
#include "libjodycode_check.h"
#include "zeromerge_simd.h"
#include "version.h"

/* File read size */
#ifndef READSIZE
 #define READSIZE (1048576 * 16)
#endif

static char program_name[PATH_MAX + 4];
static FILE *fp1, *fp2, *fp3;


void clean_exit(void)
{
	if (fp1) jc_fclose(fp1);
	if (fp2) jc_fclose(fp2);
	if (fp3) jc_fclose(fp3);
	return;
}


static void version(void)
{
	printf("zeromerge %s (%s) by Jody Bruchon <jody@jodybruchon.com>\n", VER, VERDATE);
	printf("Linked to libjodycode %s (%s)\n", LIBJODYCODE_VER, LIBJODYCODE_VERDATE);
	printf("Read buffer size is %u KiB\n", (unsigned int)(READSIZE >> 10));
	printf("\nLatest versions and support:\n");
	printf("        https://codeberg.org/jbruchon/zeromerge\n");
	printf("\nPlease consider supporting continued development with funding options at:\n");
	printf("        https://www.JodyBruchon.com/\n\n");
	return;
}


static void usage(void)
{
	printf("Usage: %s file1 file2 outfile\n", program_name);
	return;
}


#ifdef UNICODE
int wmain(int argc, wchar_t **wargv)
#else
int main(int argc, char **argv)
#endif
{
	char *buf1, *buf2;
	const char *file1, *file2, *file3;
	struct JC_STAT stat1, stat2;
	int64_t remain, progress;
	size_t read1, read2, written, idx;
	int32_t err1 = 0, err2 = 0, err3 = 0;
	int seconds = 0;
	int stdout_tty = 0;
	int hide_progress = 0;
	const char *prog_suffix = "B";
	int prog_shift = 0;
	int64_t processed = 0;
#ifdef UNICODE
	char **argv;
#endif

	atexit(clean_exit);

	/* Verify libjodycode compatibility before going further */
	if (libjodycode_version_check(1, 0) != 0) exit(EXIT_FAILURE);

#ifdef UNICODE
	if (jc_setup_unicode_terminal(argc, wargv, &argv, &stdout_tty) != 0) exit(EXIT_FAILURE);
#else
	if (jc_setup_unicode_terminal(argc, argv, &argv, &stdout_tty) != 0) exit(EXIT_FAILURE);
#endif

	snprintf(program_name, sizeof(program_name), "%s", argv[0]);

	/* Help or version text if requested */
	if (argc >= 2) {
		if (!jc_streq(argv[1], "-h") || !jc_streq(argv[1], "--help")) {
			usage();
			exit(EXIT_SUCCESS);
		}
		if (!jc_streq(argv[1], "-v") || !jc_streq(argv[1], "-V") || !jc_streq(argv[1], "--version")) {
			version();
			exit(EXIT_SUCCESS);
		}
	}

	if (argc != 4) goto error_argcount;

	/* Set up file names to open */
	file1 = argv[1]; file2 = argv[2]; file3 = argv[3];

	/* Open files to merge */
	fp1 = jc_fopen(file1, JC_FILE_MODE_RDONLY_SEQ);
	if (!fp1) goto error_file1;
	fp2 = jc_fopen(file2, JC_FILE_MODE_RDONLY_SEQ);
	if (!fp2) goto error_file2;

	/* File sizes must match; sizes also needed for loop */
	if (jc_stat(file1, &stat1) != 0) goto error_file1;
	if (jc_stat(file2, &stat2) != 0) goto error_file2;

	/* Hard link / same-file protection checks */
	if (stat1.st_ino == stat2.st_ino && stat1.st_dev == stat2.st_dev) goto error_same_file;
	if (stat1.st_size != stat2.st_size) goto error_file_sizes;
	if (stat1.st_size <= 0) goto error_empty_file;
	remain = stat1.st_size;

	/* Refuse to overwrite an existing output file */
	if (jc_access(file3, JC_F_OK) == 0) goto error_outfile_exists;

	/* Set units for progress indicator */
	for (int i = 0; jc_size_suffix[i].suffix != NULL; i++) {
		if (jc_size_suffix[i].multiplier < stat1.st_size || jc_size_suffix[i].shift == 20) {
			prog_shift = jc_size_suffix[i].shift;
			prog_suffix = jc_size_suffix[i].suffix;
		}
		/* Limit to binary units */
		if (prog_shift == 20) break;
	}

	/* If read and size check are OK, open file to write into */
	fp3 = jc_fopen(file3, JC_FILE_MODE_WRONLY_SEQ);
	if (!fp3) goto error_file3;

	buf1 = (char *)aligned_alloc(32, READSIZE);
	buf2 = (char *)aligned_alloc(32, READSIZE);
	if (!buf1 || !buf2) jc_oom("main() buffer allocation");

	/* Start progress timer and force immediate update */
	if (hide_progress == 0) {
		jc_start_alarm(1, 1);
		jc_alarm_ring = 1;
	}

	/* Main loop */
	while (remain > 0) {
		jc_errno = 0;
		read1 = jc_fread(buf1, 1, READSIZE, fp1);
		if (jc_ferror(fp1)) { err1 = jc_errno ? jc_errno : errno; goto error_short_read; }

		jc_errno = 0;
		read2 = jc_fread(buf2, 1, READSIZE, fp2);
		if (jc_ferror(fp2)) { err2 = jc_errno ? jc_errno : errno; goto error_short_read; }

		if (read1 != read2) goto error_short_read;
		if (read2 == 0) break;

		idx = 0;
#if 0
#if (defined __GNUC__ || defined __clang__) && (defined USE_AVX2 || defined USE_SSE2)
		if (read2 >= 32) {
			__builtin_cpu_init();
#ifdef USE_AVX2
			if (__builtin_cpu_supports("avx2")) idx = zero_merge_avx2(idx, read2, buf1, buf2);
#endif
#ifdef USE_SSE2
			else if (__builtin_cpu_supports("sse2")) idx = zero_merge_sse2(idx, read2, buf1, buf2);
#endif
		}
#endif
#endif /* 0 */

		/* Merge data between buffers */
		for (; idx < read2; idx++) {
			/* Error if both bytes are non-zero and different */
			if (buf1[idx] != buf2[idx] && buf1[idx] != 0 && buf2[idx] != 0)
				goto error_different;
			/* merge data into buf1 */
			buf1[idx] |= buf2[idx];
		}

		jc_errno = 0;
		written = jc_fwrite(buf1, 1, read2, fp3);
		if (written != read2 || jc_ferror(fp3)) {
			err3 = jc_errno ? jc_errno : errno;
			goto error_short_write;
		}
		remain -= (int64_t)read2;
		processed += (int64_t)read2;

		/* Progress indicator */
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
					(processed / seconds) >> prog_shift,
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
error_outfile_exists:
	fprintf(stderr, "\nError: output file already exists: "); jc_fwprint(stderr, file3, 1);
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
