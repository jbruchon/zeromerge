/* Zero Merge runtime SIMD detection and merge dispatcher
 * Copyright (C) 2020-2026 by Jody Bruchon <jody@jodybruchon.com>
 * Distributed under The GNU General Public License v2 */

#include <stddef.h>
#include <stdint.h>
#include "zeromerge_simd.h"

#if !defined NO_SIMD && ZEROMERGE_ARCH_X86

#if defined __GNUC__ || defined __clang__
#include <cpuid.h>
#endif

int zeromerge_detect_simd(void)
{
#if defined __GNUC__ || defined __clang__
	unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;

	if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) return ZM_SIMD_NONE;

	int has_sse2 = (edx & (1U << 26)) != 0;
	int has_osxsave = (ecx & (1U << 27)) != 0;

	if (!has_osxsave) {
#if defined USE_SSE2
		if (has_sse2) return ZM_SIMD_SSE2;
#endif
		return ZM_SIMD_NONE;
	}

	unsigned int xcr0_lo = 0, xcr0_hi = 0;
	__asm__ __volatile__(".byte 0x0f, 0x01, 0xd0"
			: "=a"(xcr0_lo), "=d"(xcr0_hi)
			: "c"(0));
	int ymm_enabled = ((xcr0_lo & 0x06U) == 0x06U);
	int zmm_enabled = ((xcr0_lo & 0xe6U) == 0xe6U);

	unsigned int eax7 = 0, ebx7 = 0, ecx7 = 0, edx7 = 0;
	int have_leaf7 = __get_cpuid_count(7, 0, &eax7, &ebx7, &ecx7, &edx7);

#if defined USE_AVX512
	if (have_leaf7 && zmm_enabled && ((ebx7 & (1U << 16)) != 0) && ((ecx7 & (1U << 30)) != 0)) return ZM_SIMD_AVX512;
#endif
#if defined USE_AVX2
	if (have_leaf7 && ymm_enabled && ((ebx7 & (1U << 5)) != 0)) return ZM_SIMD_AVX2;
#endif
#if defined USE_SSE2
	if (has_sse2) return ZM_SIMD_SSE2;
#endif
#endif
	return ZM_SIMD_NONE;
}

int zeromerge_merge_bufs(int simd_mode, size_t length, char *buf1, char *buf2)
{
	size_t done = 0;

#if defined USE_AVX512
	if (simd_mode == ZM_SIMD_AVX512) done = zero_merge_avx512(0, length, buf1, buf2);
	else
#endif
#if defined USE_AVX2
	if (simd_mode == ZM_SIMD_AVX2) done = zero_merge_avx2(0, length, buf1, buf2);
	else
#endif
#if defined USE_SSE2
	if (simd_mode == ZM_SIMD_SSE2) done = zero_merge_sse2(0, length, buf1, buf2);
	else
#endif
	{
		for (size_t idx = 0; idx < length; idx++) {
			if (buf1[idx] != buf2[idx] && buf1[idx] != 0 && buf2[idx] != 0) return -2;
			buf1[idx] |= buf2[idx];
		}
		return 0;
	}

	return (done == length) ? 0 : -2;
}

#endif /* !NO_SIMD && ZEROMERGE_ARCH_X86 */
