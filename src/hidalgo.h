#ifndef HIDALGO_H
#define HIDALGO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define PATH_MAX 4096

typedef struct sIndex sIndex;
typedef struct {
    char path[PATH_MAX];
    uint64_t size;
    uint64_t mtime;
} sFile;

typedef struct {
    char path[PATH_MAX];
    int score;
} sResult;

sIndex *index_new(void);
void index_free(sIndex *idx);

bool index_add(sIndex *idx, const sFile *file);
bool index_remove(sIndex *idx, const char *path);
size_t index_scan(sIndex *idx, const char *root);

size_t index_search(sIndex *idx, const char *term, sResult *out, size_t max_results);
size_t index_count(const sIndex *idx);

#endif
