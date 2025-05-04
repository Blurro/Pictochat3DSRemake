#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

#define MODE_RAW  0
#define MODE_RLE1 1
#define MODE_RLE3 2

uint8_t mode = MODE_RAW;

void rawEncode(const std::vector<unsigned char>& inNibbles, std::vector<unsigned char>& outNibbles, size_t read_pos, int run_len) {
    for (int i = 0; i < run_len && read_pos < inNibbles.size(); ++i)
        outNibbles.push_back(inNibbles[read_pos++]);
}

void rle1Encode(const std::vector<unsigned char>& inNibbles, std::vector<unsigned char>& outNibbles, size_t read_pos, int run_len) {
    while (run_len > 0) {
        int chunk = std::min(run_len, 16);
        outNibbles.push_back(inNibbles[read_pos]); // the value nibble
        outNibbles.push_back(chunk);               // the run length (max 16)
        run_len -= chunk;
        read_pos += chunk;
    }
}

void rle3Encode(const std::vector<unsigned char>& inNibbles, std::vector<unsigned char>& outNibbles, size_t read_pos, int run_len) {
    while (run_len > 0) {
        int chunk = std::min(run_len, 4096);

        // push value nibble
        outNibbles.push_back(inNibbles[read_pos]);

        // split chunk (0–4096) into 3 nibbles: high, mid, low
        // note: chunk in range [1, 4096], so subtract 1 for 0-based encoding
        chunk--;
        outNibbles.push_back((chunk >> 8) & 0x0F); // high nibble
        outNibbles.push_back((chunk >> 4) & 0x0F); // mid nibble
        outNibbles.push_back(chunk & 0x0F);        // low nibble

        run_len -= chunk + 1; // restore back to actual size used
        read_pos += chunk + 1;
    }
}

int main() {
    std::ifstream file("canvnibbles.raw", std::ios::binary);
    if (!file) return std::cerr << "Error opening input\n", 1;

    std::vector<unsigned char> inNibbles;
    unsigned char byte;
    while (file.read(reinterpret_cast<char*>(&byte), 1)) {
        inNibbles.push_back((byte >> 4) & 0x0F);  // high nibble
        inNibbles.push_back(byte & 0x0F);         // low nibble
    }
    file.close();

    std::vector<unsigned char> outNibbles;
    int run_lengths[6] = { 0 };
    int run_index = 0, current_run = 0;
    int last = -1;

    // first 6 runs
    for (size_t i = 0; i < inNibbles.size(); i++) {
        if (last == -1 || inNibbles[i] == last) {
            current_run++;
        }
        else {
            run_lengths[run_index++] = current_run;
            current_run = 1;
            if (run_index == 6) break;
        }
        last = inNibbles[i];
    }
    if (run_index < 6) run_lengths[run_index++] = current_run;

    std::cout << "Initial Runs: ";
    for (int i = 0; i < 6; i++) std::cout << run_lengths[i] << " ";
    std::cout << std::endl;

    size_t read_pos = 0;
    size_t count_offset = 0;
    for (int i = 0; i < 6; ++i) {
        count_offset += run_lengths[i];
    }
    std::cout << "initial count=" << count_offset << std::endl;

    // loop calling rawEncode until done
    while (read_pos < inNibbles.size())
    {
        uint8_t lastMode = mode;
        int r0 = run_lengths[0], r1 = run_lengths[1], r2 = run_lengths[2];
        int r3 = run_lengths[3], r4 = run_lengths[4], r5 = run_lengths[5];
        if (mode == MODE_RAW) {
            bool switchToRLE = false;
            // switch to rle if next 3 runs all > 2
            if (r0 > 2 && r1 > 2 && r2 > 2) switchToRLE = true;
            // OR next 2 runs both > 2 and one > 3
            if (r0 > 2 && r1 > 2 && (r0 > 3 || r1 > 3)) switchToRLE = true;
            // OR next 1 run > 4
            if (r0 > 4) switchToRLE = true;
            // if we're switching, decide which RLE
            if (switchToRLE) {
                if ((r0 > 16 && r1 > 16 && r2 > 16 && (r0 > 32 || r1 > 32 || r2 > 32)) || r0 > 32) {
                    mode = MODE_RLE3;
                    outNibbles.push_back(0xF); // raw's code is F, switch to RLE3 using it (rle3 doesnt have its own)
                }
                else {
                    mode = MODE_RLE1;
                    outNibbles.push_back(0xE); // rle1's code is E
                }
            }
        }
        else { // RLE1 OR RLE3
            int below3 = 0, exactly1 = 0;
            int runCheck[6] = { r0, r1, r2, r3, r4, r5 };
            for (int i = 0; i < 6; i++) {
                if (runCheck[i] < 3) below3++;
                if (runCheck[i] == 1) exactly1++;
            }

            if (mode == MODE_RLE1) {
                if (below3 == 6 && exactly1 >= 3) { 
                    mode = MODE_RAW;
                    outNibbles.push_back(0xF); // raw's code is F
                }
                else if ((r0 > 32 && r1 > 32) || r0 > 48) {
                    mode = MODE_RLE3;
                    outNibbles.push_back(0xE); // rle1's code is E, switch to rle3 using E
                }
            }
            else { // IF RLE3
                if (below3 == 6 && exactly1 >= 1) {
                    mode = MODE_RAW;
                    outNibbles.push_back(0xF); // raw's code is F
                }
                else if (r0 < 16 && r1 < 16) {
                    mode = MODE_RLE1;
                    outNibbles.push_back(0xE); // rle1's code is E
                }
            }
        }
        if (lastMode != mode) {
            std::cout << "last mode: " << std::dec << (int)lastMode << ", new mode: " << (int)mode << std::endl;
        }
        std::cout << "read pos: " << read_pos << ", read for: " << run_lengths[0] << std::endl;

        if (mode == MODE_RAW) {
            rawEncode(inNibbles, outNibbles, read_pos, r0);
        }
        else if (mode == MODE_RLE1) {
            rle1Encode(inNibbles, outNibbles, read_pos, r0);
        }
        else {
            rle3Encode(inNibbles, outNibbles, read_pos, r0);
        }
        read_pos += r0;

        // shift run lengths down
        for (int i = 0; i < 5; i++) run_lengths[i] = run_lengths[i + 1];

        int len = 0;
        // update length for however many nibbles in a row are the same
        if (count_offset < inNibbles.size()) {
            unsigned char val = inNibbles[count_offset]; // nibble to compare to
            while (count_offset + len < inNibbles.size() && inNibbles[count_offset + len] == val) {
                len++;
            }
            count_offset += len;
            run_lengths[5] = len;
        }
        else {
            run_lengths[5] = 0;
        }
    }

    std::ofstream outFile("nibbles_out.raw", std::ios::binary);
    if (!outFile) return std::cerr << "Error opening output\n", 1;

    // write out nibbles as bytes (every 2 nibbles packed into 1 byte)
    for (size_t i = 0; i < outNibbles.size(); i += 2) {
        unsigned char b = outNibbles[i] << 4;
        if (i + 1 < outNibbles.size())
            b |= outNibbles[i + 1];
        else
            b |= 0x0F;
        outFile.write(reinterpret_cast<char*>(&b), 1);
    }

    outFile.close();
    return 0;
}