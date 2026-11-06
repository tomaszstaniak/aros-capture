/* Requests from Dockbar to the AROS Capture widget provider (Dockbar widget
 * API 2, docs/applets.md in the Dockbar repository). No AROS calls here:
 * tested on the host (tests/test_widgetreq.c). */
#ifndef CAPTURE_WIDGETREQ_H
#define CAPTURE_WIDGETREQ_H
#include <stddef.h>
#include <stdio.h>
#define WR_ID 80
struct WidgetReq {
  char package[WR_ID], instance[WR_ID], session[WR_ID], operation[40],
      action[40];
  unsigned long generation;
};
enum { WR_AREA = 1, WR_SCREEN, WR_DELAY, WR_FOLDER };
/* 1 when the request has the identity fields Dockbar checks. */
int wr_parse(const char *text, size_t len, struct WidgetReq *out);
/* WR_* for operation "action" with a known action, else 0. */
int wr_action(const struct WidgetReq *);
/* The Shell command for WR_AREA, WR_SCREEN and WR_DELAY: program quoted.
 * 0 when it does not fit or the path holds a quote or a control byte. */
int wr_command(int action, const char *program, char *out, size_t cap);
/* The response Dockbar reads (status ready, or error with ok 0). */
int wr_respond(FILE *, const struct WidgetReq *, int ok, const char *message,
               long now);
#endif
