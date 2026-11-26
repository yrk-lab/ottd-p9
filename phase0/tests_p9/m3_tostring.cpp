#include <cstdio>
#include <string>
int main() { for (int i : {0, 7, 12, 255, 12345, -9}) printf("m3 [%s]\n", std::to_string(i).c_str()); }
