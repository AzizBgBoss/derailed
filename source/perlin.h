// perlin.h
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

uint32_t murmur_32_scramble(uint32_t k);
uint32_t murmur3_32(const uint8_t *key, size_t len, uint32_t seed);
int hash(uint32_t x, int seed);
int hash2d(int x, int y, int seed);
float lerp(float a, float b, float t);
float fade(float t);
float grad(int hash, float x);
float perlin1d(float x, int seed);
float grad2D(int hash, float x, float y);
float perlin2d(float x, float y, int seed);
float fractalPerlin1D(float x, int octaves, float persistence, float scale, int seed);
float fractalPerlin2D(float x, float y, int octaves, float persistence, float scale, int seed);