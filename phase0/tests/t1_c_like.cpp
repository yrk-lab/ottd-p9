// Tier 1: C-like code. Should pass trivially; proves the pipe itself works.
#include <stdio.h>
#include <stdint.h>
struct P { int32_t x, y; };
static uint32_t h(uint32_t a, uint32_t b) { return (a ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2))); }
int main() {
	P ps[16]; uint32_t acc = 1;
	for (int i = 0; i < 16; i++) { ps[i] = P{i * 3, i * i}; acc = h(acc, (uint32_t)(ps[i].x * 31 + ps[i].y)); }
	printf("t1 %u\n", acc);
	return 0;
}
