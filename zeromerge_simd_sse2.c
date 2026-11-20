/* Zero Merge SSE2 acceleration routine
 * Copyright (C) 2020-2026 by Jody Bruchon <jody@jodybruchon.com>
 * Distributed under The GNU General Public License v2 */

#include <stddef.h>
#include <stdint.h>

#if defined __x86_64__ && defined __SSE2__ && !defined NO_SIMD && !defined NO_SSE2
#include <emmintrin.h>

size_t zero_merge_sse2(size_t pos, size_t length, char *buf1, char *buf2)
{
	const __m128i zero = _mm_setzero_si128();

	while (pos + 16 <= length) {
		__m128i v1 = _mm_loadu_si128((const __m128i *)(const void *)(buf1 + pos));
		__m128i v2 = _mm_loadu_si128((const __m128i *)(const void *)(buf2 + pos));

		__m128i neq = _mm_xor_si128(_mm_cmpeq_epi8(v1, v2), _mm_cmpeq_epi8(zero, zero));
		__m128i nz1 = _mm_xor_si128(_mm_cmpeq_epi8(v1, zero), _mm_cmpeq_epi8(zero, zero));
		__m128i nz2 = _mm_xor_si128(_mm_cmpeq_epi8(v2, zero), _mm_cmpeq_epi8(zero, zero));
		__m128i conflict = _mm_and_si128(neq, _mm_and_si128(nz1, nz2));

		int mask = _mm_movemask_epi8(conflict);
		if (mask != 0) {
			for (size_t i = 0; i < 16; i++) {
				if ((mask >> i) & 1) return pos + i;
			}
		}

		_mm_storeu_si128((__m128i *)(void *)(buf1 + pos), _mm_or_si128(v1, v2));
		pos += 16;
	}

	while (pos < length) {
		if (buf1[pos] != buf2[pos] && buf1[pos] != 0 && buf2[pos] != 0) return pos;
		buf1[pos] |= buf2[pos];
		pos++;
	}

	return length;
}
#else
size_t zero_merge_sse2(size_t pos, size_t length, char *buf1, char *buf2)
{
	while (pos < length) {
		if (buf1[pos] != buf2[pos] && buf1[pos] != 0 && buf2[pos] != 0) return pos;
		buf1[pos] |= buf2[pos];
		pos++;
	}
	return length;
}
#endif
