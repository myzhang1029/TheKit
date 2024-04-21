#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>

#if !defined(NDEBUG) || defined(THEKIT_DEBUG)
#define LOG_DEBUG(...) (printf)(__VA_ARGS__)
#define LOG_DEBUG1(str) (puts)((str))
#else
#define LOG_DEBUG(...)
#define LOG_DEBUG1(str)
#endif

#define LOG_INFO(...) (printf)(__VA_ARGS__)
#define LOG_INFO1(str) (puts)((str))

#define LOG_WARN(...) (printf)("WARNING: "__VA_ARGS__)
#define LOG_WARN1(str) (puts)(("WARNING: " str))

#define LOG_ERR(...) (printf)("ERROR: " __VA_ARGS__)
#define LOG_ERR1(str) (puts)(("ERROR: " str))

#define puts(_) error("puts is not allowed")
#define printf(...) error("printf is not allowed")

#endif
