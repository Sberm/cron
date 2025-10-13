#define pr_err(...) fprintf(stderr, ##__VA_ARGS__)

#ifdef DEBUG
#define pr_debug(...) fprintf(stdout, ##__VA_ARGS__)
#else
#define pr_debug(...)
#endif

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define min(a,b)                \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b);    \
    _a < _b ? _a : _b; })
