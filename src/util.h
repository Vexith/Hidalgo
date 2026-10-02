#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

void sLog(const char *fmt, ...);
double sNow(void);
uint64_t sHash(const char *s);

#endif
