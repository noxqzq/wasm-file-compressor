#include <emscripten/emscripten.h>
#include <string.h>
#include <stdlib.h>
 
// =============================================================================
//  ZSTD-INSPIRED COMPRESSOR
//  Pipeline: Fast hash-table LZ77  →  Huffman on literals
//
//  Key difference from the naive LZ77:
//    Old: for every input position, scan up to WINDOW_SIZE bytes backward → O(n·w)
//    New: hash the next 4 bytes, look up a table → O(1) candidate, then extend
//
//  Wire format
//  ───────────
//  [4 bytes] original input length (LE uint32)
//  [4 bytes] literal section length (LE uint32)   ← compressed by Huffman
//  [4 bytes] sequence section length (LE uint32)  ← raw sequence descriptors
//  [516 bytes] Huffman frequency table (same layout as huffman.cpp)
//  [literal section] Huffman-compressed literal bytes
//  [sequence section] packed sequence descriptors (see below)
//
//  Sequence descriptor (9 bytes each):
//    [4 bytes] number of literals before this match (LE uint32)
//    [2 bytes] match offset (LE uint16, distance back into output)
//    [2 bytes] match length (LE uint16)
//    [1 byte]  flags (bit 0 = 1 means "last sequence / no more matches follow")
// =============================================================================
 
// ---------------------------------------------------------------------------
// Shared Huffman primitives (tree, heap, codes) — same logic as huffman.cpp
// ---------------------------------------------------------------------------

#define HASH_BITS   16
#define HASH_SIZE   (1 << HASH_BITS)
#define WINDOW_MAX  65535
#define MIN_MATCH   4
#define MAX_MATCH   65535