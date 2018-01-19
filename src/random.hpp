#pragma once
#include <cstdint>

class XORShift128 {

private:
	uint32_t x = 192479812u;
	uint32_t y = 784892731u;
	uint32_t z = 427398108u;
	uint32_t w =  48382934u;

public:
	void set_seed(uint32_t s){
		x = y = z = w = s;
		(*this)();
	}

	uint32_t operator()(){
		const uint32_t t = x ^ (x << 11);
		x = y; y = z; z = w;
		w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
		return w;
	}
};

static XORShift128 g_rng;

void set_seed(uint32_t s){
	g_rng.set_seed(s);
}

inline uint32_t xorshift128(){
	return g_rng();
}

inline uint32_t modulus_random(uint32_t mod){
	const auto t = static_cast<uint64_t>(xorshift128()) * mod;
	return static_cast<uint32_t>(t >> 32);
}
