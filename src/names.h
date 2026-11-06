/* File name generation for saved captures. Pure C, no AROS calls, so the
   rules can be tested on the build host. */
#ifndef CAPTURE_NAMES_H
#define CAPTURE_NAMES_H

#include <stddef.h>

struct cap_time
{
    int year, month, day, hour, min, sec;
};

/* Joins an AmigaDOS directory and a file name: nothing is inserted after a
   volume or assign (":") or a trailing "/", and an empty directory means the
   current directory. Returns 0 when the result does not fit. */
int cap_join(char *buf, size_t size, const char *dir, const char *file);

/* "Capture-YYYY-MM-DD_HH-MM-SS.png" for seq 1, "...-<seq>.png" for seq 2
   and later. Returns 0 when the result does not fit. */
int cap_name(char *buf, size_t size, const struct cap_time *t, unsigned seq);

/* True when the name ends with ".png" in any letter case. */
int cap_has_png_suffix(const char *name);

#endif
