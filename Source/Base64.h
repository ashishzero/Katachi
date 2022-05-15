#pragma once
#include <stdint.h>

static constexpr uint8_t Base64Lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static constexpr uint8_t InverseBase64Lookup[] = {
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62, 255, 255, 255, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255, 255, 255, 255, 255, 255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
	13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 255, 255, 26, 27, 28, 29, 30, 31, 32, 33, 34,
	35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255
};

constexpr size_t Base64EncodedSize(size_t size) { ptrdiff_t base64_len = (4 * size) / 3; return AlignPower2Up(base64_len, 4); }
constexpr size_t Based64DecodedSize(size_t size) { return (3 * size) / 4; }

static inline ptrdiff_t EncodeBase64(Buffer input, uint8_t *base64, ptrdiff_t length) {
	ptrdiff_t base64_len = Base64EncodedSize(input.length);
	Assert(base64_len <= length);

	int val       = 0;
	int valb      = -6;
	ptrdiff_t pos = 0;

	for (ptrdiff_t index = 0; index < input.length; ++index) {
		uint8_t c = input[index];
		val = (val << 8) + static_cast<uint8_t>(c);
		valb += 8;
		while (valb >= 0) {
			base64[pos++] = Base64Lookup[(val >> valb) & 0x3F];
			valb -= 6;
		}
	}

	if (valb > -6)
		base64[pos++] = Base64Lookup[((val << 8) >> (valb + 8)) & 0x3F];

	for (; pos < base64_len; ++pos)
		base64[pos] = '=';

	return base64_len;
}

static inline ptrdiff_t DecodeBase64(String base64, uint8_t *buffer, ptrdiff_t length) {
	while (base64[base64.length - 1] == '=')
		base64.length -= 1;

	ptrdiff_t decoded_len = Based64DecodedSize(base64.length);

	Assert(decoded_len <= length);

	uint8_t positions[4];
	ptrdiff_t pos = 0;
	ptrdiff_t index = 0;

	for (; index < base64.length - 4; index += 4) {
		positions[0] = InverseBase64Lookup[base64[index + 0]];
		positions[1] = InverseBase64Lookup[base64[index + 1]];
		positions[2] = InverseBase64Lookup[base64[index + 2]];
		positions[3] = InverseBase64Lookup[base64[index + 3]];

		buffer[pos++] = (positions[0] << 2) + ((positions[1] & 0x30) >> 4);
		buffer[pos++] = ((positions[1] & 0xf) << 4) + ((positions[2] & 0x3c) >> 2);
		buffer[pos++] = ((positions[2] & 0x3) << 6) + positions[3];
	}

	ptrdiff_t it = 0;
	for (; index < base64.length; ++index, ++it)
		positions[it] = InverseBase64Lookup[base64[index]];
	for (; it < 4; ++it)
		positions[it] = 0;

	uint8_t values[3];
	values[0] = (positions[0] << 2) + ((positions[1] & 0x30) >> 4);
	values[1] = ((positions[1] & 0xf) << 4) + ((positions[2] & 0x3c) >> 2);
	values[2] = ((positions[2] & 0x3) << 6) + positions[3];

	for (it = 0; pos < decoded_len; ++pos, ++it)
		buffer[pos] = values[it];

	return decoded_len;
}
