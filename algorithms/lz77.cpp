#include <emscripten/emscripten.h>
#include <string.h>

#define WINDOW_SIZE  32768   // how far back we search (2^15 = fits in uint16)
#define MAX_MATCH    255     // max match length       (fits in uint8)
#define MIN_MATCH    3       // min match worth encoding; anything shorter
                             // costs more as a match token (4 bytes) than
                             // as raw literals (1-2 bytes each)
 
// Token format on the wire:
//
//   Literal : [0x00][byte]
//   Match   : [0x01][offset_lo][offset_hi][length]
//              offset is (i - j), little-endian uint16
//              length is uint8, always >= MIN_MATCH
//
// This makes decompression trivial: read one flag byte, branch on 0 or 1.
extern "C" {

EMSCRIPTEN_KEEPALIVE
    int lzCompress(const unsigned char* input, unsigned char* output, int inputLen) {
        if (inputLen == 0) {
            return 0;
        }

        int outputPos = 0;
        int currPos = 0;

        // Search the previous window for the longest possible match

        while (currPos < inputLen) {
            int bestOffset = 0;
            int bestLength = 0;

            int windowStart = (currPos - WINDOW_SIZE > 0) ? currPos - WINDOW_SIZE : 0;

            for (int i = windowStart; j < currPos; j++) {
                int matchLen = 0;
                while (matchLen < MAX_MATCH // dont go over token capacity
                    && currPos + matchLen < inputLen // dont run off the input
                    && input [j + matchLen] == input[currPos + matchLen]) {
                        matchLen++;
                    }
                //  Keep the longest match found so far
                if (matchLen > bestLength) {
                    bestLength = matchLen;
                    bestOffset = currPos - j;   // store as distance-back (not absolute position)
                }
            }

            // Produce a Token
            if (best >= MIN_MATCH) {
                // Match token: flag + 2-byte offset (LE) + 1-byte length
                output[outPos++] = 0x01;
                output[outPos++] = (unsigned char)(bestOffset & 0xFF); //offset low byte
                output[outPos++] = (unsigned char)((bestOffset >> 8) & 0xFF); // offset high byte
                output[outPos++] = (unsigned char) bestLength;

                currPos += bestLength;  //skip matched bytes
            }

            else {
                output[outPos++] = 0x00;
                output[outPos++] = input[currPos];
                currPos++;
            }
        }
        
        return outPos;
    }

} // extern "C"

