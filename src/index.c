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
static int glob_match(const char *pattern, const char *str) {
    const char *target = str;
    if (strchr(pattern, '/') == NULL) {
        const char *slash = strrchr(str, '/');
        if (slash) target = slash + 1;
    }
    const char *p = pattern;
    const char *s = target;
    if (p[0] == '*' && p[1] == '\0') return 1;
    if (p[0] == '*') {
        const char *rest = p + 1;
        size_t rlen = strlen(rest), slen = strlen(s);
        if (rlen > slen) return 0;
        return strcmp(s + slen - rlen, rest) == 0;
    }
    size_t plen = strlen(p);
    if (plen > 0 && p[plen - 1] == '*')
        return strncmp(s, p, plen - 1) == 0;
    if (strchr(p, '*') != NULL) {
        char pre[PATH_MAX], suf[PATH_MAX];
        const char *star = strchr(p, '*');
        size_t pre_len = (size_t)(star - p);
        const char *after = star + 1;
        if (pre_len >= PATH_MAX) return 0;
        memcpy(pre, p, pre_len); pre[pre_len] = '\0';
        strncpy(suf, after, PATH_MAX - 1); suf[PATH_MAX - 1] = '\0';
        if (strncmp(s, pre, pre_len) != 0) return 0;
        size_t slen = strlen(s), suflen = strlen(suf);
        if (suflen > slen - pre_len) return 0;
        return strcmp(s + slen - suflen, suf) == 0;
    }
    return strcmp(p, s) == 0;
}
size_t index_search(sIndex *idx, const char **terms, size_t nterms, sResult *out, size_t max_results) {
    if (!idx || !terms || nterms == 0 || !out || max_results == 0) return 0;

    size_t n = 0;

    for (size_t i = 0; i < idx->capacity; i++) {
        for (sEntry *e = idx->buckets[i]; e; e = e->next) {
            int best = -1;

            for (size_t t = 0; t < nterms; t++) {
                const char *term = terms[t];
                int s;

                if (term[0] == '.' && strchr(term, '*') == NULL) {
                    const char *slash = strrchr(e->path, '/');
                    const char *base = slash ? slash + 1 : e->path;
                    size_t tlen = strlen(term), blen = strlen(base);
                    s = (blen >= tlen && strcmp(base + blen - tlen, term) == 0) ? 100 : -1;
                } else if (strchr(term, '*') != NULL) {
                    s = glob_match(term, e->path) ? 100 : -1;
                } else {
                    s = fuzzy_match(term, e->path);
                }

                if (s > best) best = s;
            }

            if (best < 0) continue;

            if (n < max_results) {
                out[n].path = e->path;
                out[n].score = best;
                n++;
            } else {
                size_t worst = 0;
                for (size_t k = 1; k < max_results; k++) {
                    if (out[k].score < out[worst].score) worst = k;
                }
                if (best > out[worst].score) {
                    out[worst].path = e->path;
                    out[worst].score = best;
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
