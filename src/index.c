#include "index.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

sIndex *index_new(void){
    sIndex *idx = calloc(1, sizeof(sIndex));
    if (!idx) return NULL;
    
    idx->buckets = calloc(BUCKETS, sizeof(sEntry*));
    if (!idx->buckets) {
    	free(idx);
    	return NULL;
    }
    idx->capacity = BUCKETS;
    return idx;
}

void index_free(sIndex *idx){
   if (!idx) return;
   
   for (size_t i = 0; i < idx->capacity; i++){
   	sEntry *e = idx->buckets[i];
   	while (e) {
   	   sEntry *next = e->next;
   	   free(e);
   	   e = next;
   	}
   }
   free(idx->buckets);
   free(idx);
}

bool index_add(sIndex *idx, const sFile *file) {
    if (!idx || !file) return false;
    
    uint64_t h = sHash(file->path);
    size_t bucket = h % idx->capacity;
    
    sEntry *e = idx->buckets[bucket];
    while (e) {
    	if (e->hash == h && strcmp(e->path, file->path) == 0) {
    	     e->size = file->size;
    	     e->mtime = file->mtime;
    	     return true;    	
    	}
    	e = e->next;
    }
    
    sEntry *ne = calloc(1, sizeof(sEntry));
    if (!ne) return false;
    
    strncpy(ne->path, file->path, PATH_MAX - 1);
    ne->path[PATH_MAX - 1] = '\0';
    ne->size = file->size;
    ne->mtime = file->mtime;
    ne->hash = h;
    ne->next = idx->buckets[bucket];
    
    idx->buckets[bucket] = ne;
    idx->count++;
    return true;
}

bool index_remove(sIndex *idx, const char *path) {
    if (!idx || !path) return false;
    
    uint64_t h = sHash(path);
    size_t bucket = h % idx->capacity;
    
    sEntry *e = idx->buckets[bucket];
    sEntry *prev = NULL;
    
    while (e) {
    	if (e->hash == h && strcmp(e->path, path) == 0) {
    	     if (prev) prev->next = e->next;
    	     else idx ->buckets[bucket] = e->next;
    	     free(e);
    	     idx->count--;
    	     return true;
    	}
    	prev = e;
    	e = e->next;
    }
    return false;
}

size_t index_count(const sIndex *idx) {
    return idx ? idx->count : 0;
}

static int fuzzy_match(const char *needle, const char *haystack) {
    if (!needle || !*needle) return 0;
    
    int score = 0, streak = 0;
    const char *h = haystack;
    
    for (const char *n = needle; *n; n++) {
    	char nc = *n;
    	if (nc >= 'A' && nc <= 'Z') nc = (char)(nc + 32);
    	bool found = false;
    	
    	while (*h) {
    	  char hc = *h;
    	  if (hc >= 'A' && hc <= 'Z') hc = (char)(hc + 32);
    	  h++;
    	  if (hc == nc) {
    	      found = true;
    	      streak++;
    	      score += streak * 4;
    	      break;
    	  }
    	  streak = 0;
    	}
    	if (!found) return -1;
    }
    score -= (int)strlen(haystack) / 8;
    return score;
}

size_t index_search(sIndex *idx, const char *term, sResult *out, size_t max_results) {
    if (!idx || !term || !out || max_results == 0) return 0;

    size_t n = 0;

    for (size_t i = 0; i < idx->capacity; i++) {
        for (sEntry *e = idx->buckets[i]; e; e = e->next) {
            int score = fuzzy_match(term, e->path);
            if (score < 0) continue;

            if (n < max_results) {
                strncpy(out[n].path, e->path, PATH_MAX - 1);
                out[n].path[PATH_MAX - 1] = '\0';
                out[n].score = score;
                n++;
            } else {
                size_t worst = 0;
                for (size_t k = 1; k < max_results; k++) {
                    if (out[k].score < out[worst].score)
                        worst = k;
                }
                if (score > out[worst].score) {
                    strncpy(out[worst].path, e->path, PATH_MAX - 1);
                    out[worst].path[PATH_MAX - 1] = '\0';
                    out[worst].score = score;
                }
            }
        }
    }
    for (size_t i = 1; i < n; i++) {
        sResult key = out[i];
        size_t j = i;
        while (j > 0 && out[j-1].score < key.score) {
            out[j] = out[j-1];
            j--;
        }
        out[j] = key;
    }

    return n;
}
      	
