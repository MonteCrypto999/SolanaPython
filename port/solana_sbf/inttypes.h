#ifndef _INTTYPES_H_
#define _INTTYPES_H_

#include <stdint.h>

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#ifndef PRId64
#define PRId64 "lld"
#endif

#ifndef PRIx64
#define PRIx64 "llx"
#endif

#ifndef PRIX64
#define PRIX64 "llX"
#endif

#ifndef PRIu32
#define PRIu32 "u"
#endif

#ifndef PRId32
#define PRId32 "d"
#endif

#ifndef PRIx32
#define PRIx32 "x"
#endif

#ifndef PRIX32
#define PRIX32 "X"
#endif

#ifndef PRIuPTR
#define PRIuPTR "lu"
#endif

#ifndef PRIxPTR
#define PRIxPTR "lx"
#endif

#endif

