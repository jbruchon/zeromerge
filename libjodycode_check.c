/* libjodycode version checks
 *
 * Code to embed the libjodycode version info and check against the currently
 * linked libjodycode to check for and report incompatibilities
 * Can also be compiled with -DSTANDALONE for version checking purposes
 *
 * Copyright (C) 2023-2026 by Jody Bruchon <jody@jodybruchon.com>
 * Licensed under The MIT License */

#include <stdio.h>
#include <stdlib.h>
#include <libjodycode.h>
#include "libjodycode_check.h"

/* Set your required feature level in libjodycode_check.h */
const int jc_build_min_featurelevel = MY_FEATURELEVEL_REQ;
#ifndef LIBJODYCODE_API_VERSION
 #error Your libjodycode version cannot be detected or is ancient.
 #error Compilation will fail. Update libjodycode and try again.
#endif

#if LIBJODYCODE_API_VERSION < 3
#define LIBJODYCODE_API_FEATURE_LEVEL LIBJODYCODE_API_VERSION
#define jc_api_featurelevel LIBJODYCODE_API_VERSION
#endif

/* libjodycode < 3 is missing some Unicode stuff */
#ifndef LIBJODYCODE_WINDOWS_UNICODE
 #if PATHBUF_SIZE == 8192
  /* PATHBUF_SIZE can be used as a proxy for Windows Unicode */
  #define LIBJODYCODE_WINDOWS_UNICODE 1
  #define jc_windows_unicode 1
 #elif PATHBUF_SIZE == 4096
  #define LIBJODYCODE_WINDOWS_UNICODE 0
  #define jc_windows_unicode 0
 #endif
#endif

const char *jc_build_version = LIBJODYCODE_VER;
const int jc_build_api_version = LIBJODYCODE_API_VERSION;
const int jc_build_api_featurelevel = LIBJODYCODE_API_FEATURE_LEVEL;
#ifdef LIBJODYCODE_WINDOWS_UNICODE
const int jc_build_windows_unicode = LIBJODYCODE_WINDOWS_UNICODE;
#else
 #define jc_build_windows_unicode 1
#endif

#ifdef JC_TEST
 #define JC_TEST_ONLY(a) a
#else
 #define JC_TEST_ONLY(a)
#endif


int libjodycode_version_check(int verbose, int bail)
{
	JC_TEST_ONLY(if (verbose > 1) fprintf(stderr, "libjodycode version check test code\n\n");)
	JC_TEST_ONLY(if (verbose > 1) goto incompatible_version;)
	if (jc_build_api_version != jc_api_version) goto incompatible_version;
	if (jc_build_min_featurelevel > jc_api_featurelevel) goto incompatible_version;
#ifdef LIBJODYCODE_WINDOWS_UNICODE
	if (jc_build_windows_unicode != jc_windows_unicode) goto incompatible_version;
#endif
	return 0;

incompatible_version:
	if (verbose) {
		fprintf(stderr, "\n==============================================================================\n");
		fprintf(stderr,   "internal error: libjodycode on this system is an incompatible version\n\n");
		fprintf(stderr, "Currently using libjodycode v%s, API %d, feature level %d\n",
				jc_version, jc_api_version, jc_api_featurelevel);
		fprintf(stderr, "  Built against libjodycode v%s, API %d, feature level %d\n\n",
				jc_build_version, jc_build_api_version, jc_build_api_featurelevel);
		if (jc_windows_unicode != jc_build_windows_unicode)
			fprintf(stderr, "libjodycode was built with%s Windows Unicode but %sUnicode is required.\n\n",
					jc_windows_unicode == 1 ? "" : "out",
					jc_build_windows_unicode == 1 ? "" : "non-");
		if (jc_build_min_featurelevel > jc_build_api_featurelevel)
			fprintf(stderr, "libjodycode feature level >= %d is required but linked library is level %d\n\n",
				jc_build_min_featurelevel, jc_build_api_featurelevel);
		fprintf(stderr, "==============================================================================\n\n");
		fprintf(stderr, "\nUpdate libjodycode on your system and try again. If you continue to get this\n");
		fprintf(stderr, "error, contact the package or distribution maintainer. If all else fails, send\n");
		fprintf(stderr, "an email to jody@jodybruchon.com for help (but only as a last resort, please.)\n\n");
	}
	if (bail) exit(EXIT_FAILURE);
	return 1;
}


#if defined (JC_TEST) || defined (STANDALONE)
 #ifdef UNICODE
int wmain(void)
 #else
int main(void)
 #endif
{
#ifdef JC_TEST
	libjodycode_version_check(2, 0);
#else
	printf("libjodycode ver,api,featurelevel,reqlevel:%s:%d:%d:%d\n",
			jc_build_version, jc_build_api_version,
			jc_build_api_featurelevel, jc_build_min_featurelevel);
#endif
	return 0;
}
#endif
