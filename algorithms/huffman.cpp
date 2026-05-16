#include <emscripten/emscripten.h>
#include "huffman_internal.h"

// All tree/heap/code logic now lives in huffman_internal.h.
// This file only contains the two exported WASM entry points.

extern "C" {

EMSCRIPTEN_KEEPALIVE
int huffmanCompress(const unsigned char* input, unsigned char* output, int inputLen) {
    if (inputLen == 0) return 0;
    return huffmanCompressBuffer(input, inputLen, output);
}

EMSCRIPTEN_KEEPALIVE
int huffmanDecompress(const unsigned char* input, unsigned char* output, int inputLen) {
    if (inputLen == 0) return 0;
    return huffmanDecompressBuffer(input, inputLen, output);
}

} // extern "C"