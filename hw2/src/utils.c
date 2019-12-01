#include <utils.h>

int int_pow(int base, int exp) {
    int result = 1;
    while (exp) {
        if (exp & 1)
           result *= base;
        exp /= 2;
        base *= base;
    }
    return result;
}

int get_line(char *buff, size_t sz) {
    int ch, extra;
    if (fgets(buff, sz, stdin) == NULL) {
        return READING_NO_INPUT;
    }

    // If it was too long, there'll be no newline. In that case, we flush
    // to end of line so that excess doesn't affect the next call.
    if (buff[strlen(buff)-1] != '\n') {
        extra = 0;
        while (((ch = getchar()) != '\n') && (ch != EOF))
            extra = 1;
        return (extra == 1) ? READING_TOO_LONG : READING_OK;
    }

    // Otherwise remove newline and give string back to caller.
    buff[strlen(buff)-1] = '\0';
    return READING_OK;
}
