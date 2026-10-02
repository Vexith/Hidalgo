#include "util.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

void sLog(const char *fmt, ...){
     va_list ap;
     va_start(ap, fmt);
     vfprintf(stderr, fmt, ap);
     va_end(ap);
     fputc('\n', stderr);
}
double sNow(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

uint64_t sHash(const char *s) {
    uint64_t h = 1469598103934556603ULL;
    while (*s){
    	h ^= (unsigned char)*s++;
    	h *= 1099511628211ULL;
    }
    return h;
}

