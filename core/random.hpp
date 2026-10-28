#pragma once
#include <cassert>
#include <cstdint>
#include <random>

namespace komaAHC {
struct Random {
	std::mt19937_64 engine;

	explicit Random(std::uint64_t seed) : engine(seed) {}

	int next_int(int n) {
		assert(n > 0);
		return std::uniform_int_distribution<int>(0, n - 1)(engine);
	}

	double next_double() {
		return std::generate_canonical<double, 53>(engine); // [0, 1)
	}
};

// xorshift64*: 軽量で再現可能。暗号用途には使わない。
struct FastRandom {
	std::uint64_t state;

	explicit FastRandom(std::uint64_t seed = 1) {
		// SplitMix64 で seed を混ぜ、xorshift の禁止状態である 0 を避ける。
		std::uint64_t x = seed + 0x9e3779b97f4a7c15ULL;
		x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
		x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
		state = x ^ (x >> 31);
		if(state == 0) state = 0x9e3779b97f4a7c15ULL;
	}

	std::uint64_t next_u64() {
		state ^= state >> 12;
		state ^= state << 25;
		state ^= state >> 27;
		return state * 0x2545f4914f6cdd1dULL;
	}

	int next_int(int n) {
		assert(n > 0);
		const auto bound = static_cast<std::uint32_t>(n);
		// 棄却法で余りによる偏りを除く。通常は1回で決まる。
		const std::uint32_t threshold = -bound % bound;
		for(;;) {
			const auto value = static_cast<std::uint32_t>(next_u64() >> 32);
			const std::uint64_t product = static_cast<std::uint64_t>(value) * bound;
			if(static_cast<std::uint32_t>(product) >= threshold) return static_cast<int>(product >> 32);
		}
	}

	double next_double() {
		return (next_u64() >> 11) * 0x1.0p-53; // [0, 1)
	}
};

} // namespace komaAHC
