#ifndef MYASSERT_DEF__
#define MYASSERT_DEF__

#define ASSERT(cond) assert((cond))

#define MASSERT(cond,message) do {\
    if (!(cond)) { fprintf(stderr, message); exit(1); } \
    while (0)

#define FASSERT(cond,format,...) do {\
    if (!(cond)) { fprintf(stderr, format, __VA_LIST__); exit(1); } \
    while (0)

#endif /* MYASSERT_DEF__ */
