#ifndef CACHE_H
#define CACHE_H

#include "hidalgo.h"

bool cache_load(sIndex *idx, const char *root);
bool cache_save(const sIndex *idx, const char *root);
bool cache_clear(const char *root);
uint64_t cache_dir_mtime(const char *dir_path);
void cache_record_dir(const char *path, uint64_t mtime);

#endif
