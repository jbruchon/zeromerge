/* Zero Merge - combine files with empty (zero) blocks
 * Copyright (C) 2020-2023 by Jody Bruchon <jody@jodybruchon.com>
 * Distributed under The MIT License
 */

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include <libjodycode.h>
#include "libjodycode_check.h"
#include "zeromerge_simd.h"
#include "version.h"

/* File read size */
#ifndef READSIZE
 #define READSIZE (1048576 * 16)
#endif

/* Detect Windows and modify as needed */
#if defined _WIN32 || defined __CYGWIN__
 #ifndef ON_WINDOWS
  #define ON_WINDOWS 1
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <io.h>
#else /* Not Windows */
 #ifdef UNICODE
  #error Do not define UNICODE on non-Windows platforms.
  #undef UNICODE
 #endif
#endif /* _WIN32 || __CYGWIN__ */


static char program_name[PATH_MAX + 4];
static FILE *fp1, *fp2, *fp3;


void clean_exit(void)
{
	if (fp1) fclose(fp1);
	if (fp2) fclose(fp2);
	if (fp3) fclose(fp3);
	return;
}


static void version(void)
{
	printf("zeromerge %s (%s) by Jody Bruchon <jody@jodybruchon.com>\n", VER, VERDATE);
	printf("Linked to libjodycode %s (%s)\n", LIBJODYCODE_VER, LIBJODYCODE_VERDATE);
	printf("Read buffer size is %u KiB\n", (unsigned int)(READSIZE >> 10));
	printf("\nLatest versions and support:\n");
	printf("        https://github.com/jbruchon/zeromerge\n");
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
	char *buf1, *buf2, *file1, *file2, *file3;
	struct JC_STAT stat1, stat2;
	off_t remain;
	off_t read1, read2, write;
	off_t progress;
	int seconds = 0;
	int stdout_tty = 0;
	int hide_progress = 0;
	const char *prog_suffix = "B";
	int prog_shift = 0;
	intmax_t processed = 0;

	atexit(clean_exit);

#ifdef ON_WINDOWS
	/* Windows buffers our std output; don't let it do that */
	if (setvbuf(stdout, NULL, _IONBF, 0) != 0 || setvbuf(stderr, NULL, _IONBF, 0) != 0)
		fprintf(stderr, "warning: setvbuf() failed\n");
	if (_isatty(_fileno(stdout))) stdout_tty = 1;
#else
	if (isatty(fileno(stdout))) stdout_tty = 1;
#endif /* ON_WINDOWS */

	/* Verify libjodycode compatibility before going further */
	if (libjodycode_version_check(1, 0) != 0) {
		exit(EXIT_FAILURE);
	}

#ifdef UNICODE
	/* Create a UTF-8 **argv from the wide version */
	static char **argv;
	int wa_err;
	argv = (char **)malloc(sizeof(char *) * (size_t)argc);
	if (!argv) jc_oom("main() unicode argv");
	wa_err = jc_widearg_to_argv(argc, wargv, argv);
	if (wa_err != 0) {
		jc_print_error(wa_err);
		exit(EXIT_FAILURE);
	}
	/* fix up __argv so getopt etc. don't crash */
	__argv = argv;
	jc_set_output_modes(JC_MODE_UTF16_TTY, JC_MODE_UTF16_TTY);
#endif /* UNICODE */

	strncpy(program_name, argv[0], PATH_MAX);

	/* Help text if requested */
	if (argc >= 2) {
		if ((!jc_streq(argv[1], "-h") || !jc_streq(argv[1], "--help"))) {
			usage();
			exit(EXIT_SUCCESS);
		}
		if ((!jc_streq(argv[1], "-v") || !jc_streq(argv[1], "-V") || !jc_streq(argv[1], "--version"))) {
			version();
			exit(EXIT_SUCCESS);
		}
	}

	if (argc != 4) goto error_argcount;

	/* Set up file names to open */
	file1 = argv[1]; file2 = argv[2]; file3 = argv[3];

	/* Open files to merge */
	fp1 = jc_fopen(file1, JC_FILE_MODE_RDONLY_SEQ);
	fp2 = jc_fopen(file2, JC_FILE_MODE_RDONLY_SEQ);
	if (!fp1) goto error_file1;
	if (!fp2) goto error_file2;

	/* File sizes must match; sizes also needed for loop */
	if (jc_stat(argv[1], &stat1) != 0) goto error_file1;
	if (jc_stat(argv[2], &stat2) != 0) goto error_file2;
//	fprintf(stderr, "DEBUG: size %lld,%lld dev %d,%d ino %lld,%lld\n", stat1.st_size, stat2.st_size, stat1.st_dev, stat2.st_dev, stat1.st_ino, stat2.st_ino);
	/* Hard link protection check */
	if (stat1.st_ino == stat2.st_ino && stat1.st_dev == stat2.st_dev) goto error_same_file;
	if (stat1.st_size != stat2.st_size) goto error_file_sizes;
	if (stat1.st_size == 0) goto error_empty_file;
	remain = stat1.st_size;

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
		read1 = (off_t)fread(buf1, 1, READSIZE, fp1);
		read2 = (off_t)fread(buf2, 1, READSIZE, fp2);
		if ((read1 != read2)) goto error_short_read;
		if (ferror(fp1)) goto error_file1;
		if (ferror(fp2)) goto error_file2;
		read1 = 0;

#if 0
#if (defined __GNUC__ || defined __clang__) && (defined USE_AVX2 || defined USE_SSE2)
		if (count >= 32) {
			__builtin_cpu_init ();
#ifdef USE_AVX2
			if (__builtin_cpu_supports ("avx2")) read1 = zero_merge_avx2(read1, read2, buf1, buf2);
#endif
#ifdef USE_SSE2
			else if (__builtin_cpu_supports ("sse2")) read1 = zero_merge_sse2(read1, read2, buf1, buf2);
#endif
		}
#endif
#endif // 0
		/* Merge data between buffers */
		for (; read1 < read2; read1++) {
			/* Error if both bytes are non-zero and different */
			if (buf1[read1] != buf2[read1] && buf1[read1] != 0 && buf2[read1] != 0)
				goto error_different;
			/* merge data into buf1 */
			buf1[read1] |= buf2[read1];
		}
		write = (off_t)fwrite(buf1, 1, (size_t)read2, fp3);
		if (write != read2) goto error_short_write;
		remain -= read2;
		processed += (intmax_t)read2;

		/* Progress indicator - do not show if stdout not a TTY */
		if (hide_progress == 0 && jc_alarm_ring != 0) {
			seconds += jc_alarm_ring;
			jc_alarm_ring = 0;
			progress = stat1.st_size - remain;
			if (stdout_tty == 1) printf("\r");
			printf("[zeromerge] Progress: %" PRIdMAX "%%, %" PRIdMAX " of %" PRIdMAX " %s (%" PRIdMAX " %s/sec)",
					(intmax_t)((progress * 100) / stat1.st_size),
					(intmax_t)progress >> prog_shift,
					(intmax_t)stat1.st_size >> prog_shift,
					prog_suffix,
					(intmax_t)(processed / seconds) >> prog_shift,
					prog_suffix);
			if (stdout_tty == 0) printf("\n");
			fflush(stdout);
		}
		if (feof(fp1) && feof(fp2)) break;
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
	fprintf(stderr, "\nError: short read: file 1 %s, file2 %s\n",
			strerror(ferror(fp1)), strerror(ferror(fp2)));
	exit(EXIT_FAILURE);
error_short_write:
	fprintf(stderr, "\nError: short write (%ld != %ld or %ld): %s\n",
			(long)write, (long)read1, (long)read2, strerror(ferror(fp3)));
	exit(EXIT_FAILURE);
error_empty_file:
	fprintf(stderr, "\nError: cannot merge files that are 0 bytes in size\n");
	exit(EXIT_FAILURE);
}
