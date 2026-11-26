// b1: 64-bit shifts of constants by run-time amounts: the type of a C shift is
// that of its left operand alone, so a constant must not be printed as a plain
// int.  OpenTTD's character-set tests are of this form ((1 << c) & mask, for c < 64).
// Also llvm.abs (clang's form of x < 0 ? -x : x), whose operand must be signed.
#include <cstdint>
#include <cstdio>

static volatile int sink;

static bool is_digit_mask(unsigned char c)
{
	return c < 64 && ((uint64_t { 1 } << c) & 0x03FF000000000000ULL) != 0;
}

static bool in_set(const char *set, char c)
{
	for (; *set != 0; set++)
		if (*set == c) return true;
	return false;
}

int main()
{
	int digits = 0;
	for (int c = 0; c < 128; c++) digits += is_digit_mask((unsigned char)c);
	printf("digits by mask %d\n", digits);

	int n = 0;
	const char *s = "0123456789";
	for (int c = 0; c < 128; c++) n += in_set(s, (char)c);
	printf("digits by search %d\n", n);

	for (int k = 0; k < 64; k += 7) {
		volatile int kk = k;
		uint64_t a = uint64_t { 1 } << kk;
		int64_t b = int64_t { -1 } << kk;
		int64_t c = INT64_MIN >> kk;
		uint64_t d = 0x8000000000000000ULL >> kk;
		printf("k %2d: %016llx %016llx %016llx %016llx\n", k, (unsigned long long)a, (unsigned long long)b,
		       (unsigned long long)c, (unsigned long long)d);
	}
	volatile long long in[] = { -5, 7, 0, -2147483647LL, -9223372036854775807LL, 123456789012LL };
	for (long long v : in) {
		int i = (int)v;
		short h = (short)v;
		printf("abs %lld: %d %d %lld\n", v, i < 0 ? -i : i, h < 0 ? -h : h, v < 0 ? -v : v);
	}
	sink = digits;
	return 0;
}
