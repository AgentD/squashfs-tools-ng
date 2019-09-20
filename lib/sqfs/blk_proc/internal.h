#ifndef INTERNAL_H
#define INTERNAL_H

#include "config.h"
#include "sqfs/predef.h"

#include "sqfs/block_processor.h"
#include "sqfs/compress.h"
#include "sqfs/error.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>

#ifdef WITH_PTHREAD
#include <pthread.h>
#endif

#ifdef WITH_PTHREAD
typedef struct {
	sqfs_block_processor_t *shared;
	sqfs_compressor_t *cmp;
	pthread_t thread;
	uint8_t scratch[];
} compress_worker_t;
#endif

struct sqfs_block_processor_t {
	/* synchronization primitives */
#ifdef WITH_PTHREAD
	pthread_mutex_t mtx;
	pthread_cond_t queue_cond;
	pthread_cond_t done_cond;
#endif

	/* needs rw access by worker and main thread */
	sqfs_block_t *queue;
	sqfs_block_t *queue_last;

	sqfs_block_t *done;
	bool terminate;
	size_t backlog;

	/* used by main thread only */
	uint32_t enqueue_id;
	uint32_t dequeue_id;

	unsigned int num_workers;
	sqfs_block_cb cb;
	void *user;
	int status;
	size_t max_backlog;

	/* used only by workers */
	size_t max_block_size;

#ifdef WITH_PTHREAD
	compress_worker_t *workers[];
#else
	sqfs_compressor_t *cmp;
	uint8_t scratch[];
#endif
};

SQFS_INTERNAL
int sqfs_block_process(sqfs_block_t *block, sqfs_compressor_t *cmp,
		       uint8_t *scratch, size_t scratch_size);

SQFS_INTERNAL int process_completed_blocks(sqfs_block_processor_t *proc,
					   sqfs_block_t *queue);

#endif /* INTERNAL_H */
