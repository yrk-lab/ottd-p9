#include <cstdio>
#include <vector>
#include <string>
struct Item { std::string name; int w; };
int main() { std::vector<Item> v; printf("m5 sizeof(Item)=%u sizeof(string)=%u\n", (unsigned)sizeof(Item), (unsigned)sizeof(std::string));
  for (int i = 0; i < 6; i++) { v.push_back({"x", i}); printf("m5 after push %d: size=%u cap=%u\n", i, (unsigned)v.size(), (unsigned)v.capacity()); } }
