#include <cstdio>

#define ASSERT_EQ(a,b) { if (!(a == b)) { printf("Failed assertion: " #a " == " #b " \n  in %s:%d\n", __FILE__, __LINE__); return 1; }}
#define ASSERT_NE(a,b) { if (!(a != b)) { printf("Failed assertion: " #a " != " #b " \n  in %s:%d\n", __FILE__, __LINE__); return 1; }}