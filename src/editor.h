#ifndef CAPTURE_EDITOR_H
#define CAPTURE_EDITOR_H

#include "capture.h"

/* Opens the editor on img (taken over: freed by the editor) and runs it
   until the window is closed. Returns a Shell return code. */
int editor_run(struct cap_image *img, LONG missing);

#endif
