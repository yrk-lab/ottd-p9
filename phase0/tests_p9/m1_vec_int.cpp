#include <cstdio>
#include <vector>
int main()
{
	std::vector<int> v;
	for (int i = 0; i < 20; i++) v.push_back(i * 3);
	unsigned s = 0;
	for (int x : v) s += x;
	printf("m1 n=%u cap>=%d sum=%u\n", (unsigned)v.size(), (int)(v.capacity() >= 20), s);
}
