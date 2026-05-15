#include <emscripten/emscripten.h>
#include <string.h>
#include <stdlib.h>

#define MAX_SYMBOLS 256
#define HUFFMAN_HEADER_SIZE (4 + MAX_SYMBOLS * 2)

struct HuffmanNode {
    unsigned char byte;
    int frequency;
    HuffmanNode* left;
    HuffmanNode* right;
};

HuffmanNode* createNode(unsigned char byte, int freq) {
    HuffmanNode* node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    node->byte      = byte;
    node->frequency = freq;
    node->left      = nullptr;
    node->right     = nullptr;
    return node;
}

void freeTree(HuffmanNode* node) {
    if (!node) {
        return;
    }
    freeTree(node->left);
    freeTree(node->right);
    free(node);
}

// --- Min-heap (priority queue) ---
// Keeps the lowest-frequency node at index 0.
// Push and pop are both O(log n).

struct MinHeap {
    HuffmanNode** data;
    int size;
    int capacity;
};

MinHeap* createHeap(int capacity) {
    MinHeap* heap   = (MinHeap*)malloc(sizeof(MinHeap));
    heap->data      = (HuffmanNode**)malloc(capacity * sizeof(HuffmanNode*));
    heap->size      = 0;
    heap->capacity  = capacity;
    return heap;
}

void freeHeap(MinHeap* heap) {
    free(heap->data);
    free(heap);
}

void heapSwap(MinHeap* heap, int a, int b) {
    HuffmanNode* tmp = heap->data[a];
    heap->data[a]    = heap->data[b];
    heap->data[b]    = tmp;
}

// Push node in, bubble it up until heap property is restored
void heapPush(MinHeap* heap, HuffmanNode* node) {
    int i = heap->size++;
    heap->data[i] = node;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap->data[parent]->frequency <= heap->data[i]->frequency) {
            break;
        }
        heapSwap(heap, parent, i);
        i = parent;
    }
}

// Remove and return the minimum (root), sink the new root down
HuffmanNode* heapPop(MinHeap* heap) {
    HuffmanNode* min = heap->data[0];
    heap->data[0]    = heap->data[--heap->size];
    int i = 0;
    while (true) {
        int left     = 2 * i + 1;
        int right    = 2 * i + 2;
        int smallest = i;
        if (left  < heap->size && heap->data[left]->frequency  < heap->data[smallest]->frequency) {
            smallest = left;
        }
        if (right < heap->size && heap->data[right]->frequency < heap->data[smallest]->frequency) {
            smallest = right;
        }
        if (smallest == i) {
            break;
        }
        heapSwap(heap, i, smallest);
        i = smallest;
    }
    return min;
}

// --- Frequency table ---

void buildFrequencyTable(const unsigned char* input, int inputLen, int* frequencies) {
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        frequencies[i] = 0;
    }
    for (int i = 0; i < inputLen; i++) {
        frequencies[input[i]]++;
    }
}

// --- Tree builder ---

HuffmanNode* buildHuffmanTree(int* frequencies) {
    // Count how many distinct symbols exist
    int symbolCount = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (frequencies[i] > 0) {
            symbolCount++;
        }
    }

    if (symbolCount == 0) {
        return nullptr;
    }

    MinHeap* heap = createHeap(symbolCount * 2);

    // Insert one leaf node per symbol
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (frequencies[i] > 0) {
            heapPush(heap, createNode((unsigned char)i, frequencies[i]));
        }
    }

    // Merge the two lowest-frequency nodes until only the root remains
    while (heap->size > 1) {
        HuffmanNode* left  = heapPop(heap);
        HuffmanNode* right = heapPop(heap);

        HuffmanNode* parent = createNode(0, left->frequency + right->frequency);
        parent->left  = left;
        parent->right = right;

        heapPush(heap, parent);
    }

    HuffmanNode* root = heapPop(heap);
    freeHeap(heap);
    return root;
}

// --- Code generation ---
// Walks the tree recursively, building a binary code for each leaf

void generateCodes(HuffmanNode* node, unsigned int code, int depth,
                   unsigned int* codes, int* codeLengths) {
    if (!node) {
        return;
    }
    if (!node->left && !node->right) {
        // Leaf node — store the code and its length
        codes[node->byte]       = code;
        codeLengths[node->byte] = depth == 0 ? 1 : depth;  // single-symbol edge case
        return;
    }
    if (node->left) {
        generateCodes(node->left,  code << 1,        depth + 1, codes, codeLengths);
    }
    if (node->right) {
        generateCodes(node->right, (code << 1) | 1,  depth + 1, codes, codeLengths);
    }
}

// Header layout:
//   Bytes 0-3   : original input length (little-endian int32)
//   Bytes 4-515 : frequency table, 256 x 2 bytes (little-endian int16)
//   Bytes 516-  : compressed bit-stream

extern "C" {

EMSCRIPTEN_KEEPALIVE
int huffmanCompress(const unsigned char* input, unsigned char* output, int inputLen) {
    if (inputLen == 0) {
        return 0;
    }

    int frequencies[MAX_SYMBOLS];
    buildFrequencyTable(input, inputLen, frequencies);

    HuffmanNode* root = buildHuffmanTree(frequencies);
    if (!root) {
        return 0;
    }

    unsigned int codes[MAX_SYMBOLS]      = {0};
    int          codeLengths[MAX_SYMBOLS] = {0};

    generateCodes(root, 0, 0, codes, codeLengths);
    freeTree(root);

    int outPos = 0;

    // Original length (4 bytes, little-endian)
    output[outPos++] = (unsigned char)( inputLen        & 0xFF);
    output[outPos++] = (unsigned char)((inputLen >>  8) & 0xFF);
    output[outPos++] = (unsigned char)((inputLen >> 16) & 0xFF);
    output[outPos++] = (unsigned char)((inputLen >> 24) & 0xFF);

    // Frequency table (256 x 2 bytes, little-endian int16)
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        int f = frequencies[i] > 0xFFFF ? 0xFFFF : frequencies[i];
        output[outPos++] = (unsigned char)( f       & 0xFF);
        output[outPos++] = (unsigned char)((f >> 8) & 0xFF);
    }

    // Encode bit-stream
    unsigned int bitBuffer = 0;
    int          bitCount  = 0;

    for (int i = 0; i < inputLen; i++) {
        unsigned char b       = input[i];
        unsigned int  code    = codes[b];
        int           codeLen = codeLengths[b];

        bitBuffer = (bitBuffer << codeLen) | code;
        bitCount += codeLen;

        while (bitCount >= 8) {
            bitCount -= 8;
            output[outPos++] = (unsigned char)((bitBuffer >> bitCount) & 0xFF);
        }
    }

    // Flush remaining bits (pad with zeros)
    if (bitCount > 0) {
        output[outPos++] = (unsigned char)((bitBuffer << (8 - bitCount)) & 0xFF);
    }

    return outPos;
}

EMSCRIPTEN_KEEPALIVE
int huffmanDecompress(const unsigned char* input, unsigned char* output, int inputLen) {
    if (inputLen <= HUFFMAN_HEADER_SIZE) {
        return 0;
    }

    int pos = 0;

    // Original length
    int originalLen = (int)input[pos]
                    | ((int)input[pos + 1] <<  8)
                    | ((int)input[pos + 2] << 16)
                    | ((int)input[pos + 3] << 24);
    pos += 4;

    // Frequency table
    int frequencies[MAX_SYMBOLS];
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        frequencies[i] = (int)input[pos] | ((int)input[pos + 1] << 8);
        pos += 2;
    }

    HuffmanNode* root = buildHuffmanTree(frequencies);
    if (!root) {
        return 0;
    }

    int outPos = 0;

    // Single-symbol edge case
    if (!root->left && !root->right) {
        for (int i = 0; i < originalLen; i++) {
            output[outPos++] = root->byte;
        }
        freeTree(root);
        return outPos;
    }

    HuffmanNode* current = root;

    for (int byteIdx = pos; byteIdx < inputLen && outPos < originalLen; byteIdx++) {
        unsigned char b = input[byteIdx];
        for (int bit = 7; bit >= 0 && outPos < originalLen; bit--) {
            current = (b >> bit) & 1 ? current->right : current->left;
            if (!current) {
                break;
            }
            if (!current->left && !current->right) {
                output[outPos++] = current->byte;
                current = root;
            }
        }
    }

    freeTree(root);
    return outPos;
}

} // extern "C"