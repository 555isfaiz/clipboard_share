#ifndef __UTILS__
#define __UTILS__

#include <stdint.h>
#include <stdio.h>
#include <time.h>

extern uint8_t is_verbose;

#define info(fmt, ...)                                                                                                 \
    {                                                                                                                  \
        char b[32];                                                                                                    \
        struct tm *sTm;                                                                                                \
                                                                                                                       \
        time_t now = time(0);                                                                                          \
        sTm = gmtime(&now);                                                                                            \
                                                                                                                       \
        strftime(b, sizeof(b), "[%Y-%m-%d %H:%M:%S]", sTm);                                                            \
        fprintf(stdout, "%s[info][%s:%d]: ", b, __FILE__, __LINE__);                                                   \
        fprintf(stdout, fmt, ##__VA_ARGS__);                                                                           \
    }

#define debug(fmt, ...)                                                                                                \
    {                                                                                                                  \
        if (is_verbose)                                                                                                \
        {                                                                                                              \
            char b[32];                                                                                                \
            struct tm *sTm;                                                                                            \
                                                                                                                       \
            time_t now = time(0);                                                                                      \
            sTm = gmtime(&now);                                                                                        \
                                                                                                                       \
            strftime(b, sizeof(b), "[%Y-%m-%d %H:%M:%S]", sTm);                                                        \
            fprintf(stdout, "%s[debug][%s:%d]: ", b, __FILE__, __LINE__);                                              \
            fprintf(stdout, fmt, ##__VA_ARGS__);                                                                       \
        }                                                                                                              \
    }

#define error(fmt, ...)                                                                                                \
    {                                                                                                                  \
        char b[32];                                                                                                    \
        struct tm *sTm;                                                                                                \
                                                                                                                       \
        time_t now = time(0);                                                                                          \
        sTm = gmtime(&now);                                                                                            \
                                                                                                                       \
        strftime(b, sizeof(b), "[%Y-%m-%d %H:%M:%S]", sTm);                                                            \
        fprintf(stderr, "%s[error][%s:%d]: ", b, __FILE__, __LINE__);                                                  \
        fprintf(stderr, fmt, ##__VA_ARGS__);                                                                           \
    }

#endif
