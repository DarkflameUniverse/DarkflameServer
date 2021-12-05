#include <assert.h>

// So stupid Linux doesn't assert in release
#ifdef _DEBUG
#define RakAssert(x) assert(x);
#else
#define RakAssert(x) 
#endif
