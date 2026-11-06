#include "names.h"

#include <stdio.h>
#include <string.h>

int cap_join(char *buf, size_t size, const char *dir, const char *file)
{
    size_t dl = strlen(dir), fl = strlen(file);
    int sep = dl > 0 && dir[dl - 1] != ':' && dir[dl - 1] != '/';

    if (dl + (size_t)sep + fl + 1 > size)
        return 0;
    memcpy(buf, dir, dl);
    if (sep)
        buf[dl++] = '/';
    memcpy(buf + dl, file, fl + 1);
    return 1;
}

int cap_name(char *buf, size_t size, const struct cap_time *t, unsigned seq)
{
    int n;

    if (seq <= 1)
        n = snprintf(buf, size, "Capture-%04d-%02d-%02d_%02d-%02d-%02d.png",
            t->year, t->month, t->day, t->hour, t->min, t->sec);
    else
        n = snprintf(buf, size, "Capture-%04d-%02d-%02d_%02d-%02d-%02d-%u.png",
            t->year, t->month, t->day, t->hour, t->min, t->sec, seq);
    return n > 0 && (size_t)n < size;
}

int cap_has_png_suffix(const char *name)
{
    size_t l = strlen(name);
    const char *s;

    if (l < 4)
        return 0;
    s = name + l - 4;
    return s[0] == '.' && (s[1] | 0x20) == 'p' && (s[2] | 0x20) == 'n' && (s[3] | 0x20) == 'g';
}
