#include <stdio.h>
#include "hidalgo.h"
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

static size_t walk(sIndex *idx, const char *path) {
    DIR *d = opendir(path);
    if (!d) return 0;
    
    size_t added = 0;
    struct dirent *e;
    
    while ((e = readdir(d))) {
    	if (e->d_name[0] == '.') continue;
    	
    	char full[PATH_MAX];
    	int n = snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
    	if (n < 0 || (size_t)n >= sizeof(full)) continue;
    	
    	struct stat st;
    	if (lstat(full, &st) < 0) continue;
    	
    	if (S_ISDIR(st.st_mode)) {
    	   added += walk(idx, full);
    	} else if (S_ISREG(st.st_mode)) {
           sFile f;
           memset(&f, 0, sizeof(f));
           strncpy(f.path, full, PATH_MAX - 1);
           f.size = (uint64_t)st.st_size;
           f.mtime = (uint64_t)st.st_mtime;
           if (index_add(idx, &f)) added++;
        }
    }
    closedir(d);
    return added;
}

size_t index_scan(sIndex *idx, const char *root) {
    if (!idx || !root) return 0;
    return walk(idx, root);
}
