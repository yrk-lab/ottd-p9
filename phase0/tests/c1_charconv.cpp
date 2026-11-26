// c1: std::from_chars / std::to_chars on the integer types OpenTTD parses with
// (StringConsumer::ParseIntegerBase): decimal, hex, octal, signed, out of range, junk.
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <system_error>

template <class T>
static void parse(const char *s, int base)
{
	T v{};
	auto r = std::from_chars(s, s + strlen(s), v, base);
	const char *ec = r.ec == std::errc{} ? "ok" : r.ec == std::errc::result_out_of_range ? "range" : "invalid";
	printf("from_chars<%d%s>(\"%s\", %d): %s len %d value %lld\n", (int)sizeof(T) * 8,
		T(-1) < T(0) ? "s" : "u", s, base, ec, (int)(r.ptr - s), (long long)v);
}

int main()
{
	parse<uint32_t>("1234567", 10);
	parse<int32_t>("1234567", 10);
	parse<int32_t>("-42", 10);
	parse<uint64_t>("18446744073709551615", 10);
	parse<uint64_t>("18446744073709551616", 10);
	parse<uint8_t>("255", 10);
	parse<uint8_t>("256", 10);
	parse<int16_t>("-32768", 10);
	parse<uint32_t>("ff", 16);
	parse<uint32_t>("777", 8);
	parse<uint32_t>("12abc", 10);
	parse<uint32_t>("abc", 10);
	parse<int64_t>("-9223372036854775808", 10);

	char buf[64];
	long long vals[] = { 0, 7, -1, 1234567, -9223372036854775807LL - 1, 4294967295LL };
	for (long long v : vals) {
		auto r = std::to_chars(buf, buf + sizeof buf, v);
		*r.ptr = 0;
		printf("to_chars(%lld) = %s\n", v, buf);
	}
	auto r = std::to_chars(buf, buf + sizeof buf, 3735928559u, 16);
	*r.ptr = 0;
	printf("to_chars hex = %s\n", buf);
	return 0;
}
