/* Zero Merge AVX2 acceleration routine
 * Copyright (C) 2020-2026 by Jody Bruchon <jody@jodybruchon.com>
 * Distributed under The GNU General Public License v2 */

#include <stddef.h>
#include <stdint.h>

#if defined __x86_64__ && defined __AVX2__ && !defined NO_SIMD && !defined NO_AVX2
#include <immintrin.h>

size_t zero_merge_avx2(size_t pos, size_t length, char *buf1, char *buf2)
{
	const __m256i zero = _mm256_setzero_si256();

	while (pos + 32 <= length) {
		__m256i v1 = _mm256_loadu_si256((const __m256i *)(const void *)(buf1 + pos));
		__m256i v2 = _mm256_loadu_si256((const __m256i *)(const void *)(buf2 + pos));

		__m256i neq = _mm256_xor_si256(_mm256_cmpeq_epi8(v1, v2), _mm256_cmpeq_epi8(zero, zero));
		__m256i nz1 = _mm256_xor_si256(_mm256_cmpeq_epi8(v1, zero), _mm256_cmpeq_epi8(zero, zero));
		__m256i nz2 = _mm256_xor_si256(_mm256_cmpeq_epi8(v2, zero), _mm256_cmpeq_epi8(zero, zero));
		__m256i conflict = _mm256_and_si256(neq, _mm256_and_si256(nz1, nz2));

		int mask = _mm256_movemask_epi8(conflict);
		if (mask != 0) {
			uint32_t umask = (uint32_t)mask;
			for (size_t i = 0; i < 32; i++) {
				if ((umask >> i) & 1U) return pos + i;
			}
		}

		_mm256_storeu_si256((__m256i *)(void *)(buf1 + pos), _mm256_or_si256(v1, v2));
		pos += 32;
	}

	while (pos < length) {
		if (buf1[pos] != buf2[pos] && buf1[pos] != 0 && buf2[pos] != 0) return pos;
		buf1[pos] |= buf2[pos];
		pos++;
	}

	return length;
}
#else
size_t zero_merge_avx2(size_t pos, size_t length, char *buf1, char *buf2)
{
	while (pos < length) {
		if (buf1[pos] != buf2[pos] && buf1[pos] != 0 && buf2[pos] != 0) return pos;
		buf1[pos] |= buf2[pos];
		pos++;
	}
	return length;
}
#endif
