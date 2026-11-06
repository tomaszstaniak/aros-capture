#ifndef CAPTURE_PROBE_H
#define CAPTURE_PROBE_H

#include <exec/types.h>

/* Writes the read-back evidence described in probe.c into dir. */
LONG cap_probe(CONST_STRPTR pubscreen, CONST_STRPTR dir);

#endif
