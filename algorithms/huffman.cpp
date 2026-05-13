#include <emscripten/emscripten.h>
#include <string.h>
#include <stdlib.h>

// Huffman tree node
struct HuffmanNode {
    unsigned char byte;
    int frequency;
    HuffmanNode* left;
    HuffmanNode* right;
};

HuffmanNode* createNode(unsigned char byte, int freq) {
    HuffmanNode* node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    node->byte = byte;
    node->frequency = freq;
    node->left = nullptr;
    node->right = nullptr;
    return node;
}

void freeTree(HuffmanNode* node) {
    if (!node) return;
    freeTree(node->left);
    freeTree(node->right);
    free(node);
}

void buildFrequencyTable(const unsigned char* input, int inputLen, int* frequencies) {
    for (int i = 0; i < 256; i++) frequencies[i] = 0;
    for (int i = 0; i < inputLen; i++) frequencies[input[i]]++;
}

void sortNodes(HuffmanNode** nodes, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (nodes[i]->frequency > nodes[j]->frequency) {
                HuffmanNode* temp = nodes[i];
                nodes[i] = nodes[j];
                nodes[j] = temp;
            }
        }
    }
}

HuffmanNode* buildHuffmanTree(int* frequencies) {
    HuffmanNode* nodes[256];
    int nodeCount = 0;

    for (int i = 0; i < 256; i++) {
        if (frequencies[i] > 0) {
            nodes[nodeCount++] = createNode((unsigned char)i, frequencies[i]);
        }
    }

    if (nodeCount == 0) return nullptr;
    if (nodeCount == 1) return nodes[0];

    while (nodeCount > 1) {
        sortNodes(nodes, nodeCount);

        HuffmanNode* left  = nodes[0];
        HuffmanNode* right = nodes[1];

        HuffmanNode* parent = createNode(0, left->frequency + right->frequency);
        parent->left  = left;
        parent->right = right;

        nodes[0] = parent;
        for (int i = 1; i < nodeCount - 1; i++) nodes[i] = nodes[i + 1];
        nodeCount--;
    }

    return nodes[0];
}

void generateCodes(HuffmanNode* node, unsigned int code, int depth,
                   unsigned int* codes, int* codeLengths) {
    if (!node) return;

    if (!node->left && !node->right) {
        codes[node->byte]       = code;
        codeLengths[node->byte] = depth == 0 ? 1 : depth; // single-symbol edge case
        return;
    }

    if (node->left)  generateCodes(node->left,  code << 1,        depth + 1, codes, codeLengths);
    if (node->right) generateCodes(node->right, (code << 1) | 1,  depth + 1, codes, codeLengths);
}

// Header layout (written by compress, read by decompress):
//   Bytes 0-3   : original input length (little-endian int32)
//   Bytes 4-515 : frequency table, 256 × 2 bytes each (little-endian int16)
//   Bytes 516-  : compressed bit-stream
#define HUFFMAN_HEADER_SIZE (4 + 256 * 2)  // 516 bytes

extern "C" {

EMSCRIPTEN_KEEPALIVE
int huffmanCompress(const unsigned char* input, unsigned char* output, int inputLen) {
    if (inputLen == 0) return 0;

    // --- frequency table ---
    int frequencies[256];
    buildFrequencyTable(input, inputLen, frequencies);

    // --- build tree & codes ---
    HuffmanNode* root = buildHuffmanTree(frequencies);
    if (!root) return 0;

    unsigned int codes[256]      = {0};
    int          codeLengths[256] = {0};
    generateCodes(root, 0, 0, codes, codeLengths);
    freeTree(root);

    // --- write header ---
    int outPos = 0;

    // original length (4 bytes, little-endian)
    output[outPos++] = (unsigned char)( inputLen        & 0xFF);
    output[outPos++] = (unsigned char)((inputLen >>  8) & 0xFF);
    output[outPos++] = (unsigned char)((inputLen >> 16) & 0xFF);
    output[outPos++] = (unsigned char)((inputLen >> 24) & 0xFF);

    // frequency table (256 × 2 bytes, little-endian int16)
    for (int i = 0; i < 256; i++) {
        int f = frequencies[i] > 0xFFFF ? 0xFFFF : frequencies[i];
        output[outPos++] = (unsigned char)( f       & 0xFF);
        output[outPos++] = (unsigned char)((f >> 8) & 0xFF);
    }

    // --- encode data ---
    unsigned int bitBuffer = 0;
    int          bitCount  = 0;

    for (int i = 0; i < inputLen; i++) {
        unsigned char b   = input[i];
        unsigned int  code    = codes[b];
        int           codeLen = codeLengths[b];

        bitBuffer = (bitBuffer << codeLen) | code;
        bitCount += codeLen;

        while (bitCount >= 8) {
            bitCount -= 8;
            output[outPos++] = (unsigned char)((bitBuffer >> bitCount) & 0xFF);
        }
    }

    // flush remaining bits (pad with zeros on the right)
    if (bitCount > 0) {
        output[outPos++] = (unsigned char)((bitBuffer << (8 - bitCount)) & 0xFF);
    }

    return outPos;
}

EMSCRIPTEN_KEEPALIVE
int huffmanDecompress(const unsigned char* input, unsigned char* output, int inputLen) {
    if (inputLen <= HUFFMAN_HEADER_SIZE) return 0;

    // --- read header ---
    int pos = 0;

    // original length
    int originalLen = (int)input[pos]
                    | ((int)input[pos + 1] <<  8)
                    | ((int)input[pos + 2] << 16)
                    | ((int)input[pos + 3] << 24);
    pos += 4;

    // frequency table
    int frequencies[256];
    for (int i = 0; i < 256; i++) {
        frequencies[i] = (int)input[pos] | ((int)input[pos + 1] << 8);
        pos += 2;
    }

    // --- rebuild tree ---
    HuffmanNode* root = buildHuffmanTree(frequencies);
    if (!root) return 0;

    // --- decode bit-stream ---
    int outPos = 0;

    // Special case: only one unique symbol in the original data.
    // The tree has a single leaf with no edges — every bit decodes to that leaf.
    if (!root->left && !root->right) {
        for (int i = 0; i < originalLen; i++) output[outPos++] = root->byte;
        freeTree(root);
        return outPos;
    }

    HuffmanNode* current = root;

    for (int byteIdx = pos; byteIdx < inputLen && outPos < originalLen; byteIdx++) {
        unsigned char b = input[byteIdx];

        for (int bit = 7; bit >= 0 && outPos < originalLen; bit--) {
            int direction = (b >> bit) & 1;
            current = direction ? current->right : current->left;

            if (!current) break; // malformed input guard

            if (!current->left && !current->right) {
                // leaf — emit symbol, go back to root
                output[outPos++] = current->byte;
                current = root;
            }
        }
    }

    freeTree(root);
    return outPos;
}

} // extern "C"