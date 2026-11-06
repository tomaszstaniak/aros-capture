/* A short notice on the screen for captures started without a Shell window
   (Dockbar command items): a small borderless window at the top of the
   screen that does not take the input focus and goes away after a few
   seconds or a click. */
#ifndef CAPTURE_NOTICE_H
#define CAPTURE_NOTICE_H

#include <exec/types.h>

/* Shows text on the public screen pubscreen (NULL: the default one) for
   about ticks ticks. Returns at once without a notice when no window can be
   opened. */
void cap_notice(CONST_STRPTR pubscreen, const char *text, LONG ticks);

#endif
