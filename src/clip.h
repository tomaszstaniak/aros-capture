#ifndef CAPTURE_CLIP_H
#define CAPTURE_CLIP_H

#include <exec/types.h>

/* Puts a w x h ARGB picture on clipboard unit 0 as a 24-bit ILBM. */
BOOL clip_picture(const UBYTE *argb, LONG w, LONG h);
/* Puts text on clipboard unit 0 as FTXT. */
BOOL clip_text(const char *text);

#endif
