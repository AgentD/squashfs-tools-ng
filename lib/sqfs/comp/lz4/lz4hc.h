/*
   LZ4 HC - High Compression Mode of LZ4
   Header File
   Copyright (C) 2011-2017, Yann Collet.
   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - LZ4 source repository : https://github.com/lz4/lz4
   - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/
#ifndef LZ4_HC_H_19834876238432
#define LZ4_HC_H_19834876238432

#if defined (__cplusplus)
extern "C" {
#endif

/* --- Dependency --- */
/* note : lz4hc requires lz4.h/lz4.c for compilation */
#include "lz4.h"   /* stddef, LZ4LIB_API, LZ4_DEPRECATED */


/* --- Useful constants --- */
#define LZ4HC_CLEVEL_MIN         3
#define LZ4HC_CLEVEL_DEFAULT     9
#define LZ4HC_CLEVEL_OPT_MIN    10
#define LZ4HC_CLEVEL_MAX        12


/*-************************************
 *  Block Compression
 **************************************/
/*! LZ4_compress_HC() :
 *  Compress data from `src` into `dst`, using the powerful but slower "HC" algorithm.
 * `dst` must be already allocated.
 *  Compression is guaranteed to succeed if `dstCapacity >= LZ4_compressBound(srcSize)` (see "lz4.h")
 *  Max supported `srcSize` value is LZ4_MAX_INPUT_SIZE (see "lz4.h")
 * `compressionLevel` : any value between 1 and LZ4HC_CLEVEL_MAX will work.
 *                      Values > LZ4HC_CLEVEL_MAX behave the same as LZ4HC_CLEVEL_MAX.
 * @return : the number of bytes written into 'dst'
 *           or 0 if compression fails.
 */
LZ4LIB_API int LZ4_compress_HC (const char* src, char* dst, int srcSize, int dstCapacity, int compressionLevel);


/* Note :
 *   Decompression functions are provided within "lz4.h" (BSD license)
 */


/*! LZ4_compress_HC_extStateHC() :
 *  Same as LZ4_compress_HC(), but using an externally allocated memory segment for `state`.
 * `state` size is provided by LZ4_sizeofStateHC().
 *  Memory segment must be aligned on 8-bytes boundaries (which a normal malloc() should do properly).
 */
LZ4LIB_API int LZ4_sizeofStateHC(void);
LZ4LIB_API int LZ4_compress_HC_extStateHC(void* stateHC, const char* src, char* dst, int srcSize, int maxDstSize, int compressionLevel);


/*! LZ4_compress_HC_destSize() : v1.9.0+
 *  Will compress as much data as possible from `src`
 *  to fit into `targetDstSize` budget.
 *  Result is provided in 2 parts :
 * @return : the number of bytes written into 'dst' (necessarily <= targetDstSize)
 *           or 0 if compression fails.
 * `srcSizePtr` : on success, *srcSizePtr is updated to indicate how much bytes were read from `src`
 */
LZ4LIB_API int LZ4_compress_HC_destSize(void* stateHC,
                                  const char* src, char* dst,
                                        int* srcSizePtr, int targetDstSize,
                                        int compressionLevel);


/*-************************************
 *  Streaming Compression
 *  Bufferless synchronous API
 **************************************/
 typedef union LZ4_streamHC_u LZ4_streamHC_t;   /* incomplete type (defined later) */

/*
  These functions compress data in successive blocks of any size,
  using previous blocks as dictionary, to improve compression ratio.
  One key assumption is that previous blocks (up to 64 KB) remain read-accessible while compressing next blocks.
  There is an exception for ring buffers, which can be smaller than 64 KB.
  Ring-buffer scenario is automatically detected and handled within LZ4_compress_HC_continue().

  Before starting compression, state must be allocated and properly initialized.
  LZ4_createStreamHC() does both, though compression level is set to LZ4HC_CLEVEL_DEFAULT.

  Selecting the compression level can be done with LZ4_resetStreamHC_fast() (starts a new stream)
  or LZ4_setCompressionLevel() (anytime, between blocks in the same stream) (experimental).
  LZ4_resetStreamHC_fast() only works on states which have been properly initialized at least once,
  which is automatically the case when state is created using LZ4_createStreamHC().

  After reset, a first "fictional block" can be designated as initial dictionary,
  using LZ4_loadDictHC() (Optional).

  Invoke LZ4_compress_HC_continue() to compress each successive block.
  The number of blocks is unlimited.
  Previous input blocks, including initial dictionary when present,
  must remain accessible and unmodified during compression.

  It's allowed to update compression level anytime between blocks,
  using LZ4_setCompressionLevel() (experimental).

  'dst' buffer should be sized to handle worst case scenarios
  (see LZ4_compressBound(), it ensures compression success).
  In case of failure, the API does not guarantee recovery,
  so the state _must_ be reset.
  To ensure compression success
  whenever `dst` buffer size cannot be made >= LZ4_compressBound(),
  consider using LZ4_compress_HC_continue_destSize().

  Whenever previous input blocks can't be preserved unmodified in-place during compression of next blocks,
  it's possible to copy the last blocks into a more stable memory space, using LZ4_saveDictHC().
  Return value of LZ4_saveDictHC() is the size of dictionary effectively saved into 'safeBuffer' (<= 64 KB)

  After completing a streaming compression,
  it's possible to start a new stream of blocks, using the same LZ4_streamHC_t state,
  just by resetting it, using LZ4_resetStreamHC_fast().
*/

LZ4LIB_API void LZ4_resetStreamHC_fast(LZ4_streamHC_t* streamHCPtr, int compressionLevel);   /* v1.9.0+ */

/*^**********************************************
 * !!!!!!   STATIC LINKING ONLY   !!!!!!
 ***********************************************/

/*-******************************************************************
 * PRIVATE DEFINITIONS :
 * Do not use these definitions directly.
 * They are merely exposed to allow static allocation of `LZ4_streamHC_t`.
 * Declare an `LZ4_streamHC_t` directly, rather than any type below.
 * Even then, only do so in the context of static linking, as definitions may change between versions.
 ********************************************************************/

#define LZ4HC_DICTIONARY_LOGSIZE 16
#define LZ4HC_MAXD (1<<LZ4HC_DICTIONARY_LOGSIZE)
#define LZ4HC_MAXD_MASK (LZ4HC_MAXD - 1)

#define LZ4HC_HASH_LOG 15
#define LZ4HC_HASHTABLESIZE (1 << LZ4HC_HASH_LOG)
#define LZ4HC_HASH_MASK (LZ4HC_HASHTABLESIZE - 1)


#if defined(__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#include <stdint.h>

typedef struct LZ4HC_CCtx_internal LZ4HC_CCtx_internal;
struct LZ4HC_CCtx_internal
{
    uint32_t   hashTable[LZ4HC_HASHTABLESIZE];
    uint16_t   chainTable[LZ4HC_MAXD];
    const uint8_t* end;         /* next block here to continue on current prefix */
    const uint8_t* base;        /* All index relative to this position */
    const uint8_t* dictBase;    /* alternate base for extDict */
    uint32_t   dictLimit;       /* below that point, need extDict */
    uint32_t   lowLimit;        /* below that point, no more dict */
    uint32_t   nextToUpdate;    /* index from which to continue dictionary update */
    short      compressionLevel;
    int8_t     favorDecSpeed;   /* favor decompression speed if this flag set,
                                   otherwise, favor compression ratio */
    int8_t     dirty;           /* stream has to be fully reset if this flag is set */
    const LZ4HC_CCtx_internal* dictCtx;
};

#else

typedef struct LZ4HC_CCtx_internal LZ4HC_CCtx_internal;
struct LZ4HC_CCtx_internal
{
    unsigned int   hashTable[LZ4HC_HASHTABLESIZE];
    unsigned short chainTable[LZ4HC_MAXD];
    const unsigned char* end;        /* next block here to continue on current prefix */
    const unsigned char* base;       /* All index relative to this position */
    const unsigned char* dictBase;   /* alternate base for extDict */
    unsigned int   dictLimit;        /* below that point, need extDict */
    unsigned int   lowLimit;         /* below that point, no more dict */
    unsigned int   nextToUpdate;     /* index from which to continue dictionary update */
    short          compressionLevel;
    char           favorDecSpeed;    /* favor decompression speed if this flag set,
                                        otherwise, favor compression ratio */
    char           dirty;            /* stream has to be fully reset if this flag is set */
    const LZ4HC_CCtx_internal* dictCtx;
};

#endif


/* Do not use these definitions directly !
 * Declare or allocate an LZ4_streamHC_t instead.
 */
#define LZ4_STREAMHCSIZE       (4*LZ4HC_HASHTABLESIZE + 2*LZ4HC_MAXD + 56 + ((sizeof(void*)==16) ? 56 : 0) /* AS400*/ ) /* 262200 or 262256*/
#define LZ4_STREAMHCSIZE_SIZET (LZ4_STREAMHCSIZE / sizeof(size_t))
union LZ4_streamHC_u {
    size_t table[LZ4_STREAMHCSIZE_SIZET];
    LZ4HC_CCtx_internal internal_donotuse;
}; /* previously typedef'd to LZ4_streamHC_t */

/* LZ4_streamHC_t :
 * This structure allows static allocation of LZ4 HC streaming state.
 * This can be used to allocate statically, on state, or as part of a larger structure.
 *
 * Such state **must** be initialized using LZ4_initStreamHC() before first use.
 *
 * Note that invoking LZ4_initStreamHC() is not required when
 * the state was created using LZ4_createStreamHC() (which is recommended).
 * Using the normal builder, a newly created state is automatically initialized.
 *
 * Static allocation shall only be used in combination with static linking.
 */

/* LZ4_initStreamHC() : v1.9.0+
 * Required before first use of a statically allocated LZ4_streamHC_t.
 * Before v1.9.0 : use LZ4_resetStreamHC() instead
 */
LZ4LIB_API LZ4_streamHC_t* LZ4_initStreamHC (void* buffer, size_t size);

#if defined (__cplusplus)
}
#endif

#endif /* LZ4_HC_H_19834876238432 */


/*-**************************************************
 * !!!!!     STATIC LINKING ONLY     !!!!!
 * Following definitions are considered experimental.
 * They should not be linked from DLL,
 * as there is no guarantee of API stability yet.
 * Prototypes will be promoted to "stable" status
 * after successfull usage in real-life scenarios.
 ***************************************************/
#ifdef LZ4_HC_STATIC_LINKING_ONLY   /* protection macro */
#ifndef LZ4_HC_SLO_098092834
#define LZ4_HC_SLO_098092834

#define LZ4_STATIC_LINKING_ONLY   /* LZ4LIB_STATIC_API */
#include "lz4.h"

#if defined (__cplusplus)
extern "C" {
#endif

/*! LZ4_setCompressionLevel() : v1.8.0+ (experimental)
 *  It's possible to change compression level
 *  between successive invocations of LZ4_compress_HC_continue*()
 *  for dynamic adaptation.
 */
LZ4LIB_STATIC_API void LZ4_setCompressionLevel(
    LZ4_streamHC_t* LZ4_streamHCPtr, int compressionLevel);

/*! LZ4_resetStreamHC_fast() : v1.9.0+
 *  When an LZ4_streamHC_t is known to be in a internally coherent state,
 *  it can often be prepared for a new compression with almost no work, only
 *  sometimes falling back to the full, expensive reset that is always required
 *  when the stream is in an indeterminate state (i.e., the reset performed by
 *  LZ4_resetStreamHC()).
 *
 *  LZ4_streamHCs are guaranteed to be in a valid state when:
 *  - returned from LZ4_createStreamHC()
 *  - reset by LZ4_resetStreamHC()
 *  - memset(stream, 0, sizeof(LZ4_streamHC_t))
 *  - the stream was in a valid state and was reset by LZ4_resetStreamHC_fast()
 *  - the stream was in a valid state and was then used in any compression call
 *    that returned success
 *  - the stream was in an indeterminate state and was used in a compression
 *    call that fully reset the state (LZ4_compress_HC_extStateHC()) and that
 *    returned success
 *
 *  Note:
 *  A stream that was last used in a compression call that returned an error
 *  may be passed to this function. However, it will be fully reset, which will
 *  clear any existing history and settings from the context.
 */
LZ4LIB_STATIC_API void LZ4_resetStreamHC_fast(
    LZ4_streamHC_t* LZ4_streamHCPtr, int compressionLevel);

#if defined (__cplusplus)
}
#endif

#endif   /* LZ4_HC_SLO_098092834 */
#endif   /* LZ4_HC_STATIC_LINKING_ONLY */
