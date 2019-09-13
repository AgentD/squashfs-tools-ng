#ifndef INTERNAL_H
#define INTERNAL_H

#include "config.h"
#include "sqfs/predef.h"

#include "sqfs/block_processor.h"
#include "sqfs/compress.h"
#include "sqfs/error.h"
#include "util.h"

SQFS_INTERNAL
int sqfs_block_process(sqfs_block_t *block, sqfs_compressor_t *cmp,
		       uint8_t *scratch, size_t scratch_size);

#endif /* INTERNAL_H */
