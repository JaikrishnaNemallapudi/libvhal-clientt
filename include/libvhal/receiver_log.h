#ifndef _RECEIVER_LOG_H
#define _RECEIVER_LOG_H

#include <cstring>
#include <pthread.h>
#include <stdio.h>
#include <string>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __android__
#include <android/log.h>
#endif

#define INFO 1
#define DEBUG 2
#define WARNING 3
#define ERROR 4

#ifndef __android__
#define filename(x) strrchr(x, '/') ? strrchr(x, '/') + 1 : x
#define AIC_LOG(level, fmt, ...)                                               \
    do {                                                                       \
        if (level > INFO) {                                                    \
            printf(                                                            \
              "%s %ld %s(%d): " fmt "\n",                                      \
              (level == 4)                                                     \
                ? "ERR"                                                        \
                : ((level == 3) ? "WRN" : ((level == 2) ? "DBG" : "INFO")),    \
              syscall(__NR_gettid),                                            \
              __FUNCTION__,                                                    \
              __LINE__,                                                        \
              ##__VA_ARGS__);                                                  \
            fflush(stdout);                                                    \
        }                                                                      \
    } while (0)
#else
#define AIC_LOG(level, fmt, ...)                                               \
    do {                                                                       \
        if (level > INFO) {                                                    \
            __android_log_print(                                               \
              (level == 4)                                                     \
                ? ANDROID_LOG_ERROR                                            \
                : ((level == 3)                                                \
                     ? ANDROID_LOG_WARN                                        \
                     : ((level == 2) ? ANDROID_LOG_DEBUG : ANDROID_LOG_INFO)), \
              LOG_TAG,                                                         \
              "%s(%d): " fmt,                                                  \
              __FUNCTION__,                                                    \
              __LINE__,                                                        \
              ##__VA_ARGS__);                                                  \
        }                                                                      \
    } while (0)
#endif

#endif //_RECEIVER_LOG_H