#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <curl/curl.h>

static bool s_color_enabled = true;

void utils_set_color_enabled(bool enabled) {
    s_color_enabled = enabled;
}

bool utils_is_color_enabled(void) {
    return s_color_enabled;
}

const char *col_reset(void)   { return s_color_enabled ? "\033[0m" : ""; }
const char *col_bold(void)    { return s_color_enabled ? "\033[1m" : ""; }
const char *col_dim(void)     { return s_color_enabled ? "\033[2m" : ""; }
const char *col_red(void)     { return s_color_enabled ? "\033[31;1m" : ""; }
const char *col_green(void)   { return s_color_enabled ? "\033[32;1m" : ""; }
const char *col_yellow(void)  { return s_color_enabled ? "\033[33;1m" : ""; }
const char *col_blue(void)    { return s_color_enabled ? "\033[34;1m" : ""; }
const char *col_magenta(void) { return s_color_enabled ? "\033[35;1m" : ""; }
const char *col_cyan(void)    { return s_color_enabled ? "\033[36;1m" : ""; }
const char *col_white(void)   { return s_color_enabled ? "\033[37;1m" : ""; }

char *str_trim(char *s) {
    if (!s) return NULL;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d) {
        memcpy(d, s, len + 1);
    }
    return d;
}

char *str_tolower_copy(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++) {
        out[i] = (char)tolower((unsigned char)s[i]);
    }
    out[len] = '\0';
    return out;
}

bool str_contains_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    if (!*needle) return true;

    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, nlen) == 0) {
            return true;
        }
    }
    return false;
}

bool str_starts_with_ci(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    return strncasecmp(str, prefix, strlen(prefix)) == 0;
}

void strip_html(const char *src, char *dest, size_t dest_size) {
    if (!src || !dest || dest_size == 0) return;
    size_t d = 0;
    bool in_tag = false;
    bool newline_pending = false;

    for (size_t s = 0; src[s] != '\0' && d + 1 < dest_size; s++) {
        if (src[s] == '<') {
            in_tag = true;
            if (strncasecmp(&src[s], "<br", 3) == 0 ||
                strncasecmp(&src[s], "<li", 3) == 0 ||
                strncasecmp(&src[s], "</li", 4) == 0 ||
                strncasecmp(&src[s], "</p", 3) == 0 ||
                strncasecmp(&src[s], "</h", 3) == 0 ||
                strncasecmp(&src[s], "<tr", 3) == 0 ||
                strncasecmp(&src[s], "</tr>", 5) == 0) {
                newline_pending = true;
            }
            continue;
        }
        if (src[s] == '>') {
            in_tag = false;
            if (newline_pending) {
                if (d > 0 && dest[d - 1] != '\n') {
                    dest[d++] = '\n';
                }
                newline_pending = false;
            }
            continue;
        }
        if (in_tag) continue;

        // HTML entity decoding
        if (src[s] == '&') {
            if (strncasecmp(&src[s], "&reg;", 5) == 0) { s += 4; continue; }
            if (strncasecmp(&src[s], "&trade;", 7) == 0) { s += 6; continue; }
            if (strncasecmp(&src[s], "&bull;", 6) == 0) { s += 5; continue; }
            if (strncasecmp(&src[s], "&amp;", 5) == 0) {
                if (d + 1 < dest_size) dest[d++] = '&';
                s += 4;
                continue;
            }
            if (strncasecmp(&src[s], "&quot;", 6) == 0) {
                if (d + 1 < dest_size) dest[d++] = '"';
                s += 5;
                continue;
            }
            if (strncasecmp(&src[s], "&#39;", 5) == 0 || strncasecmp(&src[s], "&apos;", 6) == 0) {
                if (d + 1 < dest_size) dest[d++] = '\'';
                s += (src[s + 1] == 'a') ? 5 : 4;
                continue;
            }
            if (strncasecmp(&src[s], "&nbsp;", 6) == 0) {
                if (d + 1 < dest_size) dest[d++] = ' ';
                s += 5;
                continue;
            }
            if (strncasecmp(&src[s], "&lt;", 4) == 0) {
                if (d + 1 < dest_size) dest[d++] = '<';
                s += 3;
                continue;
            }
            if (strncasecmp(&src[s], "&gt;", 4) == 0) {
                if (d + 1 < dest_size) dest[d++] = '>';
                s += 3;
                continue;
            }
        }

        if (src[s] == '\r') continue;
        if (src[s] == '\t') {
            if (d > 0 && dest[d - 1] != ' ' && dest[d - 1] != '\n') {
                dest[d++] = ' ';
            }
            continue;
        }
        dest[d++] = src[s];
    }
    dest[d] = '\0';
}

double parse_size_in_gb(const char *str) {
    if (!str) return 0.0;
    const char *p = str;
    while (*p) {
        if (isdigit((unsigned char)*p)) {
            char *endptr;
            double val = strtod(p, &endptr);
            if (endptr > p) {
                const char *u = endptr;
                while (*u == ' ' || *u == '\t') u++;
                // Skip optional "ssd" or similar indicator
                if (strncasecmp(u, "ssd", 3) == 0) {
                    u += 3;
                    while (*u == ' ' || *u == '\t') u++;
                }
                if (strncasecmp(u, "gb", 2) == 0 || *u == 'g' || *u == 'G') {
                    return val;
                }
                if (strncasecmp(u, "mb", 2) == 0 || *u == 'm' || *u == 'M') {
                    return val / 1024.0;
                }
            }
            p = endptr;
        } else {
            p++;
        }
    }
    return 0.0;
}

double parse_cpu_ghz(const char *str) {
    if (!str) return 0.0;
    const char *p = str;
    while (*p) {
        if (isdigit((unsigned char)*p)) {
            char *endptr;
            double val = strtod(p, &endptr);
            if (endptr > p) {
                const char *u = endptr;
                while (*u == ' ' || *u == '\t') u++;
                if (strncasecmp(u, "ghz", 3) == 0) {
                    return val;
                }
                if (strncasecmp(u, "mhz", 3) == 0) {
                    return val / 1000.0;
                }
            }
            p = endptr;
        } else {
            p++;
        }
    }
    return 0.0;
}

int parse_cpu_cores(const char *str) {
    if (!str) return 0;
    if (str_contains_ci(str, "octa core") || str_contains_ci(str, "octa-core") ||
        str_contains_ci(str, "eight core") || str_contains_ci(str, "8-core") ||
        str_contains_ci(str, "8 core")) {
        return 8;
    }
    if (str_contains_ci(str, "hexa core") || str_contains_ci(str, "six core") ||
        str_contains_ci(str, "6-core") || str_contains_ci(str, "6 core")) {
        return 6;
    }
    if (str_contains_ci(str, "quad core") || str_contains_ci(str, "quad-core") ||
        str_contains_ci(str, "4-core") || str_contains_ci(str, "4 core")) {
        return 4;
    }
    if (str_contains_ci(str, "dual core") || str_contains_ci(str, "dual-core") ||
        str_contains_ci(str, "2-core") || str_contains_ci(str, "2 core")) {
        return 2;
    }
    // Heuristics for common Intel/AMD lines
    if (str_contains_ci(str, "ryzen 9") || str_contains_ci(str, "core i9")) {
        return 12;
    }
    if (str_contains_ci(str, "ryzen 7") || str_contains_ci(str, "core i7")) {
        return 8;
    }
    if (str_contains_ci(str, "ryzen 5") || str_contains_ci(str, "core i5")) {
        return 6;
    }
    if (str_contains_ci(str, "ryzen 3") || str_contains_ci(str, "core i3")) {
        return 4;
    }
    return 0;
}

char *url_encode(const char *str) {
    if (!str) return NULL;
    CURL *curl = curl_easy_init();
    if (!curl) return str_dup(str);
    char *escaped = curl_easy_escape(curl, str, 0);
    char *ret = str_dup(escaped ? escaped : str);
    if (escaped) curl_free(escaped);
    curl_easy_cleanup(curl);
    return ret;
}
