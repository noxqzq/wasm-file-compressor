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

// Create a new node
HuffmanNode* createNode(unsigned char byte, int freq) {
    HuffmanNode* node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    node->byte = byte;
    node->frequency = freq;
    node->left = nullptr;
    node->right = nullptr;
    return node;
}

// Build frequency table
void buildFrequencyTable(const unsigned char* input, int inputLen, int* frequencies) {
    for (int i = 0; i < 256; i++) {
        frequencies[i] = 0;
    }
    for (int i = 0; i < inputLen; i++) {
        frequencies[input[i]]++;
    }
}

// Simple priority queue using array (for simplicity)
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

// Build Huffman tree
HuffmanNode* buildHuffmanTree(int* frequencies) {
    HuffmanNode* nodes[256];
    int nodeCount = 0;
    
    // Create leaf nodes for each byte that appears
    for (int i = 0; i < 256; i++) {
        if (frequencies[i] > 0) {
            nodes[nodeCount++] = createNode(i, frequencies[i]);
        }
    }
    
    if (nodeCount == 0) return nullptr;
    if (nodeCount == 1) return nodes[0];
    
    // Build tree bottom-up
    while (nodeCount > 1) {
        sortNodes(nodes, nodeCount);
        
        // Take two nodes with lowest frequency
        HuffmanNode* left = nodes[0];
        HuffmanNode* right = nodes[1];
        
        // Create parent node
        HuffmanNode* parent = createNode(0, left->frequency + right->frequency);
        parent->left = left;
        parent->right = right;
        
        // Replace first two with parent
        nodes[0] = parent;
        for (int i = 1; i < nodeCount - 1; i++) {
            nodes[i] = nodes[i + 1];
        }
        nodeCount--;
    }
    
    return nodes[0];
}

// Generate codes from tree
void generateCodes(HuffmanNode* node, unsigned int code, int depth, 
                   unsigned int* codes, int* codeLengths) {
    if (!node) return;
    
    // Leaf node - store code
    if (!node->left && !node->right) {
        codes[node->byte] = code;
        codeLengths[node->byte] = depth;
        return;
    }
    
    // Traverse left (add 0) and right (add 1)
    if (node->left) generateCodes(node->left, code << 1, depth + 1, codes, codeLengths);
    if (node->right) generateCodes(node->right, (code << 1) | 1, depth + 1, codes, codeLengths);
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    int huffmanCompress(const unsigned char* input, unsigned char* output, int inputLen) {
        if (inputLen == 0) return 0;
        
        // Build frequency table
        int frequencies[256];
        buildFrequencyTable(input, inputLen, frequencies);
        
        // Build Huffman tree
        HuffmanNode* root = buildHuffmanTree(frequencies);
        if (!root) return 0;
        
        // Generate codes
        unsigned int codes[256] = {0};
        int codeLengths[256] = {0};
        generateCodes(root, 0, 0, codes, codeLengths);
        
        // Write frequency table to output (for decompression)
        int outPos = 0;
        for (int i = 0; i < 256; i++) {
            output[outPos++] = frequencies[i] & 0xFF;
            output[outPos++] = (frequencies[i] >> 8) & 0xFF;
        }
        
        // Encode input using Huffman codes
        unsigned int bitBuffer = 0;
        int bitCount = 0;
        
        for (int i = 0; i < inputLen; i++) {
            unsigned char byte = input[i];
            unsigned int code = codes[byte];
            int codeLen = codeLengths[byte];
            
            bitBuffer = (bitBuffer << codeLen) | code;
            bitCount += codeLen;
            
            while (bitCount >= 8) {
                bitCount -= 8;
                output[outPos++] = (bitBuffer >> bitCount) & 0xFF;
            }
        }
        
        // Write remaining bits
        if (bitCount > 0) {
            output[outPos++] = (bitBuffer << (8 - bitCount)) & 0xFF;
        }
        
        return outPos;
    }
    
    EMSCRIPTEN_KEEPALIVE
    int huffmanDecompress(const unsigned char* input, unsigned char* output, int inputLen) {
        // TODO: Implement decompression
        // Read frequency table, rebuild tree, decode bits
        return 0;
    }
}
