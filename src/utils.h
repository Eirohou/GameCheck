#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdbool.h>

/* ANSI Terminal Color Controls */
void utils_set_color_enabled(bool enabled);
bool utils_is_color_enabled(void);

const char *col_reset(void);
const char *col_bold(void);
const char *col_dim(void);
const char *col_red(void);
const char *col_green(void);
const char *col_yellow(void);
const char *col_blue(void);
const char *col_magenta(void);
const char *col_cyan(void);
const char *col_white(void);

/* String Manipulation */
char *str_trim(char *s);
char *str_dup(const char *s);
char *str_tolower_copy(const char *s);
bool str_contains_ci(const char *haystack, const char *needle);
bool str_starts_with_ci(const char *str, const char *prefix);

/* HTML & Requirements Parsing */
void strip_html(const char *src, char *dest, size_t dest_size);
double parse_size_in_gb(const char *str);
double parse_cpu_ghz(const char *str);
int parse_cpu_cores(const char *str);

/* URL & Networking */
char *url_encode(const char *str);

#endif /* UTILS_H */
