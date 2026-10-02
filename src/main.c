#include "hidalgo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "util.h"

static void usage(const char *prog) {
     fprintf(stderr, "use:\n" " %s <path> <term>\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 3) { usage(argv[0]); return 1; }

    const char *root = argv[1];
    const char *term = argv[2];

    sIndex *idx = index_new();
    if (!idx) { fprintf(stderr, "no memory\n"); return 1; }

    double t0 = sNow();
    size_t n = index_scan(idx, root);
    double t1 = sNow();
    fprintf(stderr, "indexed %zu files in %.3f s\n", n, t1 - t0);

    sResult results[20];
    double t2 = sNow();
    size_t hits = index_search(idx, term, results, 20);
    double t3 = sNow();

    for (size_t i = 0; i < hits; i++) 
    	printf("[%4d] %s\n", results[i].score, results[i].path);

    fprintf(stderr, "%zu results in %.3f s\n", hits, t3 - t2);

    index_free(idx);
    return 0;
}
