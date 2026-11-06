/* AddToDockbar: installs the AROS Capture widget into a running Dockbar.
 *
 *   AddToDockbar [DOCKBAR=<Dockbar program>] [QUIET]
 *
 * Dockbar 0.6.0 and later install the package themselves when asked on
 * their widget port (dockbar_widgets.h): they check it, copy it into the
 * widget drawer they really use, and offer to put it on a dock. This
 * program only records where AROS Capture is, finds or starts Dockbar,
 * and asks. Nothing is sent to an older Dockbar.
 *
 * Where AROS Capture is (ENV:AROSCapture/Program, read by the widget) is
 * written only after Dockbar installed or updated the widget.
 *
 * Return codes: 0 installed or updated, 5 skipped (Dockbar not running and
 * none chosen), 10 Dockbar refused or failed, or is too old, 20 wrong
 * arguments or the AROS Capture drawer is incomplete. */
#include "dockbar_widgets.h"
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/rdargs.h>
#include <dos/var.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <workbench/startup.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>

#define ENV_PROGRAM "AROSCapture/Program"
#define WIDGET_DRAWER "Widget/org.aros.capture"

static int quiet, from_shell;

static void tell(const char *text)
{
    if (quiet)
        return;
    if (from_shell)
    {
        printf("%s\n", text);
        return;
    }
    struct EasyStruct es = {sizeof(struct EasyStruct), 0, (CONST_STRPTR)"AROS Capture",
        (CONST_STRPTR)"%s", (CONST_STRPTR)"OK"};
    EasyRequest(NULL, &es, NULL, (IPTR)text);
}

static int ask(const char *text, const char *gadgets)
{
    struct EasyStruct es = {sizeof(struct EasyStruct), 0, (CONST_STRPTR)"AROS Capture",
        (CONST_STRPTR)"%s", (CONST_STRPTR)gadgets};
    return EasyRequest(NULL, &es, NULL, (IPTR)text);
}

static BOOL full_path(const char *name, char *out, LONG cap)
{
    BPTR lock = Lock((CONST_STRPTR)name, ACCESS_READ);
    BOOL ok;

    if (!lock)
        return FALSE;
    ok = NameFromLock(lock, (STRPTR)out, cap);
    UnLock(lock);
    return ok;
}

static BOOL ensure_drawer(const char *path)
{
    BPTR lock = Lock((CONST_STRPTR)path, ACCESS_READ);

    if (!lock)
        lock = CreateDir((CONST_STRPTR)path);
    if (!lock)
        return FALSE;
    UnLock(lock);
    return TRUE;
}

/* The provider finds AROS Capture from this. ENVARC: may be write-protected;
 * the widget then works until the next restart, which is said. */
static int remember_program(const char *program)
{
    char back[512];

    if (!ensure_drawer("ENV:AROSCapture") ||
        !SetVar((CONST_STRPTR)ENV_PROGRAM, (CONST_STRPTR)program, -1, GVF_GLOBAL_ONLY))
        return 0;
    if (!ensure_drawer("ENVARC:AROSCapture") ||
        !SetVar((CONST_STRPTR)ENV_PROGRAM, (CONST_STRPTR)program, -1, GVF_GLOBAL_ONLY | GVF_SAVE_VAR))
        return 1;
    BPTR fh = Open((CONST_STRPTR)"ENVARC:" ENV_PROGRAM, MODE_OLDFILE);
    LONG n = 0;
    if (fh)
    {
        n = Read(fh, back, sizeof back);
        Close(fh);
    }
    return n == (LONG)strlen(program) && !memcmp(back, program, n) ? 2 : 1;
}

/* Sends one request; FALSE when no widget port exists. */
static BOOL send(struct DockbarWidgetsMsg *m)
{
    struct MsgPort *reply = CreateMsgPort(), *port;

    if (!reply)
        return FALSE;
    m->msg.mn_ReplyPort = reply;
    m->msg.mn_Length = sizeof *m;
    m->magic = DOCKBAR_WIDGETS_MAGIC;
    m->version = DOCKBAR_WIDGETS_VERSION;
    Forbid();
    port = FindPort((CONST_STRPTR)DOCKBAR_WIDGETS_PORT);
    if (port)
        PutMsg(port, &m->msg);
    Permit();
    if (port)
    {
        WaitPort(reply);
        GetMsg(reply);
    }
    DeleteMsgPort(reply);
    return port != NULL;
}

static BOOL running(const char *name)
{
    BOOL found;

    Forbid();
    found = FindPort((CONST_STRPTR)name) != NULL;
    Permit();
    return found;
}

static BOOL choose_dockbar(char *out, LONG cap)
{
    struct Library *AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 37);
    struct FileRequester *fr;
    BOOL ok = FALSE;

    if (!AslBase)
        return FALSE;
    fr = AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, (IPTR)"Choose the Dockbar program",
        ASLFR_InitialDrawer, (IPTR)"SYS:", TAG_DONE);
    if (fr && AslRequestTags(fr, TAG_DONE))
    {
        strncpy(out, (const char *)fr->fr_Drawer, cap - 1);
        out[cap - 1] = 0;
        ok = AddPart((STRPTR)out, fr->fr_File, cap);
    }
    if (fr)
        FreeAslRequest(fr);
    CloseLibrary(AslBase);
    return ok && out[0];
}

/* Starts Dockbar and waits up to 10 seconds for one of its ports. */
static BOOL start_dockbar(const char *program)
{
    char command[600];

    if (strchr(program, '"') || snprintf(command, sizeof command, "\"%s\"", program) >= (int)sizeof command)
        return FALSE;
    BPTR in = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE), out = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    if (!in || !out || SystemTags((CONST_STRPTR)command, SYS_Input, (IPTR)in, SYS_Output, (IPTR)out,
        SYS_Asynch, TRUE, SYS_UserShell, TRUE, NP_StackSize, 262144, TAG_DONE) == -1)
    {
        if (in)
            Close(in);
        if (out)
            Close(out);
        return FALSE;
    }
    for (int i = 0; i < 20; i++)
    {
        if (running(DOCKBAR_WIDGETS_PORT) || running("Dockbar"))
            return TRUE;
        Delay(25);
    }
    return FALSE;
}

int main(int argc, char **argv)
{
    char program[512], widget[512], dockbar[512] = "", text[400];
    static struct DockbarWidgetsMsg m;
    IPTR args[2] = {0, 0};
    int remembered;
    struct RDArgs *rd = NULL;

    (void)argv;
    from_shell = argc > 0;
    if (from_shell)
    {
        rd = ReadArgs((CONST_STRPTR)"DOCKBAR/K,QUIET/S", (SIPTR *)args, NULL);
        if (!rd)
        {
            PrintFault(IoErr(), (CONST_STRPTR)"AddToDockbar");
            return 20;
        }
        if (args[0])
            snprintf(dockbar, sizeof dockbar, "%s", (const char *)args[0]);
        quiet = args[1] != 0;
        FreeArgs(rd);
    }
    if (!full_path("PROGDIR:AROSCapture", program, sizeof program) ||
        !full_path("PROGDIR:" WIDGET_DRAWER, widget, sizeof widget))
    {
        tell("The AROS Capture drawer is incomplete: AROSCapture or " WIDGET_DRAWER " is missing.");
        return 20;
    }

    if (!running(DOCKBAR_WIDGETS_PORT))
    {
        if (running("Dockbar"))
        {
            tell("This Dockbar is too old to install widgets from other programs. "
                 "Dockbar 0.6.0 or newer is needed.");
            return 10;
        }
        if (!dockbar[0])
        {
            if (quiet || from_shell)
            {
                tell("Dockbar is not running. Start it, or name it with DOCKBAR=.");
                return 5;
            }
            if (ask("Dockbar is not running.\nChoose the Dockbar program to start it,\n"
                    "or skip adding the widget.", "Choose Dockbar...|Skip") != 1 ||
                !choose_dockbar(dockbar, sizeof dockbar))
                return 5;
        }
        if (!start_dockbar(dockbar))
        {
            snprintf(text, sizeof text, "Dockbar did not start from %.300s.", dockbar);
            tell(text);
            return 10;
        }
        if (!running(DOCKBAR_WIDGETS_PORT))
        {
            tell("This Dockbar is too old to install widgets from other programs. "
                 "Dockbar 0.6.0 or newer is needed.");
            return 10;
        }
    }

    memset(&m, 0, sizeof m);
    m.command = DOCKBAR_WIDGETS_INSTALL;
    m.path = (CONST_STRPTR)widget;
    m.flags = DOCKBAR_WIDGETS_OFFER_ADD;
    if (!send(&m))
    {
        tell("Dockbar quit before it could be asked.");
        return 10;
    }
    m.text[sizeof m.text - 1] = 0;
    switch (m.result)
    {
    case DOCKBAR_WIDGETS_OK:
    case DOCKBAR_WIDGETS_UPDATED:
        /* Only now: a skipped, refused or busy install leaves the widget
         * that is already there pointing at the copy it used before. */
        remembered = remember_program(program);
        if (!remembered)
        {
            tell("The widget was installed, but where AROS Capture is could not be "
                 "stored (ENV:AROSCapture). Run Add to Dockbar again.");
            return 10;
        }
        snprintf(text, sizeof text, "%s%s",
            m.result == DOCKBAR_WIDGETS_OK ? "The AROS Capture widget was added to Dockbar's widgets."
                                           : "The AROS Capture widget in Dockbar was updated.",
            remembered == 2 ? "" : "\nENVARC: could not be written: after a restart, run Add to Dockbar again.");
        tell(text);
        return 0;
    case DOCKBAR_WIDGETS_BUSY:
        snprintf(text, sizeof text, "Dockbar is using the widget right now. Try again in a moment.");
        break;
    default:
        snprintf(text, sizeof text, "Dockbar did not install the widget: %s", m.text[0] ? m.text : "no reason given");
        break;
    }
    tell(text);
    return 10;
}
