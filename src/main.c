#include "hidalgo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "util.h"
#include "cache.h"

static void usage(const char *prog) {
     fprintf(stderr, "use:\n" " %s [OPTIONS] <path> <term>\n", prog);
     fprintf(stderr, "  -n N max results (default: 0, 0 = all)\n");
}

int main(int argc, char **argv) {
    size_t max_results = 0;
    int argi = 1;
    
    if (argi < argc && strcmp(argv[argi], "-h") == 0) {
    	usage(argv[0]);
    	return 0;
    }
    
    if (argi < argc && strcmp(argv[argi], "-n") == 0) {
	if (argi + 1 >= argc) {
	    fprintf(stderr, "hidalgo: -n needs a value \n");
	    return 2;
	}
	long v = strtol(argv[argi + 1], NULL, 10);
	if (v < 0) {
	    fprintf(stderr, "hidalgo: -n must be >= 0 (0 = all)\n");
	    return 2;
	}
	max_results = (size_t)v;
	argi += 2;
    }
    
    if (argi + 2 != argc) {
    	usage(argv[0]);
    	return 2;
    }
 
    const char *root = argv[argi];
    const char **terms = (const char **)&argv[argi + 1];
    size_t nterms = (size_t)(argc - argi - 1);

    if (nterms < 1) { usage(argv[0]); return 2; }
    sIndex *idx = index_new();
    cache_load(idx, root);
    if (!idx) { fprintf(stderr, "no memory\n"); return 1; }

    double t0 = sNow();
    size_t n = index_scan(idx, root);
    double t1 = sNow();
    fprintf(stderr, "indexed %zu files in %.3f s\n", n, t1 - t0);
    
    size_t limit = (max_results == 0) ? index_count(idx) : max_results;
    if (limit == 0) limit = 1;

    sResult *results = calloc(limit, sizeof(sResult));
    if (!results) { index_free(idx); return 1; }
    
    
    double t2 = sNow();
    size_t hits = index_search(idx, terms, nterms, results, limit);
    double t3 = sNow();

    for (size_t i = 0; i < hits; i++) 
    	printf("[%4d] %s\n", results[i].score, results[i].path);

    fprintf(stderr, "%zu results in %.3f s\n", hits, t3 - t2);
    cache_save(idx, root);
    index_free(idx);
    return 0;
}
