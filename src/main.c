/* AROS Capture, Shell entry. */
#include "capture.h"
#include "probe.h"
#include "editor.h"
#include "notice.h"

#include <dos/dos.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>
#include <string.h>

static const char version[] __attribute__((used)) = "$VER: AROSCapture 0.1 (06.11.2026)";

#define TEMPLATE "SCREEN/S,REGION/S,DELAY/K/N,TO/K,DIR/K,PUBSCREEN/K,FORCE/S,QUIET/S,NOTIFY/S,PROBE/K"

enum { A_SCREEN, A_REGION, A_DELAY, A_TO, A_DIR, A_PUBSCREEN, A_FORCE, A_QUIET, A_NOTIFY, A_PROBE, A_COUNT };

/* No mode given (Workbench, Dockbar program item, plain Shell command):
   choose an area, then edit it. */
static int interactive(CONST_STRPTR pubscreen, LONG delay)
{
    struct cap_request req;
    struct cap_image img;
    LONG missing;
    char msg[768];
    enum cap_status st;

    memset(&req, 0, sizeof req);
    req.region = TRUE;
    req.delay = delay;
    req.pubscreen = pubscreen;
    req.settle = TICKS_PER_SECOND / 4;
    st = cap_take(&req, &img, &missing, msg, sizeof msg);
    if (st == CAP_CANCELLED)
        return RETURN_WARN;
    if (st != CAP_OK)
    {
        PutStr((STRPTR)"AROSCapture: ");
        PutStr((STRPTR)msg);
        PutStr((STRPTR)"\n");
        return RETURN_ERROR;
    }
#ifdef CAPTURE_NO_EDITOR
    (void)missing;
    cap_free(&img);
    PutStr((STRPTR)"AROSCapture: this build has no editor\n");
    return RETURN_ERROR;
#else
    return editor_run(&img, missing);
#endif
}

int main(int argc, char **argv)
{
    IPTR args[A_COUNT];
    struct RDArgs *rda;
    struct cap_request req;
    struct cap_result res;
    char line[800], folder[256];
    LONG delay;
    int rc;

    (void)argv;
    if (argc == 0)
        return interactive(NULL, 0);    /* started from Workbench */

    memset(args, 0, sizeof args);
    rda = ReadArgs((CONST_STRPTR)TEMPLATE, (SIPTR *)args, NULL);
    if (!rda)
    {
        PrintFault(IoErr(), (STRPTR)"AROSCapture");
        return RETURN_FAIL;
    }
    if (args[A_PROBE] && (args[A_SCREEN] || args[A_REGION] || args[A_DELAY] || args[A_TO] || args[A_DIR] ||
                          args[A_FORCE] || args[A_QUIET] || args[A_NOTIFY]))
    {
        PutStr((STRPTR)"AROSCapture: PROBE takes only PUBSCREEN\n");
        FreeArgs(rda);
        return RETURN_ERROR;
    }
    if (args[A_PROBE])
    {
        rc = cap_probe((CONST_STRPTR)args[A_PUBSCREEN], (CONST_STRPTR)args[A_PROBE]);
        FreeArgs(rda);
        return rc;
    }
    if (args[A_SCREEN] && args[A_REGION])
    {
        PutStr((STRPTR)"AROSCapture: give SCREEN or REGION, not both\n");
        FreeArgs(rda);
        return RETURN_ERROR;
    }
    if (args[A_TO] && args[A_DIR])
    {
        PutStr((STRPTR)"AROSCapture: give TO or DIR, not both\n");
        FreeArgs(rda);
        return RETURN_ERROR;
    }

    if (args[A_FORCE] && !args[A_TO])
    {
        PutStr((STRPTR)"AROSCapture: FORCE needs TO (generated names never replace a file)\n");
        FreeArgs(rda);
        return RETURN_ERROR;
    }

    /* Checked before the modes split: the editor path takes DELAY too. */
    delay = args[A_DELAY] ? *(LONG *)args[A_DELAY] : 0;
    if (delay < 0 || delay > 3600)
    {
        PutStr((STRPTR)"AROSCapture: DELAY must be 0 to 3600 seconds\n");
        FreeArgs(rda);
        return RETURN_ERROR;
    }

    if (!args[A_SCREEN] && !args[A_REGION])
    {
        if (args[A_TO] || args[A_DIR] || args[A_NOTIFY])
        {
            PutStr((STRPTR)"AROSCapture: TO, DIR, FORCE and NOTIFY need SCREEN or REGION\n");
            FreeArgs(rda);
            return RETURN_ERROR;
        }
        rc = interactive((CONST_STRPTR)args[A_PUBSCREEN], delay);
        FreeArgs(rda);
        return rc;
    }

    memset(&req, 0, sizeof req);
    req.region = args[A_REGION] != 0;
    req.delay = delay;
    req.to = (CONST_STRPTR)args[A_TO];
    cap_folder(folder, sizeof folder);
    req.dir = args[A_DIR] ? (CONST_STRPTR)args[A_DIR] : (CONST_STRPTR)folder;
    req.pubscreen = (CONST_STRPTR)args[A_PUBSCREEN];
    req.force = args[A_FORCE] != 0;
    req.create_dir = !args[A_DIR];
    /* Lets the Shell window or a launcher's feedback redraw out of the way. */
    req.settle = TICKS_PER_SECOND / 5;

    cap_run(&req, &res);
    if (res.status != CAP_OK || !args[A_QUIET] || res.missing > 0 || res.ioerr)
    {
        snprintf(line, sizeof line, "AROSCapture: %s\n", res.message);
        PutStr((STRPTR)line);
    }
    /* For callers without a Shell window, such as a Dockbar command item. A
       cancelled area choice needs no notice: the user did it. */
    if (args[A_NOTIFY] && res.status != CAP_CANCELLED)
    {
        if (res.status == CAP_OK)
        {
            const char *name = strrchr(res.path, '/');

            if (!name)
                name = strrchr(res.path, ':');
            snprintf(line, sizeof line, "Saved %s%s", name ? name + 1 : res.path,
                res.missing > 0 ? " (see-through windows not included)" : "");
        }
        else
            snprintf(line, sizeof line, "Not saved: %s", res.message);
        cap_notice((CONST_STRPTR)args[A_PUBSCREEN], line, 5 * TICKS_PER_SECOND / 2);
    }
    rc = res.status == CAP_OK ? RETURN_OK
       : res.status == CAP_CANCELLED ? RETURN_WARN : RETURN_ERROR;
    FreeArgs(rda);
    return rc;
}
