/* CaptureWidget: the provider of the AROS Capture widget for Dockbar
 * (widget API 2, actions only). Dockbar starts it as
 *   CaptureWidget "<request file>"
 * only when the user chooses an action. It starts AROS Capture as a
 * program of its own and answers at once: an area choice or the editor
 * may stay open far longer than Dockbar waits for an answer. */
#include "widgetreq.h"
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/workbench.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ENV_PROGRAM "AROSCapture/Program"
#define ENV_FOLDER "AROSCapture/Folder"
#define DEFAULT_FOLDER "SYS:Pictures/Captures"

static int exists(const char *path)
{
    BPTR lock = Lock((CONST_STRPTR)path, ACCESS_READ);
    if (!lock)
        return 0;
    UnLock(lock);
    return 1;
}

static int start(const char *command)
{
    BPTR in = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
    BPTR out = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    LONG rc;

    if (!in || !out)
    {
        if (in)
            Close(in);
        if (out)
            Close(out);
        return 0;
    }
    /* Asynchronous: the streams belong to the new process from here on.
       The new process is given no copy of this program's drawer (PROGDIR:):
       on ABIv11 2026.09 that copy stayed locked after both processes had
       ended, so Dockbar could not replace the widget's bin drawer in an
       update. AROS Capture gets its own PROGDIR: when it is loaded. */
    struct Process *me = (struct Process *)FindTask(NULL);
    BPTR home = me->pr_HomeDir;
    me->pr_HomeDir = BNULL;
    rc = SystemTags((CONST_STRPTR)command, SYS_Input, (IPTR)in, SYS_Output, (IPTR)out,
        SYS_Asynch, TRUE, SYS_UserShell, TRUE, NP_StackSize, 262144, NP_Name, (IPTR)"AROS Capture",
        TAG_DONE);
    me->pr_HomeDir = home;
    if (rc == -1)
    {
        Close(in);
        Close(out);
        return 0;
    }
    return 1;
}

static int act(int action, char *message, size_t cap)
{
    char program[512], folder[512], command[600];

    if (action == WR_FOLDER)
    {
        struct Library *WorkbenchBase;
        int ok = 0;

        if (GetVar((CONST_STRPTR)ENV_FOLDER, (STRPTR)folder, sizeof folder, GVF_GLOBAL_ONLY) <= 0)
            snprintf(folder, sizeof folder, "%s", DEFAULT_FOLDER);
        if (!exists(folder))
        {
            snprintf(message, cap, "No captures yet: %.200s does not exist", folder);
            return 0;
        }
        WorkbenchBase = OpenLibrary((CONST_STRPTR)"workbench.library", 44);
        if (WorkbenchBase)
        {
            ok = OpenWorkbenchObjectA((STRPTR)folder, NULL) != FALSE;
            CloseLibrary(WorkbenchBase);
        }
        snprintf(message, cap, ok ? "Opened %.200s" : "Could not open %.200s", folder);
        return ok;
    }
    if (GetVar((CONST_STRPTR)ENV_PROGRAM, (STRPTR)program, sizeof program, GVF_GLOBAL_ONLY) <= 0 ||
        !exists(program))
    {
        snprintf(message, cap, "AROS Capture was not found; run Add to Dockbar in its drawer again");
        return 0;
    }
    if (!wr_command(action, program, command, sizeof command) || !start(command))
    {
        snprintf(message, cap, "Could not start AROS Capture");
        return 0;
    }
    snprintf(message, cap, "%s", action == WR_AREA ? "Choose an area"
        : action == WR_SCREEN ? "Capturing the screen" : "Capturing the screen in 5 seconds");
    return 1;
}

int main(int argc, char **argv)
{
    struct WidgetReq q;
    char path[1100], done[1100], message[300] = "";
    char *text;
    long n;
    int ok = 0, action;
    FILE *f;

    if (argc != 2 || strlen(argv[1]) > 1000)
        return 20;
    f = fopen(argv[1], "rb");
    if (!f)
        return 20;
    text = malloc(65537);
    n = text ? (long)fread(text, 1, 65536, f) : 0;
    fclose(f);
    if (!text || n <= 0 || !wr_parse(text, (size_t)n, &q))
    {
        free(text);
        return 20;
    }
    free(text);
    action = wr_action(&q);
    if (action)
        ok = act(action, message, sizeof message);
    else
        snprintf(message, sizeof message, "This widget only runs when you choose an action");
    /* Written beside the request, then renamed: Dockbar only reads a
     * complete answer. */
    snprintf(path, sizeof path, "%s.tmp", argv[1]);
    snprintf(done, sizeof done, "%s.done", argv[1]);
    f = fopen(path, "wb");
    if (!f)
        return 20;
    int written = wr_respond(f, &q, ok, message, (long)time(NULL));
    if (fclose(f))
        written = 0;
    if (!written || rename(path, done))
    {
        remove(path);
        return 20;
    }
    return ok ? 0 : 10;
}
