#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include "cache.h"
#include "index.h"
#include "util.h"

#define CACHE_MAGIC 0x48494443u
#define CACHE_VER 1u

#define DMAP_BUCKETS 65536

typedef struct DEntry {
    char *path;
    uint64_t mtime;
    struct DEntry *next;
} DEntry;

static DEntry *g_dmap[DMAP_BUCKETS];
static int g_dmap_loaded = 0;

static DEntry *dmap_find(const char *path) {
    uint64_t h = sHash(path) % DMAP_BUCKETS;
    for (DEntry *e = g_dmap[h]; e; e = e->next)
    	if (strcmp(e->path,path) == 0) return e;
    return NULL;
}

static void dmap_set(const char *path, uint64_t mtime) {
    uint64_t h = sHash(path) % DMAP_BUCKETS;
    for (DEntry *e = g_dmap[h]; e; e = e->next)
    	if (strcmp(e->path,path) == 0) return;
    DEntry *e = malloc(sizeof(DEntry));
    if (!e) return;
    e->path = strdup(path);
    e->mtime = mtime;
    e->next = g_dmap[h];
    g_dmap[h] = e;
}

static void dmap_free(void) {
    for (size_t i = 0; i < DMAP_BUCKETS; i++) {
    	DEntry *e = g_dmap[i];
    	while (e) { DEntry *n = e->next; free(e->path); free(e); e = n; }
    	g_dmap[i] = NULL;
    }
    g_dmap_loaded = 0;
}

uint64_t cache_dir_mtime(const char *dir_path) {
    if (!g_dmap_loaded) return 0;
    DEntry *e = dmap_find(dir_path);
    return e ? e->mtime : 0;
}
void cache_record_dir(const char *path, uint64_t mtime) {
    dmap_set(path, mtime);
}

static char *cache_file_path(const char *root) {
    const char *home = getenv("HOME");
    if (!home) {
    	struct passwd *pw = getpwuid(getuid());
    	home = pw ? pw->pw_dir : "/tmp";
    }
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/.cache/hidalgo", home);
    mkdir(dir, 0755);
    
    char *p = malloc(4096);
    if (!p) return NULL;
    uint64_t h = sHash(root);
    snprintf(p, 4096, "%s/%015lx.bin", dir, (unsigned long)h);
    return p;
}

bool cache_save(const sIndex *idx, const char *root) {
    if (!idx) return false;
    char *path = cache_file_path(root);
    if (!path) return false;
    
    FILE *fp = fopen(path, "wb");
    if (!fp) { free(path); return false; }
    
    size_t n_dirs = 0;
    for (size_t i = 0; i < DMAP_BUCKETS; i++)
    	for (DEntry *e = g_dmap[i]; e; e = e->next) n_dirs++;
    
    uint32_t magic = CACHE_MAGIC;
    uint32_t version = CACHE_VER;
    uint64_t n_files = idx->count;
    uint64_t nd = n_dirs;
    
    fwrite(&magic, 4, 1, fp);
    fwrite(&version , 4, 1, fp);
    fwrite(&n_files, 8, 1, fp);
    fwrite(&nd, 8, 1, fp);
    // files
    for (size_t i = 0; i < idx->capacity; i++) {
    	for (sEntry *e = idx->buckets[i]; e; e = e->next) {
    	     uint32_t plen = (uint32_t)strlen(e->path);
    	     fwrite(&plen, 4, 1, fp);
    	     fwrite(e->path, 1, plen, fp);
    	     fwrite(&e->size, 8, 1, fp);
    	     fwrite(&e->mtime, 8, 1, fp);
    	}
    }
    // directorys
    for (size_t i = 0; i < DMAP_BUCKETS; i++) {
    	for (DEntry *e = g_dmap[i]; e; e = e->next) {
    	    uint32_t plen = (uint32_t)strlen(e->path);
    	    fwrite(&plen, 4, 1, fp);
    	    fwrite(e->path, 1, plen, fp);
    	    fwrite(&e->mtime, 8, 1, fp);
    	}
    }
    
    fclose(fp);
    free(path);
    return true;
}

bool cache_load(sIndex *idx, const char *root) {
    if (!idx) return false;
    char *path = cache_file_path(root);
    if (!path) return false;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) { free(path); return false; }
    
    uint32_t magic, version;
    uint64_t n_files, n_dirs;
    
    if (fread(&magic, 4, 1, fp) != 1) { fclose(fp); free(path); return false; }
    if (fread(&version, 4, 1, fp) != 1) { fclose(fp); free(path); return false; }
    if (fread(&n_files, 8, 1, fp) != 1) { fclose(fp); free(path); return false; }
    if (fread(&n_dirs, 8, 1, fp) != 1) { fclose(fp); free(path); return false; }
    
    if (magic != CACHE_MAGIC || version != CACHE_VER) {
    	fclose(fp); free(path); return false;
    }
    dmap_free();
    
    for (uint64_t i = 0; i < n_files; i++) {
    	uint32_t plen;
    	if (fread(&plen, 4, 1, fp) != 1) break;
    	if (plen >= PATH_MAX) break;
    	
    	char p[PATH_MAX];
    	if (fread(p, 1, plen, fp) != plen) break;
    	p[plen] = '\0';
    	
    	sFile f;
    	memset(&f, 0, sizeof(f));
    	memcpy(f.path, p, plen + 1);
    	if (fread(&f.size, 8, 1, fp) != 1) break;
    	if (fread(&f.mtime, 8, 1, fp) != 1) break;
    	
    	index_add(idx, &f);
    }
    
    for (uint64_t i = 0; i < n_dirs; i++) {
    	uint32_t plen;
    	if (fread(&plen, 4, 1, fp) != 1) break;
    	if (plen >= PATH_MAX) break;
    	
    	char p[PATH_MAX];
    	if (fread(p, 1, plen, fp) != plen) break;
    	p[plen] = '\0';
    	
    	uint64_t mtime;
    	if (fread(&mtime, 8, 1, fp) != 1) break;
    	dmap_set(p, mtime);
    }
    
    g_dmap_loaded = 1;
    fclose(fp);
    free(path);
    return true;
}

bool cache_clear(const char *root) {
   dmap_free();
   char *path = cache_file_path(root);
   if (!path) return false;
   int r = remove(path);
   free(path);
   return r == 0;
}
