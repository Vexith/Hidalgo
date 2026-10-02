#ifndef INTERNAL_INDEX_H
#define INTERNAL_INDEX_H

#include "hidalgo.h"

#define BUCKETS 65536

typedef struct sEntry {
     char path[PATH_MAX];
     uint64_t size;
     uint64_t mtime;
     uint64_t hash;
     struct sEntry *next;
} sEntry;

struct sIndex {
    sEntry **buckets;
    size_t count;
    size_t capacity;
};

#endif
