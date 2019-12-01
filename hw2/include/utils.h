#include <stdio.h>
#include <string.h>

#define READING_OK       0
#define READING_NO_INPUT 1
#define READING_TOO_LONG 2

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/*
 * Function: int_pow
 * --------------------
 * Calculates base^exp for integer inputs.
 */
int int_pow(int base, int exp);

/*
 * Function: get_line
 * --------------------
 * Safe string reading from stdin.
 */
int get_line(char *buff, size_t sz);
