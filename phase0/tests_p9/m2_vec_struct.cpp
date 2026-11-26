#include <cstdio>
#include <vector>
#include <string>
struct Item {
	std::string name;
	int w;
};
int main()
{
	std::vector<Item> v;
	for (int i = 0; i < 20; i++) v.push_back({ "item" + std::to_string(i), i });
	unsigned s = 0;
	for (auto &it : v) s += it.w + (unsigned)it.name.size();
	printf("m2 n=%u s=%u\n", (unsigned)v.size(), s);
}
