
#if (defined(__GNUC__)  || defined(__GCCXML__)) && !defined(__WIN32)
#include <string.h>
int _stricmp(const char* s1, const char* s2)
{
	return strcasecmp(s1,s2);
}
int _strnicmp(const char* s1, const char* s2, size_t n)
{
	return strncasecmp(s1,s2,n);
}
#endif
