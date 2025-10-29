// multimedia_steg.cpp
// Single-file C++ program implementing:
// 1) text -> BMP (black bg, white text)
// 2) BMP -> encode payload into WAV (LSB of 16-bit samples)
// 3) WAV -> generate waveform PNG (and copy payload bits into PNG LSBs)
// 4) PNG waveform -> decode payload -> save txt
//
// No external libraries required: stb_image_write and stb_image included below (single header).
// Compile: g++ -std=c++17 -O2 multimedia_steg.cpp -o multimedia_steg

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <utility>
using namespace std;
#include <thread>
#include <chrono>
#include <cctype>
#ifdef _WIN32
// avoid including windows.h due to name conflicts with std::byte in some toolchains
#endif
#ifdef _WIN32
// Minimal declarations for dynamic calls to kernel32 (avoid windows.h)
extern "C" void* __stdcall LoadLibraryA(const char* lpLibFileName);
extern "C" void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
extern "C" int   __stdcall FreeLibrary(void* hModule);
#endif
#ifndef _WIN32
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <conio.h> // for _getch() to mask password input on Windows
#endif
#ifdef _WIN32
#include <io.h>
#endif
// filesystem used to help list files when a user-provided BMP is not found
#include <filesystem>

/* -------------------------
   Minimal 8x8 bitmap font (printable ASCII 32..126)
   Each character is 8 bytes; bit = 1 means pixel on.
   We'll include a small font for ASCII 32..127 (space..DEL).
   For brevity I include a basic font covering common characters.
   You can expand later.
---------------------------*/

// A very small 8x8 font (partial but covers letters, digits, punctuation).
// This font was adapted from public domain tiny fonts for demonstration.
static const unsigned char tiny8x8_font[96][8] = {
    // 32 ' '
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 33 '!'
    {0x18,0x3c,0x3c,0x18,0x18,0x00,0x18,0x00},
    // 34 '"'
    {0x6c,0x6c,0x48,0x00,0x00,0x00,0x00,0x00},
    // 35 '#'
    {0x6c,0x6c,0xfe,0x6c,0xfe,0x6c,0x6c,0x00},
    // 36 '$'
    {0x18,0x3e,0x58,0x3c,0x1a,0x7c,0x18,0x00},
    // 37 '%'
    {0x00,0xc6,0xcc,0x18,0x30,0x66,0xc6,0x00},
    // 38 '&'
    {0x38,0x6c,0x38,0x76,0xdc,0xcc,0x76,0x00},
    // 39 '''
    {0x30,0x30,0x60,0x00,0x00,0x00,0x00,0x00},
    // 40 '('
    {0x0c,0x18,0x30,0x30,0x30,0x18,0x0c,0x00},
    // 41 ')'
    {0x30,0x18,0x0c,0x0c,0x0c,0x18,0x30,0x00},
    // 42 '*'
    {0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00},
    // 43 '+'
    {0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00},
    // 44 ','
    {0x00,0x00,0x00,0x00,0x30,0x30,0x60,0x00},
    // 45 '-'
    {0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00},
    // 46 '.'
    {0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00},
    // 47 '/'
    {0x06,0x0c,0x18,0x30,0x60,0xc0,0x80,0x00},
    // 48 '0'
    {0x7c,0xc6,0xce,0xd6,0xe6,0xc6,0x7c,0x00},
    // 49 '1'
    {0x30,0x70,0x30,0x30,0x30,0x30,0xfc,0x00},
    // 50 '2'
    {0x78,0xcc,0x0c,0x38,0x60,0xcc,0xfc,0x00},
    // 51 '3'
    {0x78,0xcc,0x0c,0x38,0x0c,0xcc,0x78,0x00},
    // 52 '4'
    {0x1c,0x3c,0x6c,0xcc,0xfe,0x0c,0x1e,0x00},
    // 53 '5'
    {0xfc,0xc0,0xf8,0x0c,0x0c,0xcc,0x78,0x00},
    // 54 '6'
    {0x38,0x60,0xc0,0xf8,0xcc,0xcc,0x78,0x00},
    // 55 '7'
    {0xfc,0xcc,0x0c,0x18,0x30,0x30,0x30,0x00},
    // 56 '8'
    {0x78,0xcc,0xcc,0x78,0xcc,0xcc,0x78,0x00},
    // 57 '9'
    {0x78,0xcc,0xcc,0x7c,0x0c,0x18,0x70,0x00},
    // 58 ':'
    {0x00,0x30,0x30,0x00,0x00,0x30,0x30,0x00},
    // 59 ';'
    {0x00,0x30,0x30,0x00,0x00,0x30,0x30,0x60},
    // 60 '<'
    {0x0c,0x18,0x30,0x60,0x30,0x18,0x0c,0x00},
    // 61 '='
    {0x00,0x00,0x7e,0x00,0x00,0x7e,0x00,0x00},
    // 62 '>'
    {0x30,0x18,0x0c,0x06,0x0c,0x18,0x30,0x00},
    // 63 '?'
    {0x78,0xcc,0x0c,0x18,0x30,0x00,0x30,0x00},
    // 64 '@'
    {0x7c,0xc6,0xde,0xde,0xde,0xc0,0x78,0x00},
    // 65 'A'
    {0x30,0x78,0xcc,0xcc,0xfc,0xcc,0xcc,0x00},
    // 66 'B'
    {0xf8,0xcc,0xcc,0xf8,0xcc,0xcc,0xf8,0x00},
    // 67 'C'
    {0x78,0xcc,0xc0,0xc0,0xc0,0xcc,0x78,0x00},
    // 68 'D'
    {0xf0,0xd8,0xcc,0xcc,0xcc,0xd8,0xf0,0x00},
    // 69 'E'
    {0xfc,0xc0,0xc0,0xf8,0xc0,0xc0,0xfc,0x00},
    // 70 'F'
    {0xfc,0xc0,0xc0,0xf8,0xc0,0xc0,0xc0,0x00},
    // 71 'G'
    {0x78,0xcc,0xc0,0xdc,0xcc,0xcc,0x78,0x00},
    // 72 'H'
    {0xcc,0xcc,0xcc,0xfc,0xcc,0xcc,0xcc,0x00},
    // 73 'I'
    {0x78,0x30,0x30,0x30,0x30,0x30,0x78,0x00},
    // 74 'J'
    {0x3c,0x18,0x18,0x18,0x18,0xd8,0x70,0x00},
    // 75 'K'
    {0xcc,0xd8,0xf0,0xe0,0xf0,0xd8,0xcc,0x00},
    // 76 'L'
    {0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xfc,0x00},
    // 77 'M'
    {0xc6,0xee,0xfe,0xd6,0xc6,0xc6,0xc6,0x00},
    // 78 'N'
    {0xc6,0xe6,0xf6,0xde,0xce,0xc6,0xc6,0x00},
    // 79 'O'
    {0x78,0xcc,0xcc,0xcc,0xcc,0xcc,0x78,0x00},
    // 80 'P'
    {0xf8,0xcc,0xcc,0xf8,0xc0,0xc0,0xc0,0x00},
    // 81 'Q'
    {0x78,0xcc,0xcc,0xcc,0xd4,0xc8,0x74,0x00},
    // 82 'R'
    {0xf8,0xcc,0xcc,0xf8,0xe0,0xd8,0xcc,0x00},
    // 83 'S'
    {0x78,0xcc,0xc0,0x78,0x0c,0xcc,0x78,0x00},
    // 84 'T'
    {0xfc,0x30,0x30,0x30,0x30,0x30,0x30,0x00},
    // 85 'U'
    {0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0x78,0x00},
    // 86 'V'
    {0xcc,0xcc,0xcc,0xcc,0xcc,0x78,0x30,0x00},
    // 87 'W'
    {0xc6,0xc6,0xc6,0xd6,0xfe,0xee,0xc6,0x00},
    // 88 'X'
    {0xc6,0xc6,0x6c,0x38,0x6c,0xc6,0xc6,0x00},
    // 89 'Y'
    {0xcc,0xcc,0xcc,0x78,0x30,0x30,0x30,0x00},
    // 90 'Z'
    {0xfc,0x8c,0x18,0x30,0x60,0x66,0xfc,0x00},
    // 91 '['
    {0x78,0x60,0x60,0x60,0x60,0x60,0x78,0x00},
    // 92 '\'
    {0xc0,0x60,0x30,0x18,0x0c,0x06,0x02,0x00},
    // 93 ']'
    {0x78,0x18,0x18,0x18,0x18,0x18,0x78,0x00},
    // 94 '^'
    {0x10,0x38,0x6c,0xc6,0x00,0x00,0x00,0x00},
    // 95 '_'
    {0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00},
    // 96 '`'
    {0x30,0x18,0x0c,0x00,0x00,0x00,0x00,0x00},
    // 97 'a'
    {0x00,0x00,0x78,0x0c,0x7c,0xcc,0x76,0x00},
    // 98 'b'
    {0xe0,0x60,0x6c,0x76,0x6c,0x6c,0xf8,0x00},
    // 99 'c'
    {0x00,0x00,0x78,0xcc,0xc0,0xcc,0x78,0x00},
    // 100 'd'
    {0x1c,0x0c,0x7c,0xcc,0xcc,0xcc,0x76,0x00},
    // 101 'e'
    {0x00,0x00,0x78,0xcc,0xfc,0xc0,0x78,0x00},
    // 102 'f'
    {0x38,0x6c,0x60,0xf8,0x60,0x60,0xf0,0x00},
    // 103 'g'
    {0x00,0x00,0x76,0xcc,0xcc,0x7c,0x0c,0xf8},
    // 104 'h'
    {0xe0,0x60,0x6c,0x76,0x6c,0x6c,0x6c,0x00},
    // 105 'i'
    {0x30,0x00,0x70,0x30,0x30,0x30,0x78,0x00},
    // 106 'j'
    {0x0c,0x00,0x1c,0x0c,0x0c,0xcc,0xcc,0x78},
    // 107 'k'
    {0xe0,0x60,0x66,0x6c,0x78,0x6c,0x66,0x00},
    // 108 'l'
    {0x70,0x30,0x30,0x30,0x30,0x30,0x78,0x00},
    // 109 'm'
    {0x00,0x00,0xec,0xfe,0xd6,0xd6,0xd6,0x00},
    // 110 'n'
    {0x00,0x00,0xdc,0x66,0x66,0x66,0x66,0x00},
    // 111 'o'
    {0x00,0x00,0x78,0xcc,0xcc,0xcc,0x78,0x00},
    // 112 'p'
    {0x00,0x00,0xf8,0x6c,0x6c,0x78,0x60,0xf0},
    // 113 'q'
    {0x00,0x00,0x76,0xcc,0xcc,0x7c,0x0c,0x1e},
    // 114 'r'
    {0x00,0x00,0xdc,0x76,0x60,0x60,0xf0,0x00},
    // 115 's'
    {0x00,0x00,0x7c,0xc0,0x78,0x0c,0xf8,0x00},
    // 116 't'
    {0x30,0x30,0xfc,0x30,0x30,0x34,0x18,0x00},
    // 117 'u'
    {0x00,0x00,0xcc,0xcc,0xcc,0xcc,0x76,0x00},
    // 118 'v'
    {0x00,0x00,0xcc,0xcc,0xcc,0x78,0x30,0x00},
    // 119 'w'
    {0x00,0x00,0xc6,0xd6,0xfe,0x6c,0x6c,0x00},
    // 120 'x'
    {0x00,0x00,0xc6,0x6c,0x38,0x6c,0xc6,0x00},
    // 121 'y'
    {0x00,0x00,0xcc,0xcc,0xcc,0x7e,0x0c,0xf8},
    // 122 'z'
    {0x00,0x00,0xfc,0x8c,0x18,0x32,0xfc,0x00},
    // 123 '{'
    {0x1c,0x30,0x30,0x60,0x30,0x30,0x1c,0x00},
    // 124 '|'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},
    // 125 '}'
    {0x70,0x18,0x18,0x0c,0x18,0x18,0x70,0x00},
    // 126 '~'
    {0x76,0xdc,0x00,0x00,0x00,0x00,0x00,0x00},
    // 127 DEL (unused)
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};

// Utility: clamp
static inline int clampi(int v, int a, int b){ return v < a ? a : (v > b ? b : v); }

// portable case-insensitive equals for extensions and simple comparisons
static inline bool iequals(const string &a, const string &b){
    if(a.size() != b.size()) return false;
    for(size_t i=0;i<a.size();++i) if(std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

/* -------------------------
   BMP write (24-bit) and read
   We'll implement simple BMP writer for RGB24 uncompressed.
---------------------------*/
#pragma pack(push,1)
struct BMPFileHeader {
    uint16_t bfType; // 'BM'
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
};
struct BMPInfoHeader {
    uint32_t biSize; // 40
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};
#pragma pack(pop)

bool writeBMP24(const string &filename, int w, int h, const vector<uint8_t> &rgb) {
    // rgb: row-major top-to-bottom, each pixel 3 bytes (R,G,B)
    // BMP expects BGR and rows bottom-to-top with padding
    // Write to a temporary file first, then rename to final filename to avoid leaving a corrupted file on interruption.
    string tmpfn = filename + ".tmp";
    FILE *f = fopen(tmpfn.c_str(), "wb");
    if(!f) return false;
    int rowBytes = ((w*3 + 3)/4)*4;
    int imgSize = rowBytes * h;
    BMPFileHeader fh;
    BMPInfoHeader ih;
    fh.bfType = 0x4D42; // 'BM'
    fh.bfSize = sizeof(fh) + sizeof(ih) + imgSize;
    fh.bfReserved1 = 0; fh.bfReserved2 = 0;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    ih.biSize = 40;
    ih.biWidth = w;
    ih.biHeight = h;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = 0;
    ih.biSizeImage = imgSize;
    ih.biXPelsPerMeter = 2835;
    ih.biYPelsPerMeter = 2835;
    ih.biClrUsed = 0;
    ih.biClrImportant = 0;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    vector<uint8_t> pad(rowBytes - w*3);
    for(int y = h-1; y >= 0; --y) {
        for(int x = 0; x < w; ++x) {
            size_t idx = (y*w + x)*3; // our rgb is top-to-bottom; but we loop bottom-up so this gives top->bottom reversed
            uint8_t r = rgb[idx+0];
            uint8_t g = rgb[idx+1];
            uint8_t b = rgb[idx+2];
            uint8_t pixel[3] = { b, g, r };
            fwrite(pixel, 1, 3, f);
        }
        if(!pad.empty()) fwrite(pad.data(), 1, pad.size(), f);
    }
    fclose(f);
    // replace target atomically
    // remove existing target if present
    remove(filename.c_str());
    int rv = rename(tmpfn.c_str(), filename.c_str());
    if(rv != 0) {
        // failed to rename; cleanup tmp and report failure
        remove(tmpfn.c_str());
        return false;
    }
    return true;
}

bool readAllFile(const string &path, vector<uint8_t> &out) {
    FILE *f = fopen(path.c_str(),"rb");
    if(!f) return false;
    fseek(f,0,SEEK_END);
    long s = ftell(f);
    fseek(f,0,SEEK_SET);
    if(s < 0) { fclose(f); return false; }
    out.resize(s);
    if(s>0) fread(out.data(),1,s,f);
    fclose(f);
    return true;
}

/* -------------------------
   Simple WAV I/O (16-bit PCM mono)
---------------------------*/
#pragma pack(push,1)
struct WAVHeader {
    char riff[4]; // "RIFF"
    uint32_t overall_size;
    char wave[4]; // "WAVE"
    char fmt_chunk_marker[4]; // "fmt "
    uint32_t length_of_fmt; // 16
    uint16_t format_type; // 1 for PCM
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byterate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_chunk_header[4]; // "data"
    uint32_t data_size;
};
#pragma pack(pop)

bool writeWAV_LSBCarrier(const string &filename, const vector<uint8_t> &payload, int sample_rate = 44100) {
    // payload: raw bytes to embed into LSBs of samples
    // We'll write 32-bit length (uint32 little-endian) then payload bytes,
    // one bit per sample LSB. We'll create enough samples; other bits 0 => silence.
    uint32_t payload_len = (uint32_t)payload.size();
    vector<uint8_t> buffer;
    buffer.reserve(4 + payload_len);
    for(int i=0;i<4;i++) buffer.push_back((payload_len >> (8*i)) & 0xFF);
    buffer.insert(buffer.end(), payload.begin(), payload.end());
    uint32_t total_bits = buffer.size() * 8;
    // We'll keep 1 sample per bit (extraction reads one sample per bit). To make the WAV audible
    // produce a continuous sine-wave carrier and then set each sample's LSB to the payload bit.
    uint32_t num_samples = total_bits; // 1 bit per sample (extraction expects this)
    vector<int16_t> samples;
    samples.resize(num_samples);
    const double two_pi = 6.28318530717958647692;
    double freq = 1000.0; // carrier frequency in Hz (audible)
    double amplitude = 20000.0; // amplitude of the carrier (fits in int16)
    uint32_t bitIndex = 0;
    for(size_t i=0;i<buffer.size();++i) {
        uint8_t b = buffer[i];
        for(int bit=0; bit<8; ++bit, ++bitIndex) {
            int bitval = (b >> bit) & 1;
            // generate base carrier sample
            size_t sample_i = bitIndex;
            double t = (double)sample_i / (double)sample_rate;
            double s = amplitude * sin(two_pi * freq * t);
            int16_t base = (int16_t)llround(s);
            // set LSB according to bitval
            int16_t final_sample = (int16_t)((base & ~1) | (bitval & 1));
            samples[bitIndex] = final_sample;
        }
    }
    // Prepare WAV header
    WAVHeader wh;
    memcpy(wh.riff, "RIFF", 4);
    memcpy(wh.wave, "WAVE", 4);
    memcpy(wh.fmt_chunk_marker, "fmt ", 4);
    wh.length_of_fmt = 16;
    wh.format_type = 1;
    wh.channels = 1;
    wh.sample_rate = sample_rate;
    wh.bits_per_sample = 16;
    wh.block_align = (wh.channels * wh.bits_per_sample) / 8;
    wh.byterate = wh.sample_rate * wh.block_align;
    memcpy(wh.data_chunk_header, "data", 4);
    wh.data_size = num_samples * sizeof(int16_t);
    wh.overall_size = wh.data_size + sizeof(WAVHeader) - 8;
    FILE *f = fopen(filename.c_str(), "wb");
    if(!f) return false;
    fwrite(&wh, sizeof(wh), 1, f);
    // Write samples (little endian)
    for(uint32_t i=0;i<num_samples;++i) {
        int16_t s = samples[i];
        fwrite(&s, sizeof(s), 1, f);
    }
    fclose(f);
    return true;
}

bool readWAV_samples(const string &filename, vector<int16_t> &out_samples, int &sample_rate) {
    vector<uint8_t> data;
    if(!readAllFile(filename, data)) return false;
    if(data.size() < sizeof(WAVHeader)) return false;
    WAVHeader wh;
    memcpy(&wh, data.data(), sizeof(WAVHeader));
    if(strncmp(wh.riff,"RIFF",4) != 0 || strncmp(wh.wave,"WAVE",4) != 0) return false;
    sample_rate = wh.sample_rate;
    size_t dataPos = sizeof(WAVHeader);
    size_t datasz = wh.data_size;
    if(dataPos + datasz > data.size()) datasz = (uint32_t)(data.size() - dataPos);
    size_t num_samples = datasz / sizeof(int16_t);
    out_samples.resize(num_samples);
    memcpy(out_samples.data(), data.data()+dataPos, num_samples * sizeof(int16_t));
    return true;
}

bool extractPayloadFromWAV_LSB(const string &wavfile, vector<uint8_t> &payload) {
    vector<int16_t> samples;
    int sr;
    if(!readWAV_samples(wavfile, samples, sr)) return false;
    if(samples.empty()) return false;
    // Read first 32 bits to get length (assemble bitwise little-endian)
    if(samples.size() < 32) return false;
    auto get_bit = [&](size_t bitIndex)->uint8_t {
        if(bitIndex >= samples.size()) return 0;
        return (uint8_t)(samples[bitIndex] & 1);
    };
    if(samples.size() < 32) return false;
    uint32_t payload_len = 0;
    for(size_t b=0; b<32; ++b) {
        uint8_t bit = get_bit(b);
        payload_len |= (uint32_t)bit << (b);
    }
    size_t total_bits_needed = (size_t)payload_len * 8;
    if(32 + total_bits_needed > samples.size()) {
        // Not enough bits
        return false;
    }
    payload.clear();
    payload.resize(payload_len);
    for(size_t i=0;i<payload_len;++i) {
        uint8_t byte = 0;
        for(int bit=0; bit<8; ++bit) {
            size_t bi = 32 + i*8 + bit;
            byte |= (get_bit(bi) << bit);
        }
        payload[i] = byte;
    }
    return true;
}

/* -------------------------
   Waveform image generation
   We'll use stb_image_write to write PNG.
   During generation we'll copy payload bits (if any) into pixel LSB (blue channel LSB).
---------------------------*/

/*
 * stb_image_write - v1.16 - public domain/MIT-style
 * We'll include implementation here (only the PNG writer is used). For brevity we include the single-file header.
 *
 * NOTE: This is the minimal integrated version of stb_image_write.h (public domain). It's longish but still a single-file.
 */

/* === Begin: stb_image_write.h implementation (minified for png) === */
/*
  For brevity in this answer I include a small subset of stb_image_write that allows writing PNG via stbi_write_png.
  I will embed the original public-domain header's implementation macros and provide stbi_write_png function.
  (In production you'd keep stb_image_write.h separate; including it here keeps the program self-contained.)
*/
#define STB_IMAGE_WRITE_IMPLEMENTATION
// Minimal subset to support PNG writing (using a tiny PNG encoder).
// For reliability and brevity we instead implement a very small raw-PNG writer supporting 8-bit RGB.
// This is simpler than including the full stb implementation text in this reply.
// We'll implement a tiny PNG writer using zlib-less uncompressed IDAT (not optimal but valid PNG with no compression).
//
// WARNING: This tiny PNG writer creates valid PNGs using store/none compression (no compression). That is larger but simple.

static inline uint32_t crc32_for_bytes(const unsigned char *s, size_t l) {
    static uint32_t crc_table[256];
    static bool inited = false;
    if(!inited){
        inited = true;
        for(int i=0;i<256;i++){
            uint32_t c = (uint32_t)i;
            for(int j=0;j<8;j++){
                if(c & 1) c = 0xedb88320L ^ (c >> 1);
                else c = c >> 1;
            }
            crc_table[i] = c;
        }
    }
    uint32_t c = 0xffffffffu;
    for(size_t i=0;i<l;i++) c = crc_table[(c ^ s[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

static inline void write_be32(vector<uint8_t> &out, uint32_t v){
    out.push_back((v>>24)&0xFF);
    out.push_back((v>>16)&0xFF);
    out.push_back((v>>8)&0xFF);
    out.push_back((v)&0xFF);
}
static inline void write_be16(vector<uint8_t> &out, uint16_t v){
    out.push_back((v>>8)&0xFF);
    out.push_back((v)&0xFF);
}

// Tiny PNG writer: writes 8-bit RGB PNG, no compression (store), filter type 0.
bool writePNG_raw(const string &filename, int w, int h, const vector<uint8_t> &rgb) {
    // rgb: top-to-bottom, row-major, 3 bytes per pixel
    vector<uint8_t> png;
    // PNG signature
    const unsigned char sig[8] = {137,80,78,71,13,10,26,10};
    png.insert(png.end(), sig, sig+8);
    // IHDR
    vector<uint8_t> ihdr;
    write_be32(ihdr, (uint32_t)w);
    write_be32(ihdr, (uint32_t)h);
    ihdr.push_back(8); // bit depth
    ihdr.push_back(2); // color type RGB
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace
    // write IHDR chunk: length, type, data, CRC (CRC covers type+data)
    write_be32(png, (uint32_t)ihdr.size());
    size_t pos = png.size();
    const char ihdr_type[4] = {'I','H','D','R'};
    png.insert(png.end(), ihdr_type, ihdr_type+4);
    png.insert(png.end(), ihdr.begin(), ihdr.end());
    uint32_t crc = crc32_for_bytes(png.data()+pos, 4 + ihdr.size());
    write_be32(png, crc);
    // IDAT: create uncompressed DEFLATE blocks (no compression)
    // We'll construct zlib wrapper with CMF/FLG and a single stored block per row.
    vector<uint8_t> idat;
    // zlib header: CMF (0x78) and FLG with no compression - but to be safe use 0x01 (fast) or 0x9C; using 0x78 0x01 is fine if no preset.
    idat.push_back(0x78);
    idat.push_back(0x01);
    // Uncompressed DEFLATE blocks:
    // For each scanline we add a stored block: [BFINAL|BTYPE][LEN][~LEN][data...]
    // But stored blocks have max length 65535; we'll break if needed.
    // Build raw data: each scanline starts with filter byte 0 then pixels
    vector<uint8_t> raw;
    raw.reserve((size_t)h * (w*3+1));
    for(int y=0;y<h;++y){
        raw.push_back(0); // filter 0
        size_t rowStart = (size_t)y * (size_t)w * 3;
        raw.insert(raw.end(), rgb.begin()+rowStart, rgb.begin()+rowStart + w*3);
    }
    size_t remaining = raw.size();
    size_t rp = 0;
    while(remaining > 0) {
        size_t chunk = remaining < 65535 ? remaining : 65535;
        uint8_t bfinal = (remaining <= 65535) ? 1 : 0;
        idat.push_back((uint8_t)(bfinal)); // BFINAL=1/0, BTYPE=00 stored
        // LEN and NLEN (little endian)
        uint16_t len = (uint16_t)chunk;
        idat.push_back((uint8_t)(len & 0xFF));
        idat.push_back((uint8_t)((len>>8)&0xFF));
        uint16_t nlen = ~len;
        idat.push_back((uint8_t)(nlen & 0xFF));
        idat.push_back((uint8_t)((nlen>>8)&0xFF));
        // data
        idat.insert(idat.end(), raw.begin()+rp, raw.begin()+rp+chunk);
        rp += chunk;
        remaining -= chunk;
    }
    // Adler-32 checksum for zlib:
    // Compute adler32 of raw
    uint32_t a = 1, b = 0;
    for(size_t i=0;i<raw.size();++i){
        a = (a + (uint8_t)raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    uint32_t adler = (b << 16) | a;
    // append adler32 big-endian
    idat.push_back((adler>>24)&0xFF);
    idat.push_back((adler>>16)&0xFF);
    idat.push_back((adler>>8)&0xFF);
    idat.push_back((adler)&0xFF);
    // write IDAT chunk: length, type, data, CRC
    write_be32(png, (uint32_t)idat.size());
    pos = png.size();
    const char idat_type[4] = {'I','D','A','T'};
    png.insert(png.end(), idat_type, idat_type+4);
    png.insert(png.end(), idat.begin(), idat.end());
    crc = crc32_for_bytes(png.data()+pos, 4 + idat.size());
    write_be32(png, crc);
    // IEND chunk: zero-length data
    write_be32(png, 0);
    pos = png.size();
    const char iendstr[4] = {'I','E','N','D'};
    png.insert(png.end(), iendstr, iendstr+4);
    crc = crc32_for_bytes((const unsigned char*)(&png[pos]), 4);
    write_be32(png, crc);
    // write to file
    FILE *f = fopen(filename.c_str(), "wb");
    if(!f) return false;
    fwrite(png.data(), 1, png.size(), f);
    fclose(f);
    return true;
}

/* === End tiny PNG writer === */

/* -------------------------
   Waveform generation and embedding payload bits into PNG LSBs
---------------------------*/
// forward-declare the PNG reader used for diagnostics (implemented later)
bool readPNG_extractRGB(const string &filename, int &W, int &H, vector<uint8_t> &outRGB);

bool generateWaveformPNGWithPayload(const string &wavfile, const string &pngfile) {
    vector<int16_t> samples;
    int sr;
    if(!readWAV_samples(wavfile, samples, sr)) {
        cerr << "Failed to read WAV samples or unsupported WAV format.\n";
        return false;
    }
    if(samples.empty()) {
        cerr << "WAV has no samples.\n";
        return false;
    }
    // Extract payload bits from WAV LSBs (if any) but also copy them to use in image
    // We'll attempt to read header length (32 bits) first
    auto get_bit = [&](size_t bitIndex)->uint8_t {
        if(bitIndex >= samples.size()) return 0;
        return (uint8_t)(samples[bitIndex] & 1);
    };
    // If WAV contains fewer than 32 bits, no payload
    vector<uint8_t> payload;
    bool wavHasPayload = false;
    if(samples.size() >= 32) {
        uint32_t payload_len = 0;
        for(size_t b=0;b<32;++b) payload_len |= (uint32_t)get_bit(b) << b;
        if(payload_len > 0 && 32 + (size_t)payload_len*8 <= samples.size()) {
            wavHasPayload = true;
            payload.resize(payload_len);
            for(size_t i=0;i<payload_len;++i) {
                uint8_t byte = 0;
                for(int bit=0; bit<8; ++bit) {
                    size_t bi = 32 + i*8 + bit;
                    byte |= (get_bit(bi) << bit);
                }
                payload[i] = byte;
            }
            cout << "Found payload in WAV (" << payload_len << " bytes). It will be copied into PNG LSBs.\n";
        } else {
            cout << "No payload found in WAV or not enough bits.\n";
        }
    }
    // Prepare image size
    int W = 1400;
    int H = 400;
    vector<uint8_t> img(W * H * 3);
    // Fill background black
    fill(img.begin(), img.end(), 0);
    // Draw waveform (mono) center line at H/2
    int cx = H/2;
    // We'll sample down the audio to W points
    size_t N = samples.size();
    for(int x=0;x<W;++x) {
        size_t idx = (size_t)((double)x / W * N);
        if(idx >= N) idx = N-1;
        double sample = samples[idx] / 32768.0;
        int y = (int)( (0.5 - sample*0.45) * H ); // scale
        if(y<0) y=0; if(y>=H) y=H-1;
        // draw vertical line thickness 2
        for(int t=-2;t<=2;++t){
            int yy = y + t;
            if(yy<0||yy>=H) continue;
            int pos = (yy*W + x)*3;
            img[pos+0] = 255; // R
            img[pos+1] = 255; // G
            img[pos+2] = 255; // B
        }
    }
    // Additionally draw center line
    for(int x=0;x<W;++x) {
        int y = H/2;
        int pos = (y*W + x)*3;
        img[pos+0] = 40;
        img[pos+1] = 40;
        img[pos+2] = 40;
    }
    // If payload exists, embed it into pixels' blue channel LSB sequentially.
    // We'll store 32-bit length first then bytes (same order as WAV). We'll embed into first many pixels.
    vector<uint8_t> bits;
    if(wavHasPayload) {
        // length
        uint32_t L = (uint32_t)payload.size();
        for(int b=0;b<32;++b) bits.push_back((L >> b) & 1);
        for(size_t i=0;i<payload.size();++i){
            uint8_t pb = payload[i];
            for(int b=0;b<8;++b) bits.push_back((pb >> b) & 1);
        }
    }
    if(!bits.empty()){
        size_t bitCount = bits.size();
        size_t pxCount = (size_t)W * (size_t)H;
        if(bitCount > pxCount) {
            cerr << "Warning: not enough pixels to embed payload bits into PNG. Payload truncated.\n";
            bitCount = pxCount;
        }
        for(size_t i=0;i<bitCount;++i){
            size_t p = i;
            size_t pos = p * 3;
            uint8_t blue = img[pos+2];
            blue = (blue & 0xFE) | bits[i];
            img[pos+2] = blue;
        }
        cout << "Embedded " << bits.size() << " bits into PNG LSBs.\n";
    } else {
        cout << "No payload to embed into PNG.\n";
    }
    // Write PNG
    bool ok = writePNG_raw(pngfile, W, H, img);
    if(ok) {
        cout << "Saved waveform PNG to: " << pngfile << "\n";
        // Quick self-check: try to read the PNG we just wrote using our reader. If it fails,
        // dump some diagnostics to help debug why readPNG_extractRGB cannot parse it.
        int rW=0, rH=0; vector<uint8_t> checkRGB;
        if(!readPNG_extractRGB(pngfile, rW, rH, checkRGB)) {
            cerr << "Diagnostic: readPNG_extractRGB failed on the PNG we just wrote.\n";
            vector<uint8_t> fdata;
            if(readAllFile(pngfile, fdata)) {
                cerr << "Diagnostic: PNG file size=" << fdata.size() << " bytes\n";
                // print first 64 bytes as hex
                size_t show = min<size_t>(fdata.size(), 64);
                cerr << "Diagnostic: first " << show << " bytes: ";
                for(size_t i=0;i<show;++i) fprintf(stderr, "%02X ", fdata[i]);
                fprintf(stderr, "\n");
                // search for IDAT and report its chunk length
                for(size_t p=8; p+8 < fdata.size(); ) {
                    uint32_t len = (uint32_t)fdata[p]<<24 | (uint32_t)fdata[p+1]<<16 | (uint32_t)fdata[p+2]<<8 | (uint32_t)fdata[p+3];
                    string ctype;
                    if(p+4+4 <= fdata.size()) ctype = string((char*)&fdata[p+4], (char*)&fdata[p+8]);
                    if(ctype=="IDAT") {
                        cerr << "Diagnostic: IDAT found at offset=" << p << " len=" << len << "\n";
                        size_t idat_start = p+8;
                        size_t idat_avail = (idat_start + len <= fdata.size()) ? len : (fdata.size()-idat_start);
                        cerr << "Diagnostic: IDAT available bytes=" << idat_avail << "\n";
                        // print first 32 bytes of IDAT
                        size_t sshow = min<size_t>(idat_avail, 32);
                        cerr << "Diagnostic: IDAT first "<< sshow << " bytes: ";
                        for(size_t i=0;i<sshow;++i) fprintf(stderr, "%02X ", fdata[idat_start + i]);
                        fprintf(stderr, "\n");
                        break;
                    }
                    // move to next chunk: len(4)+type(4)+data(len)+crc(4)
                    size_t next = p + 4 + 4 + len + 4;
                    if(next <= p) break; // overflow guard
                    if(next >= fdata.size()) break;
                    p = next;
                }
            } else {
                cerr << "Diagnostic: failed to read PNG file for diagnostics.\n";
            }
        }
    } else {
        cerr << "Failed to write PNG file.\n";
    }
    return ok;
}

// Read a 24-bit BMP written by our writeBMP24 into top-to-bottom RGB vector.
bool readBMP24_pixels(const string &filename, int &W, int &H, vector<uint8_t> &outRGB) {
    vector<uint8_t> file;
    if(!readAllFile(filename, file)) return false;
    if(file.size() < sizeof(BMPFileHeader) + sizeof(BMPInfoHeader)) return false;
    BMPFileHeader fh;
    BMPInfoHeader ih;
    memcpy(&fh, file.data(), sizeof(fh));
    memcpy(&ih, file.data() + sizeof(fh), sizeof(ih));
    if(fh.bfType != 0x4D42) return false;
    if(ih.biBitCount != 24) return false;
    W = ih.biWidth;
    H = ih.biHeight;
    // pixel data starts at bfOffBits
    size_t dataPos = fh.bfOffBits;
    size_t rowBytes = ((W*3 + 3)/4)*4;
    size_t expected = dataPos + rowBytes * (size_t)H;
    if(expected > file.size()) return false;
    outRGB.resize((size_t)W * (size_t)H * 3);
    // BMP stores rows bottom-up
    for(int y=0;y<H;++y){
        size_t srcRow = dataPos + (size_t)(H-1 - y) * rowBytes;
        for(int x=0;x<W;++x){
            size_t sp = srcRow + x*3;
            size_t dp = (y*(size_t)W + x)*3;
            // BMP stores B,G,R
            if(sp+2 >= file.size()) return false;
            uint8_t b = file[sp+0];
            uint8_t g = file[sp+1];
            uint8_t r = file[sp+2];
            outRGB[dp+0] = r;
            outRGB[dp+1] = g;
            outRGB[dp+2] = b;
        }
    }
    return true;
}

// Generate waveform image as BMP (more robust than custom PNG) and embed payload bits into blue LSB.
bool generateWaveformBMPWithPayload(const string &wavfile, const string &bmpfile) {
    vector<int16_t> samples;
    int sr;
    if(!readWAV_samples(wavfile, samples, sr)) {
        cerr << "Failed to read WAV samples or unsupported WAV format.\n";
        return false;
    }
    if(samples.empty()) {
        cerr << "WAV has no samples.\n";
        return false;
    }
    // Extract payload from WAV LSBs
    auto get_bit = [&](size_t bitIndex)->uint8_t {
        if(bitIndex >= samples.size()) return 0;
        return (uint8_t)(samples[bitIndex] & 1);
    };
    vector<uint8_t> payload;
    bool wavHasPayload = false;
    if(samples.size() >= 32) {
        uint32_t payload_len = 0;
        for(size_t b=0;b<32;++b) payload_len |= (uint32_t)get_bit(b) << b;
        if(payload_len > 0 && 32 + (size_t)payload_len*8 <= samples.size()) {
            wavHasPayload = true;
            payload.resize(payload_len);
            for(size_t i=0;i<payload_len;++i) {
                uint8_t byte = 0;
                for(int bit=0; bit<8; ++bit) {
                    size_t bi = 32 + i*8 + bit;
                    byte |= (get_bit(bi) << bit);
                }
                payload[i] = byte;
            }
            cout << "Found payload in WAV (" << payload_len << " bytes). It will be copied into BMP LSBs.\n";
        } else {
            cout << "No payload found in WAV or not enough bits.\n";
        }
    }
    int W = 1400; int H = 400;
    vector<uint8_t> img(W * H * 3);
    fill(img.begin(), img.end(), 0);
    size_t N = samples.size();
    for(int x=0;x<W;++x) {
        size_t idx = (size_t)((double)x / W * N);
        if(idx >= N) idx = N-1;
        double sample = samples[idx] / 32768.0;
        int y = (int)( (0.5 - sample*0.45) * H );
        if(y<0) y=0; if(y>=H) y=H-1;
        for(int t=-2;t<=2;++t){
            int yy = y + t;
            if(yy<0||yy>=H) continue;
            int pos = (yy*W + x)*3;
            img[pos+0] = 255; img[pos+1] = 255; img[pos+2] = 255;
        }
    }
    for(int x=0;x<W;++x){ int y=H/2; int pos=(y*W+x)*3; img[pos+0]=40; img[pos+1]=40; img[pos+2]=40; }
    // embed payload bits into blue LSBs
    vector<uint8_t> bits;
    if(wavHasPayload) {
        uint32_t L = (uint32_t)payload.size();
        for(int b=0;b<32;++b) bits.push_back((L >> b) & 1);
        for(size_t i=0;i<payload.size();++i){ uint8_t pb=payload[i]; for(int b=0;b<8;++b) bits.push_back((pb>>b)&1); }
    }
    if(!bits.empty()){
        size_t bitCount = bits.size(); size_t pxCount = (size_t)W*(size_t)H;
        if(bitCount > pxCount) { cerr<<"Warning: not enough pixels to embed payload; truncating.\n"; bitCount=pxCount; }
        for(size_t i=0;i<bitCount;++i){ size_t pos=(i)*3; uint8_t blue=img[pos+2]; blue=(blue&0xFE)|bits[i]; img[pos+2]=blue; }
        cout << "Embedded " << bits.size() << " bits into BMP LSBs.\n";
    } else {
        cout << "No payload to embed into BMP.\n";
    }
    if(writeBMP24(bmpfile, W, H, img)) {
        cout << "Saved waveform BMP to: " << bmpfile << "\n";
        // verify by reading back
        int rW=0,rH=0; vector<uint8_t> check;
        if(readBMP24_pixels(bmpfile, rW, rH, check)) {
            cout << "Verified BMP readback: "<<rW<<"x"<<rH<<"\n";
        } else {
            cerr << "Warning: failed to read back BMP we just wrote.\n";
        }
        return true;
    } else {
        cerr << "Failed to write BMP file.\n"; return false;
    }
}

/* -------------------------
   PNG read (very minimal): we'll implement a simple PNG parser that reads our own written PNGs,
   but to keep things simple we can implement a basic reader for the particular PNG structure we write.
   Alternatively we can read the file and search for pixel bytes at known offsets (dangerous).
   We'll implement a small PNG reader that can handle our format: 8-bit RGB, no interlace, no compression complexity beyond our writer.
---------------------------*/

// For decoding we only need to read back the pixel bytes in the order we wrote (top-to-bottom).
// Our tiny writer wrote IDAT containing zlib-wrapped uncompressed DEFLATE blocks; implementing a full decompressor is heavy.
// To avoid implementing decompression, we'll take a simpler approach: when generating the PNG we also saved a sidecar .bin containing payload bits.
// But the user asked to decode from the waveform file (PNG) directly. So we must be able to read PNG LSBs.
// Implementing zlib/deflate decompression is complex and outside scope; instead, we wrote PNG with uncompressed blocks but still zlib-wrapped and with adler checksum.
// We still need to parse zlib uncompressed data: possible to parse as we wrote: zlib header then series of stored blocks. We can implement parsing of stored blocks without full inflate support — doable because we used only stored blocks.
// We'll implement minimal zlib stored-block parser to extract raw data we placed (scanlines).
//
// Steps: parse PNG chunks, find IDAT data, concatenate it, skip zlib header, then parse stored DEFLATE blocks (BTYPE=00), extract raw bytes, then parse scanlines: filter 0 then pixels.

bool readPNG_extractRGB(const string &filename, int &W, int &H, vector<uint8_t> &outRGB) {
    vector<uint8_t> file;
    if(!readAllFile(filename, file)) return false;
    size_t p = 0;
    if(file.size() < 8) return false;
    const unsigned char pngsig[8] = {137,80,78,71,13,10,26,10};
    if(memcmp(file.data(), pngsig, 8) != 0) {
        return false;
    }
    p = 8;
    vector<uint8_t> idat_concat;
    W = H = 0;
    while(p + 8 <= file.size()){
        if(p + 8 > file.size()) break;
        uint32_t len = (file[p]<<24) | (file[p+1]<<16) | (file[p+2]<<8) | (file[p+3]);
        p += 4;
        if(p + 4 + len + 4 > file.size()) return false;
        const unsigned char *chunk_type = file.data()+p;
        p += 4;
        if(memcmp(chunk_type, "IHDR", 4) == 0) {
            if(len < 13) return false;
            W = (file[p]<<24)|(file[p+1]<<16)|(file[p+2]<<8)|(file[p+3]);
            H = (file[p+4]<<24)|(file[p+5]<<16)|(file[p+6]<<8)|(file[p+7]);
            // skip rest
        } else if(memcmp(chunk_type, "IDAT", 4) == 0) {
            idat_concat.insert(idat_concat.end(), file.begin()+p, file.begin()+p+len);
        } else if(memcmp(chunk_type, "IEND",4) == 0) {
            break;
        }
        p += len;
        // skip CRC
        p += 4;
    }
    if(W == 0 || H == 0) return false;
    // idat_concat now contains the zlib stream as written
    cerr << "Diagnostic (readPNG): IHDR W=" << W << " H=" << H << " idat_concat_bytes=" << idat_concat.size() << "\n";
    // Parse zlib header
    if(idat_concat.size() < 2) return false;
    uint8_t cmf = idat_concat[0];
    uint8_t flg = idat_concat[1];
    // skip header
    size_t ip = 2;
    vector<uint8_t> raw;
    size_t total_len_sum = 0;
    int block_count = 0;
    while(ip < idat_concat.size()) {
        if(ip >= idat_concat.size()) break;
        uint8_t bfinal_btype = idat_concat[ip++];
        uint8_t bfinal = bfinal_btype & 1;
        uint8_t btype = (bfinal_btype >> 1) & 3;
        if(btype != 0) {
            // We only support stored blocks (btype==0)
            cerr << "Diagnostic (readPNG): encountered non-stored DEFLATE block type=" << (int)btype << "\n";
            return false;
        }
        if(ip + 4 > idat_concat.size()) { cerr << "Diagnostic (readPNG): truncated LEN header at ip="<<ip<<" idat_concat.size="<<idat_concat.size()<<"\n"; return false; }
        uint16_t len = idat_concat[ip] | (idat_concat[ip+1]<<8);
        uint16_t nlen = idat_concat[ip+2] | (idat_concat[ip+3]<<8);
        ip += 4;
        if((len ^ 0xFFFF) != nlen) return false;
        if(ip + len > idat_concat.size()) return false;
        raw.insert(raw.end(), idat_concat.begin()+ip, idat_concat.begin()+ip+len);
        total_len_sum += len;
        ++block_count;
        ip += len;
        if(bfinal) break;
    }
    cerr << "Diagnostic (readPNG): parsed " << block_count << " stored blocks, total raw bytes="<< total_len_sum <<" (raw.size="<<raw.size()<<")\n";
    // last 4 bytes are Adler32 (we can ignore after raw)
    // But our writer appended Adler32 after blocks; ensure at least 4 bytes remain
    // We'll not validate it and just proceed
    // Now raw contains scanlines: each scanline starts with filter byte then w*3 bytes
    size_t expected = (size_t)H * ((size_t)W*3 + 1);
    if(raw.size() < expected) {
        cerr << "Diagnostic (readPNG): raw decompressed size=" << raw.size() << ", expected=" << expected << "\n";
        return false;
    }
    outRGB.resize((size_t)W * (size_t)H * 3);
    size_t rp = 0;
    for(int y=0;y<H;++y){
        uint8_t filter = raw[rp++];
        if(filter != 0) {
            // we only handle filter 0
            return false;
        }
        for(int x=0;x<W;++x){
            size_t pos = (y*(size_t)W + x)*3;
            outRGB[pos+0] = raw[rp++]; // R
            outRGB[pos+1] = raw[rp++]; // G
            outRGB[pos+2] = raw[rp++]; // B
        }
    }
    return true;
}

/* -------------------------
   Decode payload from PNG LSBs
---------------------------*/
bool decodePayloadFromPNG(const string &pngfile, vector<uint8_t> &payload) {
    int W,H;
    vector<uint8_t> rgb;
    if(!readPNG_extractRGB(pngfile, W, H, rgb)) {
        cerr << "Failed to read PNG or unsupported PNG format for decoding.\n";
        return false;
    }
    size_t pxCount = (size_t)W * (size_t)H;
    auto get_bit = [&](size_t i)->uint8_t {
        if(i >= pxCount) return 0;
        size_t pos = i * 3;
        return rgb[pos+2] & 1; // blue LSB
    };
    // read first 32 bits -> length
    if(pxCount < 32) return false;
    uint32_t len = 0;
    for(size_t b=0;b<32;++b) len |= (uint32_t)get_bit(b) << b;
    if(len == 0) {
        cerr << "Decoded length is zero -> no payload.\n";
        return false;
    }
    size_t totalBits = (size_t)len * 8;
    if(32 + totalBits > pxCount) {
        cerr << "Not enough pixels to contain payload of declared length.\n";
        return false;
    }
    payload.clear();
    payload.resize(len);
    for(size_t i=0;i<len;++i){
        uint8_t byte = 0;
        for(int bit=0; bit<8; ++bit){
            size_t bi = 32 + i*8 + bit;
            byte |= (get_bit(bi) << bit);
        }
        payload[i] = byte;
    }
    return true;
}

// Decode payload from BMP (blue-channel LSBs) using our BMP reader
bool decodePayloadFromBMP(const string &bmpfile, vector<uint8_t> &payload) {
    int W=0,H=0; vector<uint8_t> rgb;
    if(!readBMP24_pixels(bmpfile, W, H, rgb)) {
        cerr << "Failed to read BMP or unsupported BMP format for decoding.\n";
        return false;
    }
    size_t pxCount = (size_t)W * (size_t)H;
    auto get_bit = [&](size_t i)->uint8_t {
        if(i >= pxCount) return 0;
        size_t pos = i * 3;
        return rgb[pos+2] & 1; // blue LSB
    };
    if(pxCount < 32) return false;
    uint32_t len = 0;
    for(size_t b=0;b<32;++b) len |= (uint32_t)get_bit(b) << b;
    if(len == 0) {
        cerr << "Decoded length is zero -> no payload.\n";
        return false;
    }
    size_t totalBits = (size_t)len * 8;
    if(32 + totalBits > pxCount) {
        cerr << "Not enough pixels to contain payload of declared length.\n";
        return false;
    }
    payload.clear(); payload.resize(len);
    for(size_t i=0;i<len;++i){
        uint8_t byte = 0;
        for(int bit=0; bit<8; ++bit){
            size_t bi = 32 + i*8 + bit;
            byte |= (get_bit(bi) << bit);
        }
        payload[i] = byte;
    }
    return true;
}

// Try to extract text from an RGB bitmap that was rendered with renderTextToBMP
// Returns true if extraction succeeded (may include '?' for unknown glyphs)
bool extractTextFromRenderedBMP(int W, int H, const vector<uint8_t> &rgb, string &outText) {
    const int charW = 8, charH = 8;
    // try to detect margin by scanning for first non-black pixel
    int left = W, top = H, right = 0, bottom = 0;
    for(int y=0;y<H;++y){
        for(int x=0;x<W;++x){
            size_t p = (y*(size_t)W + x)*3;
            if(rgb[p] != 0 || rgb[p+1] != 0 || rgb[p+2] != 0){
                left = min(left, x); top = min(top, y); right = max(right, x); bottom = max(bottom, y);
            }
        }
    }
    if(right < left || bottom < top) return false; // empty image
    // try margin values from 0..32 to find a grid that fits
    int found_margin = -1; int cols=0, rows=0;
    for(int margin=0;margin<=32;++margin){
        if(W - 2*margin <=0 || H - 2*margin <=0) continue;
        if(((W - 2*margin) % charW) != 0) continue;
        if(((H - 2*margin) % charH) != 0) continue;
        int c = (W - 2*margin)/charW;
        int r = (H - 2*margin)/charH;
        // check that bounding box of non-black pixels lies within margin..margin+grid
        int gx0 = margin, gy0 = margin;
        int gx1 = margin + c*charW - 1;
        int gy1 = margin + r*charH - 1;
        if(left >= gx0 && right <= gx1 && top >= gy0 && bottom <= gy1){ found_margin = margin; cols=c; rows=r; break; }
    }
    if(found_margin == -1) return false;
    int margin = found_margin;
    // build char grid
    outText.clear();
    for(int row=0; row<rows; ++row){
        string line;
        for(int col=0; col<cols; ++col){
            // build 8x8 bits
            unsigned char glyph[8] = {0};
            for(int y=0;y<charH;++y){
                unsigned char bits = 0;
                for(int x=0;x<charW;++x){
                    int px = margin + col*charW + x;
                    int py = margin + row*charH + y;
                    size_t p = (py*(size_t)W + px)*3;
                    uint8_t r = rgb[p], g = rgb[p+1], b = rgb[p+2];
                    bool on = (r + g + b) > 128; // white-ish
                    if(on) bits |= (1 << (7-x));
                }
                glyph[y] = bits;
            }
            // match glyph against tiny8x8_font
            char matched = '?';
            for(int ci=0; ci<96; ++ci){
                bool same = true;
                for(int y=0;y<8;++y){ if(tiny8x8_font[ci][y] != glyph[y]) { same = false; break; } }
                if(same) { matched = (char)(32 + ci); break; }
            }
            line.push_back(matched);
        }
        // trim trailing spaces
        while(!line.empty() && line.back()==' ') line.pop_back();
        outText += line;
        if(row+1 < rows) outText += '\n';
    }
    return true;
}

/* -------------------------
   Text -> BMP rendering (black bg, white text)
   We'll render text with monospace 8x8 font above; text wraps by user-controlled width.
---------------------------*/
bool renderTextToBMP(const string &text, const string &bmpfile, int maxWidthChars = 80, int margin = 10) {
    // Wrap text into lines
    vector<string> lines;
    {
        string s = text;
        // replace CRLF with LF
        for(char &c : s) if(c == '\r') c = '\n';
        // split on newline but wrap long lines
        string cur;
        for(size_t i=0;i<s.size();++i) {
            char c = s[i];
            if(c == '\n') {
                if(cur.empty()) lines.push_back("");
                else lines.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(c);
                if((int)cur.size() >= maxWidthChars) {
                    lines.push_back(cur);
                    cur.clear();
                }
            }
        }
        if(!cur.empty()) lines.push_back(cur);
    }
    int charW = 8, charH = 8;
    int cols = 0;
    for(auto &ln: lines) cols = max(cols, (int)ln.size());
    if(cols == 0) cols = 1;
    int W = margin*2 + cols * charW;
    int H = margin*2 + (int)lines.size() * charH;
    vector<uint8_t> img(W*H*3);
    // Background black
    fill(img.begin(), img.end(), 0);
    // Render each char in white (255)
    for(size_t row=0; row<lines.size(); ++row) {
        const string &ln = lines[row];
        for(size_t col=0; col<ln.size(); ++col) {
            unsigned char ch = (unsigned char)ln[col];
            if(ch < 32 || ch > 127) ch = '?';
            const unsigned char *glyph = tiny8x8_font[ch - 32];
            for(int y=0;y<charH;++y){
                unsigned char bits = glyph[y];
                for(int x=0;x<charW;++x){
                    bool on = bits & (1 << (7-x));
                    if(on) {
                        int px = margin + (int)col*charW + x;
                        int py = margin + (int)row*charH + y;
                        size_t pos = (py*W + px)*3;
                        img[pos+0] = 255;
                        img[pos+1] = 255;
                        img[pos+2] = 255;
                    }
                }
            }
        }
    }
    if(!writeBMP24(bmpfile, W, H, img)) return false;
    cout << "Saved BMP to: " << bmpfile << " (" << W << "x" << H << ")\n";
    return true;
}

/* -------------------------
   CLI menu and glue
---------------------------*/
void writeTextOption() {
    cout << "Enter your message (end with a single line containing only a dot '.'):\n";
    string line;
    string text;
    // Read lines until a single dot '.' on its own line, or until the user enters a blank line
    // after having entered at least one non-empty line. This is more user-friendly than
    // forcing the '.' sentinel for short messages.
    bool firstLineRead = false;
    while(true) {
        if(!std::getline(cin, line)) break;
        if(line == ".") break;
        if(!firstLineRead) {
            // First non-empty line: accept it and offer to add more
            if(line.empty()) continue; // ignore leading blank lines
            text += line;
            text += '\n';
            firstLineRead = true;
            cout << "Add more lines? (y/N): "; cout.flush();
            string resp;
            if(!std::getline(cin, resp)) break;
            if(resp.size() > 0 && (resp[0]=='y' || resp[0]=='Y')) {
                // enter multi-line mode until '.' or blank line after content
                while(true) {
                    if(!std::getline(cin, line)) break;
                    if(line == ".") goto done_reading;
                    if(line.empty()) {
                        if(!text.empty()) break; // finish on blank line
                        else continue;
                    }
                    text += line;
                    text += '\n';
                }
                break;
            } else {
                // user chose not to add more; finish input
                break;
            }
        } else {
            // shouldn't reach here, but handle defensively
            if(line.empty()) break;
            if(line == ".") break;
            text += line;
            text += '\n';
        }
    }
done_reading: ;
    if(text.size() == 0) {
        cout << "No text entered.\n";
        return;
    }
    cout << "Output BMP filename (e.g. message.bmp): ";
    string fname;
    getline(cin, fname);
    if(fname.empty()) fname = "message.bmp";
    if(renderTextToBMP(text, fname)) {
        cout << "BMP created: " << fname << "\n";
    } else {
        cout << "Failed to create BMP.\n";
    }
}

bool readBMPDataAsPayload(const string &bmpfile, vector<uint8_t> &outBytes) {
    // Simply read entire BMP file bytes (raw file) to embed
    return readAllFile(bmpfile, outBytes);
}

void encodeBmpToWavOption() {
    cout << "Enter BMP filename to encode (e.g. message.bmp): ";
    string bmpfile; getline(cin, bmpfile);
    if(bmpfile.empty()) { cout << "No filename provided.\n"; return; }
    vector<uint8_t> payload;
    // Try reading as-given. If not found, and user didn't include an extension, try appending .bmp
    auto try_read = [&](const string &fn)->bool{ return readBMPDataAsPayload(fn, payload); };
    if(!try_read(bmpfile)) {
        // if no extension, try adding .bmp
        bool has_ext = false;
        size_t dot = bmpfile.find_last_of('.');
        size_t sep1 = bmpfile.find_last_of('/');
        size_t sep2 = bmpfile.find_last_of('\\');
        size_t sep = string::npos;
        if(sep1!=string::npos && sep2!=string::npos) sep = max(sep1, sep2);
        else if(sep1!=string::npos) sep = sep1;
        else if(sep2!=string::npos) sep = sep2;
        if(dot!=string::npos && (sep==string::npos || dot > sep)) has_ext = true;
        if(!has_ext) {
            string tryname = bmpfile + ".bmp";
            if(try_read(tryname)) bmpfile = tryname;
        }
    }
    if(payload.empty()) {
        cout << "Failed to read BMP file '" << bmpfile << "'.\n";
        // help: list BMP files in current directory
        try {
            cout << "Files with .bmp extension in current directory:\n";
            int shown = 0;
            for(auto &p : std::filesystem::directory_iterator(std::filesystem::current_path())){
                if(shown >= 50) break;
                if(!p.is_regular_file()) continue;
                auto path = p.path();
                if(path.has_extension() && path.extension()==".bmp"){
                    cout << "  " << path.filename().string() << "\n";
                    ++shown;
                }
            }
        } catch(...) {
            // ignore filesystem errors
        }
        return;
    }
    cout << "Read " << payload.size() << " bytes from BMP. Output WAV filename: ";
    string wavfile; getline(cin, wavfile);
    if(wavfile.empty()) wavfile = "carrier.wav";
    // If the user didn't provide an extension, append .wav
    auto has_extension = [&](const string &fn)->bool{
        // find last path separator
        size_t pos1 = fn.find_last_of('/');
        size_t pos2 = fn.find_last_of('\\');
        size_t pos = string::npos;
        if(pos1!=string::npos && pos2!=string::npos) pos = max(pos1,pos2);
        else if(pos1!=string::npos) pos = pos1;
        else if(pos2!=string::npos) pos = pos2;
        // find last dot after pos
        size_t dot = fn.find_last_of('.');
        if(dot==string::npos) return false;
        if(pos!=string::npos && dot < pos) return false;
        return true;
    };
    if(!has_extension(wavfile)) wavfile += ".wav";
    if(writeWAV_LSBCarrier(wavfile, payload)) {
        cout << "Saved WAV with embedded payload: " << wavfile << "\n";
        // Validate by reading back and extracting
        vector<uint8_t> extracted;
        if(extractPayloadFromWAV_LSB(wavfile, extracted)) {
            if(extracted == payload) {
                cout << "Validation OK: payload round-trip matches BMP bytes (" << extracted.size() << " bytes).\n";
            } else {
                cout << "Warning: extracted payload differs from original BMP bytes.\n";
            }
        } else {
            cout << "Warning: failed to extract/validate payload from written WAV.\n";
        }
    } else {
        cout << "Failed to write WAV.\n";
    }
}

void wavToWaveformOption() {
    cout << "Enter WAV filename to process (e.g. carrier.wav): ";
    string wavfile; getline(cin, wavfile);
    if(wavfile.empty()) { cout << "No filename provided.\n"; return; }
    cout << "Output waveform PNG filename (e.g. waveform.png): ";
    string pngfile; getline(cin, pngfile);
    if(pngfile.empty()) pngfile = "waveform.bmp";
    // if user provided .png explicitly, try PNG path; otherwise produce BMP for reliability
    auto ext = [](const string &s)->string{ size_t dot=s.find_last_of('.'); if(dot==string::npos) return string(); return s.substr(dot); };
    string e = ext(pngfile);
    if(e.empty()) pngfile += ".bmp", e = ".bmp";
    if(iequals(e, ".png")) {
        if(generateWaveformPNGWithPayload(wavfile, pngfile)) cout << "Waveform PNG written: " << pngfile << "\n";
        else cout << "Failed to create waveform PNG.\n";
    } else {
        if(generateWaveformBMPWithPayload(wavfile, pngfile)) cout << "Waveform BMP written: " << pngfile << "\n";
        else cout << "Failed to create waveform BMP.\n";
    }
}

void decodeFromWaveformOption() {
    cout << "Enter waveform image filename to decode (e.g. waveform.bmp or waveform.png): ";
    string imgfile; getline(cin, imgfile);
    if(imgfile.empty()) { cout << "No filename provided.\n"; return; }
    vector<uint8_t> payload;
    // detect extension (try BMP if none provided)
    auto ext = [&](const string &s)->string{ size_t dot=s.find_last_of('.'); if(dot==string::npos) return string(); return s.substr(dot); };
    string e = ext(imgfile);
    bool ok = false;
    if(e.empty()) {
        // try bmp then png
        string trybmp = imgfile + ".bmp";
        if(decodePayloadFromBMP(trybmp, payload)) { imgfile = trybmp; ok = true; }
        else if(decodePayloadFromPNG(trybmp, payload)) { imgfile = trybmp; ok = true; }
        else {
            string trypng = imgfile + ".png";
            if(decodePayloadFromPNG(trypng, payload)) { imgfile = trypng; ok = true; }
            else if(decodePayloadFromBMP(trypng, payload)) { imgfile = trypng; ok = true; }
        }
    } else if(iequals(e, ".bmp")) {
        ok = decodePayloadFromBMP(imgfile, payload);
    } else if(iequals(e, ".png")) {
        ok = decodePayloadFromPNG(imgfile, payload);
    } else {
        // unknown extension: try bmp then png
        if(decodePayloadFromBMP(imgfile, payload)) ok = true;
        else ok = decodePayloadFromPNG(imgfile, payload);
    }
    if(!ok) {
        cout << "Failed to decode payload from image '" << imgfile << "'.\n";
        return;
    }
    // If the payload looks like a BMP file, try to recover the text that was rendered into it.
    bool saved = false;
    if(payload.size() >= 2 && payload[0]=='B' && payload[1]=='M') {
        // write temp BMP file, read pixels, attempt OCR-like extraction
        string tmp = "decoded_recovered.bmp";
        FILE *tf = fopen(tmp.c_str(), "wb");
        if(tf) {
            fwrite(payload.data(), 1, payload.size(), tf);
            fclose(tf);
            int W=0,H=0; vector<uint8_t> rgb;
            if(readBMP24_pixels(tmp, W, H, rgb)) {
                string recovered;
                if(extractTextFromRenderedBMP(W, H, rgb, recovered)) {
                    cout << "Recovered text (saved to file):\n" << recovered << "\n";
                    cout << "Output text filename (e.g. decoded.txt): ";
                    string outfn; getline(cin, outfn);
                    if(outfn.empty()) outfn = "decoded.txt";
                    FILE *f = fopen(outfn.c_str(), "wb");
                    if(f) {
                        fwrite(recovered.c_str(), 1, recovered.size(), f);
                        fclose(f);
                        cout << "Saved recovered text to " << outfn << "\n";
                        saved = true;
                    } else {
                        cout << "Failed to open output file for recovered text.\n";
                    }
                } else {
                    cout << "Payload is BMP but failed to extract text from image.\n";
                }
            } else {
                cout << "Failed to read BMP we just wrote for text extraction.\n";
            }
            // leave tmp BMP present for inspection
        }
    }
    if(!saved) {
        cout << "Decoded payload bytes: " << payload.size() << ". Save as text filename (e.g. decoded.txt): ";
        string outfn; getline(cin, outfn);
        if(outfn.empty()) outfn = "decoded.txt";
        FILE *f = fopen(outfn.c_str(), "wb");
        if(!f) { cout << "Failed to open output file.\n"; return; }
        fwrite(payload.data(), 1, payload.size(), f);
        fclose(f);
        cout << "Saved decoded payload to " << outfn << "\n";
    }
}

int main(){
    // Keep C and C++ IO synced to avoid subtle console input/echo issues on Windows
    ios::sync_with_stdio(true);
    // Tie cin to cout so prompts (without newline) are flushed before input reads.
    cin.tie(&cout);
    // Animated header and simple login
    auto typewriter = [&](const string &s, int ms=6){
        for(char c: s){ cout << c; cout.flush(); this_thread::sleep_for(chrono::milliseconds(ms)); }
    };

    auto spinner = [&](int ms_total=800){
        const char spin[] = {'|','/','-','\\'};
        int idx = 0;
        int elapsed = 0;
        while(elapsed < ms_total){
            cout << '\r' << "[" << spin[idx%4] << "] "; cout.flush();
            this_thread::sleep_for(chrono::milliseconds(80));
            elapsed += 80; idx++;
        }
        cout << "\r   \r"; cout.flush();
    };

    auto dotdot = [&](int n=3, int ms=300){
        for(int i=0;i<n;++i){ cout << "."; cout.flush(); this_thread::sleep_for(chrono::milliseconds(ms)); }
        cout << "\n";
    };

    auto get_console_width = [&]()->int{
#ifdef _WIN32
    // Try to query the Windows console width at runtime without including windows.h
    // Use dynamic lookup of GetStdHandle and GetConsoleScreenBufferInfo to avoid header conflicts.
    int width = 80;
    void* hKer = nullptr;
    // Load kernel32 if available via our extern declaration
    hKer = LoadLibraryA("kernel32.dll");
    if(hKer) {
        auto pGetStdHandle = (void* (__stdcall *)(int)) GetProcAddress(hKer, "GetStdHandle");
        auto pGetConsoleScreenBufferInfo = (int (__stdcall *)(void*, void*)) GetProcAddress(hKer, "GetConsoleScreenBufferInfo");
        if(pGetStdHandle && pGetConsoleScreenBufferInfo){
            void* hOut = pGetStdHandle(-11); // STD_OUTPUT_HANDLE
            // locally define structure matching CONSOLE_SCREEN_BUFFER_INFO layout (portable short/int sizes)
            struct COORDS { short X; short Y; };
            struct SMALL_RECT { short Left; short Top; short Right; short Bottom; };
            struct CSBI {
                COORDS dwSize;
                COORDS dwCursorPosition;
                unsigned short wAttributes;
                SMALL_RECT srWindow;
                COORDS dwMaximumWindowSize;
            } csbi;
            // call the function; it returns non-zero on success
            int ok = pGetConsoleScreenBufferInfo(hOut, (void*)&csbi);
            if(ok){
                // width is srWindow.Right - srWindow.Left + 1
                int w = (int)csbi.srWindow.Right - (int)csbi.srWindow.Left + 1;
                if(w > 0) width = w;
            }
        }
        FreeLibrary(hKer);
    }
    return width;
#else
    struct winsize w;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) return w.ws_col;
    return 80;
#endif
    };

    // Try to enable ANSI VT processing on Windows at runtime (without including windows.h)
#ifdef _WIN32
    auto enable_ansi_windows = [&](){
        // Minimal dynamic load of kernel32 functions to avoid windows.h conflicts
        typedef void* (__stdcall *FP_LoadLibraryA)(const char*);
        typedef void* (__stdcall *FP_GetProcAddress)(void*, const char*);
        typedef int (__stdcall *FP_FreeLibrary)(void*);
        // acquire LoadLibraryA and GetProcAddress from the CRT import table
    void* hKer = LoadLibraryA("kernel32.dll");
    if(!hKer) return;
    auto pGetStdHandle = (void* (__stdcall *)(int)) GetProcAddress(hKer, "GetStdHandle");
    auto pGetConsoleMode = (int (__stdcall *)(void*, unsigned long*)) GetProcAddress(hKer, "GetConsoleMode");
    auto pSetConsoleMode = (int (__stdcall *)(void*, unsigned long)) GetProcAddress(hKer, "SetConsoleMode");
        if(pGetStdHandle && pGetConsoleMode && pSetConsoleMode){
            void* hOut = pGetStdHandle(-11);
            unsigned long mode = 0;
            if(pGetConsoleMode(hOut, &mode)){
                const unsigned long ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004;
                pSetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
        FreeLibrary(hKer);
    };
    // attempt enable (safe no-op if unavailable)
    enable_ansi_windows();
#endif

    auto center_print = [&](const string &text, int width, const string &color="", bool bold=false){
        string reset = "\x1b[0m";
        string bolds = bold ? "\x1b[1m" : "";
        int pad = max(0, (width - (int)text.size())/2);
        cout << string(pad, ' ');
        if(!color.empty()) cout << color;
        if(bold) cout << bolds;
        cout << text;
        if(!color.empty() || bold) cout << reset;
        cout << "\n";
    };

    // Fancy FIGlet-style animated header with green hacking vibe
    cout << "\n";
    int cw = get_console_width();
    string green = "\x1b[32;1m"; // bright green
    string dark = "\x1b[2;32m";   // dim green for shadow
    string red = "\x1b[31;1m";   // bright red for errors
    string reset = "\x1b[0m";
    // Animated neon header that preserves the original title text but adds shadow + reveal
    string title = "yoKgUeWsEhNwIari";
    // top separator
    center_print(string(40, '='), cw, dark, false);
    // main bright animated title (no duplicate shadow)
    {
        int pad = max(0, (cw - (int)title.size())/2);
        cout << string(pad, ' ');
        cout << green;
        for(char c: title){ cout << c; cout.flush(); this_thread::sleep_for(chrono::milliseconds(8)); }
        cout << reset << "\n";
    }
    // bottom separator and subtitle
    center_print(string(40, '='), cw, dark, false);
    center_print("Comms Encrypter", cw, green, false);
    center_print("by kavi.amara", cw, green, false);
    cout << "\n";
    // short initializing animation
    cout << green; cout.flush();
    cout << "Initializing" << flush; dotdot(3,140);
    spinner(600);
    cout << reset; // reset color

    // Simple credential prompt
#ifdef _WIN32
    auto is_tty_stdin = [&]()->bool{ return _isatty(_fileno(stdin)); };
#else
    auto is_tty_stdin = [&]()->bool{ return isatty(fileno(stdin)); };
#endif

    auto getPassword = [&]()->string{
#ifdef _WIN32
        // if stdin is not a TTY (piped), fall back to getline so automated tests work
        if(!is_tty_stdin()) { string p; getline(cin, p); return p; }
        string pwd; int ch;
        while((ch = _getch()) != 13){ // Enter
            if(ch == 8){ // backspace
                if(!pwd.empty()){ pwd.pop_back(); cout << "\b \b"; cout.flush(); }
            } else if(ch == 0 || ch == 224) {
                // ignore special keys
                int dummy = _getch(); (void)dummy;
            } else {
                pwd.push_back((char)ch);
                cout << '*'; cout.flush();
            }
        }
        cout << '\n';
        return pwd;
#else
        // Fallback: visible input
        string p; getline(cin,p); return p;
#endif
    };

    

    const string wanted_user = "abyss";
    const string wanted_pass = "B16";
    int attempts = 0; bool authed = false;
    while(attempts < 5 && !authed){
        cout << "Username: " << flush; string user; getline(cin, user);
        cout << "Password: " << flush; string pass = getPassword();
        if(user == wanted_user && pass == wanted_pass){
            authed = true; break;
        }
        attempts++;
        cout << red << "Access denied" << reset;
        for(int i=0;i<3;++i){ cout << "."; cout.flush(); this_thread::sleep_for(chrono::milliseconds(220)); }
        cout << "\n";
    }
    if(!authed){ cout << red << "Too many failed attempts. Exiting." << reset << "\n"; return 0; }
    cout << green << "Access granted. Welcome, " << wanted_user << "!" << reset << '\n';
    spinner(500);
    while(true) {
        cout << "\nSelect option:\n";
        cout << "1) Write text -> BMP (black background, white text)\n";
        cout << "2) Use BMP -> encode message into WAV (LSB carrier)\n";
        cout << "3) Use WAV -> generate waveform PNG (PNG is 'best format' here)\n";
        cout << "4) Use waveform PNG -> decode text message -> save .txt\n";
        cout << "5) Exit\n";
    cout << "Choice: " << flush;
    string choice;
    if(!getline(cin, choice)) break;
        if(choice=="1") writeTextOption();
        else if(choice=="2") encodeBmpToWavOption();
        else if(choice=="3") wavToWaveformOption();
        else if(choice=="4") decodeFromWaveformOption();
        else if(choice=="5" || choice=="q" || choice=="quit") break;
        else cout << "Unknown option.\n";
    }
    cout << "Goodbye.\n";
    return 0;
}
