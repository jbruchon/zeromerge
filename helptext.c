/* zeromerge help text
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
#include "helptext.h"
#include "version.h"


static char program_name[PATH_MAX + 4];

void set_program_name(const char * const name)
{
	snprintf(program_name, sizeof(program_name), "%s", name);
	return;
}

void version(void)
{
	printf("zeromerge %s (%s) by Jody Bruchon <jody@jodybruchon.com>\n", VER, VERDATE);
	printf("Linked to libjodycode %s (%s)\n", LIBJODYCODE_VER, LIBJODYCODE_VERDATE);
	printf("Distributed under the GNU General Public License v2\n");
	printf("Latest versions and support:\n");
	printf("        https://codeberg.org/jbruchon/zeromerge\n");
	printf("Please consider supporting continued development with funding options at:\n");
	printf("        https://www.jodybruchon.com/\n");
	return;
}


void usage(void)
{
	version();
	printf("Usage:\n");
	printf("  %s [-C ##] [-q] file1 file2 outfile\n", program_name);
	printf("  %s [-C ##] [-q] [-s SIZE[suffix]] -r [directories/files...]\n", program_name);
	printf("\nOptions:\n");
	printf("  -r             Recursively scan paths, group files by size, merge matches, and link/copy\n");
	printf("  -s SIZE[suffix] Minimum file size to consider in -r mode; supports K/M/G/T[i]B suffixes\n");
	printf("  -C ##          Set I/O chunk size index (0 = 4K, doubles up to 18 = 1GB; default 12 = 16MB)\n");
	printf("  -q             Suppress all progress and completion output\n");
	printf("  -h, --help     Display this help and exit\n");
	printf("  -v, --version  Output version information and exit\n");
	return;
}
