// Tier 3: the STL surface OpenTTD actually leans on.
#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <unordered_map>
#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <span>
#include <numeric>

struct Item {
	std::string name;
	int w;
};
static int sum(std::span<const int> s)
{
	return std::accumulate(s.begin(), s.end(), 0);
}
int main()
{
	std::vector<Item> v;
	for (int i = 0; i < 20; i++) v.push_back({ "item" + std::to_string(i * 7 % 13), (i * 37) % 11 });
	std::sort(v.begin(), v.end(),
	          [](const Item &a, const Item &b) { return a.w != b.w ? a.w < b.w : a.name < b.name; });
	std::unordered_map<std::string, int> um;
	for (auto &it : v) um[it.name] += it.w;
	std::map<std::string, int> om(um.begin(), um.end());
	unsigned h = 0;
	for (auto &[k, val] : om)
		for (char c : k) h = h * 131 + (unsigned char)c + val;
	std::array<int, 5> arr = { 5, 4, 3, 2, 1 };
	std::optional<int> o = arr[2] > 2 ? std::optional<int>(arr[0]) : std::nullopt;
	std::variant<int, std::string> var = std::string("variant");
	auto up = std::make_unique<Item>(Item { "uniq", 9 });
	std::string_view sv = up->name;
	printf("t3 %u n=%u sum=%d opt=%d var=%u sv=%u\n", h, (unsigned)v.size(), sum(arr), o.value_or(-1),
	       (unsigned)std::get<std::string>(var).size(), (unsigned)sv.size());
	return 0;
}
