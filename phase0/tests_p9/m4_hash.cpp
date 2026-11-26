#include <cstdio>
#include <string>
#include <functional>
int main() { std::hash<std::string> h; std::string a = "item1", b = "a much longer string that exceeds sixteen bytes in length ok";
  printf("m4 %u %u\n", (unsigned)(h(a) & 0xffffffffu), (unsigned)(h(b) & 0xffffffffu)); }
