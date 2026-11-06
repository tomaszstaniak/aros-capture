/* cc -I src tests/test_widgetreq.c src/widgetreq.c
 * With -DWITH_DOCKBAR -I<dockbar>/src and Dockbar's src/applets/json.c and
 * snapshot.c, the response is also read with Dockbar's own parser. */
#include "widgetreq.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WITH_DOCKBAR
#include "applets/snapshot.h"
#endif
int main(void) {
  struct WidgetReq q;
  char cmd[300];
  /* As Dockbar 0.6 writes it (worker_aros.c request()). */
  const char *req =
      "{\"api\":2,\"package\":\"org.aros.capture\",\"packageRoot\":"
      "\"Work:Dockbar/Widgets/org.aros.capture\",\"instance\":\"i-1-2\","
      "\"session\":\".s-9\",\"generation\":3,\"operation\":\"action\","
      "\"action\":\"screen\",\"query\":\"\",\"settings\":{}}\n";
  assert(wr_parse(req, strlen(req), &q));
  assert(!strcmp(q.package, "org.aros.capture") && !strcmp(q.instance, "i-1-2") &&
         !strcmp(q.session, ".s-9") && q.generation == 3 &&
         wr_action(&q) == WR_SCREEN);
  /* A key inside settings does not count; other actions; refresh. */
  const char *nested =
      "{\"package\":\"p\",\"instance\":\"i\",\"session\":\"s\",\"generation\":1,"
      "\"operation\":\"update\",\"settings\":{\"action\":\"area\",\"x\":[1,{\"y\":\"}\"}]}}";
  assert(wr_parse(nested, strlen(nested), &q) && !q.action[0] && !wr_action(&q));
  const char *ops[] = {"area", "delay", "folder", "other"};
  int want[] = {WR_AREA, WR_DELAY, WR_FOLDER, 0};
  for (int i = 0; i < 4; i++) {
    char r[300];
    snprintf(r, sizeof r,
             "{\"package\":\"p\",\"instance\":\"i\",\"session\":\"s\","
             "\"generation\":1,\"operation\":\"action\",\"action\":\"%s\"}",
             ops[i]);
    assert(wr_parse(r, strlen(r), &q) && wr_action(&q) == want[i]);
  }
  /* Missing identity, bad generation, broken JSON. */
  const char *bad[] = {
      "{\"package\":\"p\",\"instance\":\"i\",\"generation\":1}",
      "{\"package\":\"p\",\"instance\":\"i\",\"session\":\"s\",\"generation\":0}",
      "{\"package\":\"p\",\"instance\":\"i\",\"session\":\"s\",\"generation\":1",
      "{\"package\":1,\"instance\":\"i\",\"session\":\"s\",\"generation\":1}",
      "[]", ""};
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
    assert(!wr_parse(bad[i], strlen(bad[i]), &q));
  /* Commands. */
  assert(wr_command(WR_AREA, "Work:AROSCapture/AROSCapture", cmd, sizeof cmd) &&
         !strcmp(cmd, "\"Work:AROSCapture/AROSCapture\""));
  assert(wr_command(WR_SCREEN, "Work:A B/AROSCapture", cmd, sizeof cmd) &&
         !strcmp(cmd, "\"Work:A B/AROSCapture\" SCREEN NOTIFY"));
  assert(wr_command(WR_DELAY, "RAM:x", cmd, sizeof cmd) &&
         !strcmp(cmd, "\"RAM:x\" SCREEN DELAY=5 NOTIFY"));
  assert(!wr_command(WR_FOLDER, "RAM:x", cmd, sizeof cmd));
  assert(!wr_command(WR_AREA, "RAM:\"x", cmd, sizeof cmd));
  assert(!wr_command(WR_AREA, "RAM:x\n", cmd, sizeof cmd));
  assert(!wr_command(WR_AREA, "", cmd, sizeof cmd));
  assert(!wr_command(WR_AREA, "RAM:x", cmd, 5));
  /* Response. */
  assert(wr_parse(req, strlen(req), &q));
  FILE *f = tmpfile();
  assert(f && wr_respond(f, &q, 1, "Saved \"a\\b\"\n", 1000));
  long n = ftell(f);
  char *text = calloc(1, (size_t)n + 1);
  rewind(f);
  assert(text && fread(text, 1, (size_t)n, f) == (size_t)n);
  fclose(f);
  assert(strstr(text, "\"status\":\"ready\"") && strstr(text, "\"generation\":3"));
#ifdef WITH_DOCKBAR
  struct DockAppletSnapshot *s = calloc(1, sizeof *s);
  assert(s && dock_applet_snapshot_parse(text, strlen(text), s));
  assert(s->status == DA_READY && s->generation == 3 &&
         !strcmp(s->message, "Saved \"a\\b\"\n") && !strcmp(s->session, ".s-9"));
  free(s);
  puts("widgetreq: Dockbar parser reads the response");
#endif
  free(text);
  puts("widgetreq: passed");
  return 0;
}
