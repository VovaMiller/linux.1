#include <bitmap.h>

char read_bit(char *bytes, unsigned int pos) {
    unsigned int i = pos / 8;
    return bytes[i] & ((char)1 << (pos % 8));
}

void write_bit(char *bytes, unsigned int pos, char bit) {
    unsigned int i = pos / 8;
    if (bit) {
        bytes[i] |= (char)1 << (pos % 8);
    } else {
        bytes[i] &= ~((char)1 << (pos % 8));
    }
}

void bitmap_fwrite(char *bytes, unsigned int count, FILE *file) {
    fwrite(bytes, count, 1, file);
}

void bitmap_fread(char *bytes, unsigned int count, FILE *file) {
    fread(bytes, count, 1, file);
}
