#include "widgetreq.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
/* The request is one flat JSON object written by Dockbar, then a settings
 * object (empty for this widget). Only top-level keys are read: a key
 * inside "settings" never counts. */
static const char *skip_ws(const char *p, const char *end) {
  while (p < end && isspace((unsigned char)*p))
    p++;
  return p;
}
static const char *string_end(const char *p, const char *end) {
  for (p++; p < end; p++) {
    if (*p == '\\')
      p++;
    else if (*p == '"')
      return p;
  }
  return NULL;
}
static int copy_string(const char *p, const char *e, char *out, size_t cap) {
  size_t n = 0;
  for (p++; p < e; p++) {
    char c = *p;
    if (c == '\\') {
      if (++p >= e)
        return 0;
      c = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p;
      if (*p == 'u')
        return 0; /* not used in the fields read here */
    }
    if (n + 1 >= cap)
      return 0;
    out[n++] = c;
  }
  out[n] = 0;
  return 1;
}
/* Skips one value; returns the byte after it or NULL. */
static const char *skip_value(const char *p, const char *end) {
  p = skip_ws(p, end);
  if (p >= end)
    return NULL;
  if (*p == '"') {
    p = string_end(p, end);
    return p ? p + 1 : NULL;
  }
  if (*p == '{' || *p == '[') {
    int depth = 0;
    for (; p < end; p++) {
      if (*p == '"') {
        if (!(p = string_end(p, end)))
          return NULL;
      } else if (*p == '{' || *p == '[')
        depth++;
      else if (*p == '}' || *p == ']') {
        if (--depth == 0)
          return p + 1;
      }
    }
    return NULL;
  }
  while (p < end && *p != ',' && *p != '}' && !isspace((unsigned char)*p))
    p++;
  return p;
}
int wr_parse(const char *text, size_t len, struct WidgetReq *q) {
  const char *p = text, *end = text + len;
  int have = 0;
  memset(q, 0, sizeof *q);
  p = skip_ws(p, end);
  if (p >= end || *p++ != '{')
    return 0;
  for (;;) {
    p = skip_ws(p, end);
    if (p < end && *p == '}')
      break;
    if (p >= end || *p != '"')
      return 0;
    const char *ke = string_end(p, end);
    if (!ke)
      return 0;
    char key[32];
    if ((size_t)(ke - p) >= sizeof key || !copy_string(p, ke, key, sizeof key))
      key[0] = 0;
    p = skip_ws(ke + 1, end);
    if (p >= end || *p++ != ':')
      return 0;
    p = skip_ws(p, end);
    const char *v = p, *ve = skip_value(p, end);
    if (!ve)
      return 0;
    char *field = !strcmp(key, "package")     ? q->package
                  : !strcmp(key, "instance")  ? q->instance
                  : !strcmp(key, "session")   ? q->session
                  : !strcmp(key, "operation") ? q->operation
                  : !strcmp(key, "action")    ? q->action
                                              : NULL;
    size_t cap = field == q->operation || field == q->action ? 40 : WR_ID;
    if (field) {
      if (*v != '"' || !copy_string(v, ve - 1, field, cap))
        return 0;
      have |= field == q->package ? 1 : field == q->instance ? 2
              : field == q->session ? 4 : 0;
    } else if (!strcmp(key, "generation")) {
      char *stop;
      unsigned long g = strtoul(v, &stop, 10);
      if (stop != ve || !g)
        return 0;
      q->generation = g;
      have |= 8;
    }
    p = skip_ws(ve, end);
    if (p < end && *p == ',') {
      p++;
      continue;
    }
    if (p < end && *p == '}')
      break;
    return 0;
  }
  return have == 15;
}
int wr_action(const struct WidgetReq *q) {
  if (strcmp(q->operation, "action"))
    return 0;
  return !strcmp(q->action, "area")     ? WR_AREA
         : !strcmp(q->action, "screen") ? WR_SCREEN
         : !strcmp(q->action, "delay")  ? WR_DELAY
         : !strcmp(q->action, "folder") ? WR_FOLDER
                                        : 0;
}
int wr_command(int action, const char *program, char *out, size_t cap) {
  const char *args = action == WR_AREA     ? ""
                     : action == WR_SCREEN ? " SCREEN NOTIFY"
                     : action == WR_DELAY  ? " SCREEN DELAY=5 NOTIFY"
                                           : NULL;
  if (!args || !program || !*program)
    return 0;
  for (const char *s = program; *s; s++)
    if (*s == '"' || (unsigned char)*s < 32 || *s == '*')
      return 0;
  int w = snprintf(out, cap, "\"%s\"%s", program, args);
  return w > 0 && (size_t)w < cap;
}
static void quoted(FILE *f, const char *s) {
  fputc('"', f);
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\')
      fprintf(f, "\\%c", c);
    else if (c < 32)
      fprintf(f, "\\u%04x", c);
    else
      fputc(c, f);
  }
  fputc('"', f);
}
int wr_respond(FILE *f, const struct WidgetReq *q, int ok, const char *message,
               long now) {
  fputs("{\"api\":2,\"package\":", f);
  quoted(f, q->package);
  fputs(",\"instance\":", f);
  quoted(f, q->instance);
  fputs(",\"session\":", f);
  quoted(f, q->session);
  fprintf(f,
          ",\"generation\":%lu,\"status\":\"%s\",\"updated\":%ld,"
          "\"sourceTime\":%ld,\"fingerprint\":\"\",\"title\":\"AROS Capture\","
          "\"message\":",
          q->generation, ok ? "ready" : "error", now, now);
  quoted(f, message ? message : "");
  fputs(",\"fields\":{}}\n", f);
  return !ferror(f);
}
