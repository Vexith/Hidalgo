#include <stdio.h>
#include "hidalgo.h"
#include "util.h"
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>

#define N_THREADS 4

typedef struct QueueNode {
	char path[PATH_MAX];
	struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *head;
    QueueNode *tail;
    int active;
    int done;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} WorkQueue;

typedef struct {
    sIndex *idx;
    WorkQueue *q;
    pthread_mutex_t *idx_mutex;
    size_t added;
} Worker;

static void queue_init(WorkQueue *q) {
    q->head = q->tail = NULL;
    q->active = 0;
    q->done = 0;
    pthread_cond_init(&q->cond, NULL);
    pthread_mutex_init(&q->mutex, NULL);
}

static void queue_push(WorkQueue *q, const char *path) {
    QueueNode *n = malloc(sizeof(QueueNode));
    if (!n) return;
    strncpy(n->path, path, PATH_MAX - 1);
    n->path[PATH_MAX - 1] = '\0';
    n->next = NULL;
    
    pthread_mutex_lock(&q->mutex);
    if (q->tail) q->tail->next = n;
    else q->head = n;
    q->tail = n;
    q->active++;
    
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

static int queue_pop(WorkQueue *q, char *out) {
   pthread_mutex_lock(&q->mutex);
   while (q->head == NULL && !q->done) {
     pthread_cond_wait(&q->cond, &q->mutex);
   }
   if (q->head == NULL && q->done) {
     pthread_mutex_unlock(&q->mutex);
     return 0;
   }
   QueueNode *n = q->head;
   q->head = n->next;
   if (q->head == NULL) q->tail = NULL;
   strncpy(out, n->path, PATH_MAX - 1);
   out[PATH_MAX - 1] = '\0';
   free(n);
   pthread_mutex_unlock(&q->mutex);
   return 1;
}

static void queue_finishitem(WorkQueue *q) {
    pthread_mutex_lock(&q->mutex);
    q->active--;
    if (q->active == 0) {
    	q->done = 1;
    	pthread_cond_broadcast(&q->cond);
    }
    pthread_mutex_unlock(&q->mutex);
}

static void *worker_main(void *arg) {
    Worker *w = arg;
    char path[PATH_MAX];
    
    while (queue_pop(w->q, path)) {
    	DIR *d = opendir(path);
    	if (d) {
    	   struct dirent *e;
    	   while ((e = readdir(d))) {
    	   	if (e->d_name[0] == '.') continue;
    	   	
    	   	char full[PATH_MAX];
    	   	size_t plen = strlen(path);
    	   	int n;
    	   	if (plen > 0 && path[plen - 1] == '/')
    	            n = snprintf(full, sizeof(full), "%s%s", path, e->d_name);
    	        else
    	            n = snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
    	        if (n < 0 || (size_t)n >= sizeof(full)) continue;
    	        struct stat st;
    	        
    	        if (lstat(full, &st) < 0) continue;
    	        if (S_ISDIR(st.st_mode)) {
    	            queue_push(w->q, full);
    	        } else if (S_ISREG(st.st_mode)) {
    	            sFile f;
    	            memset(&f, 0, sizeof(f));
    	            strncpy(f.path, full, PATH_MAX - 1);
    	            f.size = (uint64_t)st.st_size;
    	            f.mtime = (uint64_t)st.st_mtime;
    	            
    	            pthread_mutex_lock(w->idx_mutex);
    	            if (index_add(w->idx, &f)) w->added++;
    	            pthread_mutex_unlock(w->idx_mutex);
    	        }
    	    }
    	    closedir(d);
    	}
    	queue_finishitem(w->q);
     }
     return NULL;
}
size_t index_scan(sIndex *idx, const char *root) {
    if (!idx || !root) return 0;
    
    
    char clean[PATH_MAX];
    strncpy(clean, root, PATH_MAX - 1);
    clean[PATH_MAX - 1] = '\0';
    size_t len = strlen(clean);
    while (len > 1 && clean[len - 1] == '/') clean[--len] = '\0';
    
    WorkQueue q;
    queue_init(&q);
    queue_push(&q, clean);
    
    pthread_mutex_t idx_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_t threads[N_THREADS];
    Worker workers[N_THREADS];
    
    for (int i = 0; i < N_THREADS; i++) {
    	workers[i].idx = idx;
    	workers[i].q = &q;
    	workers[i].idx_mutex = &idx_mutex;
    	workers[i].added = 0;
    	pthread_create(&threads[i], NULL, worker_main, &workers[i]);
    }
    
    size_t total = 0;
    for (int i = 0; i < N_THREADS; i++) {
    	pthread_join(threads[i], NULL);
    	total += workers[i].added;
    }
    
    pthread_mutex_destroy(&q.mutex);
    pthread_cond_destroy(&q.cond);
    
    return total;
}

