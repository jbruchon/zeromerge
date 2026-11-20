/* Zero Merge AVX-512 acceleration routine
 * Copyright (C) 2020-2026 by Jody Bruchon <jody@jodybruchon.com>
 * Distributed under The GNU General Public License v2 */

#include <stddef.h>
#include <stdint.h>

#if defined __x86_64__ && defined __AVX512F__ && defined __AVX512BW__ && !defined NO_SIMD && !defined NO_AVX512
#include <immintrin.h>

size_t zero_merge_avx512(size_t pos, size_t length, char *buf1, char *buf2)
{
	const __m512i zero = _mm512_setzero_si512();

	while (pos + 64 <= length) {
		__m512i v1 = _mm512_loadu_si512((const void *)(buf1 + pos));
		__m512i v2 = _mm512_loadu_si512((const void *)(buf2 + pos));

		__mmask64 eq = _mm512_cmpeq_epi8_mask(v1, v2);
		__mmask64 z1 = _mm512_cmpeq_epi8_mask(v1, zero);
		__mmask64 z2 = _mm512_cmpeq_epi8_mask(v2, zero);
		__mmask64 conflict = (~eq) & (~z1) & (~z2);

		if (conflict != 0) {
			for (size_t i = 0; i < 64; i++) {
				if ((conflict >> i) & 1ULL) return pos + i;
			}
		}

		_mm512_storeu_si512((void *)(buf1 + pos), _mm512_or_si512(v1, v2));
		pos += 64;
	}

	while (pos < length) {
		if (buf1[pos] != buf2[pos] && buf1[pos] != 0 && buf2[pos] != 0) return pos;
		buf1[pos] |= buf2[pos];
		pos++;
	}

	return length;
}
#else
size_t zero_merge_avx512(size_t pos, size_t length, char *buf1, char *buf2)
{
	while (pos < length) {
		if (buf1[pos] != buf2[pos] && buf1[pos] != 0 && buf2[pos] != 0) return pos;
		buf1[pos] |= buf2[pos];
		pos++;
	}
	return length;
}
#endif
