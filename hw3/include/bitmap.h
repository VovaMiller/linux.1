#include <stdio.h>

char read_bit(char *bytes, unsigned int pos);

void write_bit(char *bytes, unsigned int pos, char bit);

void bitmap_fwrite(char *bytes, unsigned int count, FILE *file);

void bitmap_fread(char *bytes, unsigned int count, FILE *file);

