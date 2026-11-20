/* Jody Bruchon's SIMD configuration headers
 * See zeromerge.c for license information */

#ifndef ZEROMERGE_SIMD_H
#define ZEROMERGE_SIMD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

enum zeromerge_simd_mode {
	ZM_SIMD_NONE = 0,
	ZM_SIMD_SSE2 = 1,
	ZM_SIMD_AVX2 = 2,
	ZM_SIMD_AVX512 = 3
};

#if defined __x86_64__ || defined __i386__
 #define ZEROMERGE_ARCH_X86 1
#else
 #define ZEROMERGE_ARCH_X86 0
#endif

#if defined NO_SIMD || !ZEROMERGE_ARCH_X86 || (defined NO_SSE2 && defined NO_AVX2 && defined NO_AVX512)
 #undef USE_SSE2
 #undef USE_AVX2
 #undef USE_AVX512
 #ifndef NO_SIMD
  #define NO_SIMD
 #endif
#endif

#if !defined NO_SIMD && ZEROMERGE_ARCH_X86
 #if !defined NO_SSE2
  #define USE_SSE2
 #endif
 #if !defined NO_AVX2
  #define USE_AVX2
 #endif
 #if !defined NO_AVX512
  #define USE_AVX512
 #endif
 #if defined _MSC_VER || defined _WIN32 || defined __MINGW32__
  #include <intrin.h>
  #define ZM_ALIGNED_ALLOC(align, size) _aligned_malloc((size), (align))
  #define ZM_ALIGNED_FREE(ptr) _aligned_free(ptr)
 #elif defined __GNUC__ || defined __clang__
  #include <x86intrin.h>
  #define ZM_ALIGNED_ALLOC(align, size) malloc(size)
  #define ZM_ALIGNED_FREE(ptr) free(ptr)
 #endif
#endif

#ifndef ZM_ALIGNED_ALLOC
 #define ZM_ALIGNED_ALLOC(align, size) malloc(size)
#endif
#ifndef ZM_ALIGNED_FREE
 #define ZM_ALIGNED_FREE(ptr) free(ptr)
#endif

#if !defined NO_SIMD && ZEROMERGE_ARCH_X86
extern int zeromerge_detect_simd(void);
extern int zeromerge_merge_bufs(int simd_mode, size_t length, char *buf1, char *buf2);
extern size_t zero_merge_sse2(size_t pos, size_t length, char *buf1, char *buf2);
extern size_t zero_merge_avx2(size_t pos, size_t length, char *buf1, char *buf2);
extern size_t zero_merge_avx512(size_t pos, size_t length, char *buf1, char *buf2);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZEROMERGE_SIMD_H */
