#include <string.h>
#include <stddef.h>
#include <stdint.h>

int64_t strfind(const char *src, const char *search, long n) {
    if (!src || !search || n < 0) return -1;
    size_t src_len = strlen(src);
    if ((size_t)n > src_len) return -1;
    const char *start = src + n;
    const char *found = strstr(start, search);
    if (!found) return -1;
    return (long)(found - src);
}

int64_t substr(char *dest, int64_t size, const char *src, int64_t pos, int64_t len) {
    if (!dest || !src || size <= 0 || pos < 0 || len < 0) return 0;
    size_t src_len = strlen(src);
    if ((size_t)pos > src_len) return 0;
    size_t copy_len = (size_t)len;
    if (pos + copy_len > src_len) copy_len = src_len - pos;
    if (copy_len >= (size_t)size) copy_len = (size_t)size - 1;
    memcpy(dest, src + pos, copy_len);
    dest[copy_len] = '\0';
    return (long)copy_len;
}

int64_t strat(const char *src, int64_t pos) {
    char c = *(src + pos);
    return (int64_t)c;
}