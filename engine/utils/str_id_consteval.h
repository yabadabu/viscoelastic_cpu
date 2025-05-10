#pragma once

// Code adapted from https://github.com/aappleby/smhasher

//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// Note - The x86 and x64 versions do _not_ produce the same results, as the
// algorithms are optimized for their respective platforms. You can still
// compile and run any of them on any platform, but your performance with the
// non-native version will be less than optimal.

using uint32_t = unsigned int;

consteval uint32_t strID(const char* key) {

	auto strLength = [](const char* str) {
		int length = 0;
		while (*str != '\0') {
			++length;
			++str;
		}
		return length;
		};

	auto getblock32 = [](const char* p, int i) -> uint32_t {
		return (p[i * 4])
			| (p[i * 4 + 1] << 8)
			| (p[i * 4 + 2] << 16)
			| (p[i * 4 + 3] << 24)
			;
		};

	auto rotl32 = [](uint32_t x, int r) -> uint32_t {
		return (x << r) | (x >> (32 - r));
		};

	auto fmix32 = [](uint32_t h) -> uint32_t {
		h ^= h >> 16;
		h *= 0x85ebca6b;
		h ^= h >> 13;
		h *= 0xc2b2ae35;
		h ^= h >> 16;
		return h;
		};

	const int len = strLength(key);
	const int nblocks = len / 4;
	const char* data = (key);
	uint32_t seed = 0;
	uint32_t h1 = seed;

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	//----------
	// body

	const char* blocks = (data + nblocks * 4);

	for (int i = -nblocks; i; i++)
	{
		uint32_t k1 = getblock32(blocks, i);

		k1 *= c1;
		k1 = rotl32(k1, 15);
		k1 *= c2;

		h1 ^= k1;
		h1 = rotl32(h1, 13);
		h1 = h1 * 5 + 0xe6546b64;
	}

	//----------
	// tail
	const char* tail = blocks;

	uint32_t k1 = 0;

	switch (len & 3)
	{
	case 3: k1 ^= tail[2] << 16;
	case 2: k1 ^= tail[1] << 8;
	case 1: k1 ^= tail[0];
		k1 *= c1; k1 = rotl32(k1, 15); k1 *= c2; h1 ^= k1;
	};

	//----------
	// finalization

	h1 ^= len;

	h1 = fmix32(h1);

	return h1;
}
