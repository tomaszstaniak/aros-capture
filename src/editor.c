/* The editor window: a capture with a toolbar of actions and drawing tools,
   the colour under the pointer, the picture's size and the zoom, after the
   layout of Shottr's editor. Built from zunekit components (zk_toolbar,
   zk_imageview, zk_dragout) on standard Zune. */
#include "editor.h"
#include "annot.h"
#include "capture.h"
#include "clip.h"

#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <utility/hooks.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include <stdio.h>
#include <string.h>

#include "zk_dragout.h"
#include "zk_imageview.h"
#include "zk_toolbar.h"

#define ENV_FOLDER CAP_ENV_FOLDER
#define DEFAULT_FOLDER CAP_DEFAULT_FOLDER
#define ICONS "PROGDIR:Icons/"

enum
{
    ACT_CAPTURE = 1, ACT_COPY, ACT_SAVE, ACT_UNDO, ACT_SETTINGS,
    TOOL_SELECT = 20, TOOL_RECT, TOOL_ELLIPSE, TOOL_LINE, TOOL_ARROW, TOOL_PENCIL
};

enum
{
    ID_TOOL = 100, ID_PRESSED, ID_POINTER, ID_ZOOM, ID_TAB, ID_DRAGOUT,
    ID_SETTINGS_SAVE, ID_SETTINGS_CANCEL, ID_UNDO_KEY
};

static const struct zk_toolbar_item tools[] =
{
    { ZK_TB_ACTION, 'n', ACT_CAPTURE, ICONS "screenshot.svg", "New", "New capture (N)" },
    { ZK_TB_ACTION, 'c', ACT_COPY, ICONS "copy.svg", "Copy", "Copy the picture (C)" },
    { ZK_TB_ACTION, 's', ACT_SAVE, ICONS "device-floppy.svg", "Save", "Save to the capture folder (S)" },
    { ZK_TB_SEPARATOR, 0, 0, NULL, NULL, NULL },
    { ZK_TB_TOOL, 'v', TOOL_SELECT, ICONS "pointer.svg", "Drag", "Drag the picture out to a drawer (V)" },
    { ZK_TB_TOOL, 'r', TOOL_RECT, ICONS "square.svg", "Rect", "Rectangle (R)" },
    { ZK_TB_TOOL, 'o', TOOL_ELLIPSE, ICONS "circle.svg", "Oval", "Ellipse (O)" },
    { ZK_TB_TOOL, 'l', TOOL_LINE, ICONS "line.svg", "Line", "Line (L)" },
    { ZK_TB_TOOL, 'a', TOOL_ARROW, ICONS "arrow-up-right.svg", "Arrow", "Arrow (A)" },
    { ZK_TB_TOOL, 'p', TOOL_PENCIL, ICONS "pencil.svg", "Pen", "Pencil (P)" },
    { ZK_TB_SEPARATOR, 0, 0, NULL, NULL, NULL },
    { ZK_TB_ACTION, 'z', ACT_UNDO, ICONS "arrow-back-up.svg", "Undo", "Undo (Z)" },
    { ZK_TB_ACTION, ',', ACT_SETTINGS, ICONS "settings.svg", "Settings", "Settings (,)" },
    { ZK_TB_SPACE, 0, 0, NULL, NULL, NULL }
};

struct editor
{
    Object *app, *win, *toolbar, *view, *swatch, *hex, *size, *zoom;
    Object *setwin, *folder;
    struct cap_image base;      /* the capture */
    UBYTE *shown;               /* capture with annotations */
    struct annot_list annots;
    struct annot *drawing;
    LONG missing;
    ULONG tool;
    ULONG colour;
    int width;
    BOOL pressed, dragged;
    LONG press_x, press_y;
    char folder_path[256];
    char title[700];
    char hex_text[48], size_text[48], zoom_text[32], swatch_spec[40];
    char hexcode[8];
    struct Hook input_hook;
};

/* ---- settings ---- */

static void load_settings(struct editor *ed)
{
    cap_folder(ed->folder_path, sizeof ed->folder_path);
}

static BOOL ensure_drawer(const char *name)
{
    BPTR lock = Lock((CONST_STRPTR)name, SHARED_LOCK);

    if (!lock)
        lock = CreateDir((CONST_STRPTR)name);
    if (!lock)
        return FALSE;
    UnLock(lock);
    return TRUE;
}

/* TRUE when the file holds exactly value (the form SetVar writes). */
static BOOL file_holds(const char *file, const char *value)
{
    char buf[300];
    LONG n, len = strlen(value);
    BPTR fh = Open((CONST_STRPTR)file, MODE_OLDFILE);

    if (!fh)
        return FALSE;
    n = Read(fh, buf, sizeof buf);
    Close(fh);
    return n == len && memcmp(buf, value, len) == 0;
}

/* Stores the folder, and uses it only when it was stored: after a failure
   the program keeps saving where it did before. SetVar reports success
   even when the ENVARC: copy could not be written (a write-protected
   ENVARC:), so both copies are read back. */
static BOOL save_settings(struct editor *ed, const char *folder)
{
    char value[sizeof ed->folder_path];

    snprintf(value, sizeof value, "%s", folder[0] ? folder : DEFAULT_FOLDER);
    /* SetVar does not create the variable's drawer: without ENV:AROSCapture
       and ENVARC:AROSCapture it fails. */
    if (!ensure_drawer("ENV:AROSCapture") || !ensure_drawer("ENVARC:AROSCapture"))
        return FALSE;
    if (!SetVar((CONST_STRPTR)ENV_FOLDER, (CONST_STRPTR)value, -1, GVF_GLOBAL_ONLY | GVF_SAVE_VAR) ||
        !file_holds("ENV:" ENV_FOLDER, value) || !file_holds("ENVARC:" ENV_FOLDER, value))
    {
        /* The running system goes on with the folder in use before. */
        SetVar((CONST_STRPTR)ENV_FOLDER, (CONST_STRPTR)ed->folder_path, -1, GVF_GLOBAL_ONLY);
        return FALSE;
    }
    memcpy(ed->folder_path, value, sizeof value);
    return TRUE;
}

/* ---- picture ---- */

static void set_title(struct editor *ed, const char *what)
{
    if (ed->missing > 0)
        snprintf(ed->title, sizeof ed->title, "AROS Capture - %s (%ld see-through window(s) not included)",
            what, (long)ed->missing);
    else
        snprintf(ed->title, sizeof ed->title, "AROS Capture - %s", what);
    set(ed->win, MUIA_Window_Title, (IPTR)ed->title);
}

static void recompose(struct editor *ed, struct annot_box box)
{
    box = annot_clip(box, ed->base.width, ed->base.height);
    if (box.w <= 0 || box.h <= 0)
        return;
    annot_compose(ed->shown, ed->base.pixels, ed->base.width, ed->base.height, &ed->annots, box);
    zk_imageview_changed(ed->view, box.x, box.y, box.w, box.h);
}

static BOOL set_picture(struct editor *ed, struct cap_image *img, LONG missing)
{
    UBYTE *shown = AllocVec((ULONG)img->width * img->height * 4, MEMF_ANY);

    if (!shown)
        return FALSE;
    zk_imageview_set_image(ed->view, NULL, 0, 0);
    if (ed->shown)
        FreeVec(ed->shown);
    cap_free(&ed->base);
    annot_clear(&ed->annots);
    ed->drawing = NULL;
    ed->base = *img;
    ed->shown = shown;
    ed->missing = missing;
    CopyMem(ed->base.pixels, ed->shown, (ULONG)img->width * img->height * 4);
    zk_imageview_set_image(ed->view, ed->shown, img->width, img->height);
    snprintf(ed->size_text, sizeof ed->size_text, "\33b%ldx%ld\33n\nImage size",
        (long)img->width, (long)img->height);
    set(ed->size, MUIA_Text_Contents, (IPTR)ed->size_text);
    set_title(ed, "new capture");
    return TRUE;
}

static int write_png(void *ctx, const char *path, char *why, size_t whyn)
{
    struct editor *ed = ctx;
    struct cap_image img = { ed->shown, ed->base.width, ed->base.height };
    LONG err;
    char msg[768];

    if (cap_save_png(&img, 0, 0, img.width, img.height, (CONST_STRPTR)path, FALSE, &err, msg, sizeof msg))
        return 1;
    snprintf(why, whyn, "%s", msg);
    return 0;
}

/* ---- actions ---- */

/* Ends the gesture in progress, if any: the shape is kept unless cancel is
   set or it is too small to keep. Also used when something else cuts a
   gesture short (a new capture, a drag-out, a press whose release before it
   was lost), so that no later pointer move extends a stale shape. */
static void end_gesture(struct editor *ed, BOOL cancel)
{
    struct annot_box before;

    ed->pressed = FALSE;
    ed->dragged = FALSE;
    if (ed->drawing)
    {
        before = annot_bounds(ed->drawing);
        if (cancel || annot_is_empty(ed->drawing))
            annot_pop(&ed->annots);
        ed->drawing = NULL;
        recompose(ed, before);
    }
}

/* Undo from the toolbar or a key; during a drawing gesture it cancels that
   gesture, and the release that follows finds nothing to finish. */
static void do_undo(struct editor *ed)
{
    struct annot_box gone;

    if (annot_undo(&ed->annots, &ed->drawing, &gone))
        recompose(ed, gone);
}

static void do_save(struct editor *ed)
{
    struct cap_image img = { ed->shown, ed->base.width, ed->base.height };
    char path[512], msg[768];
    LONG err = 0;
    int tries;

    /* A name taken between the choice and the write gets the next one. */
    for (tries = 0; tries < 5; tries++)
    {
        if (!cap_unique_path((CONST_STRPTR)ed->folder_path, TRUE, path, sizeof path, msg, sizeof msg))
        {
            set_title(ed, msg);
            return;
        }
        if (cap_save_png(&img, 0, 0, img.width, img.height, (CONST_STRPTR)path, FALSE, &err, msg, sizeof msg))
        {
            snprintf(msg, sizeof msg, "saved %s", path);
            set_title(ed, msg);
            return;
        }
        if (err != ERROR_OBJECT_EXISTS)
            break;
    }
    set_title(ed, msg);
}

static void do_copy(struct editor *ed)
{
    set_title(ed, clip_picture(ed->shown, ed->base.width, ed->base.height)
        ? "copied to the clipboard" : "copying to the clipboard failed");
}

static void do_capture(struct editor *ed)
{
    struct cap_request req;
    struct cap_image img;
    LONG missing;
    char msg[768];
    enum cap_status st;

    /* Closing the window ends the view's press: end the gesture here too. */
    end_gesture(ed, FALSE);
    memset(&req, 0, sizeof req);
    req.region = TRUE;
    /* Lets the windows behind the editor redraw before the screen is read. */
    req.settle = TICKS_PER_SECOND / 3;
    set(ed->win, MUIA_Window_Open, FALSE);
    st = cap_take(&req, &img, &missing, msg, sizeof msg);
    set(ed->win, MUIA_Window_Open, TRUE);
    if (st == CAP_OK)
    {
        if (!set_picture(ed, &img, missing))
        {
            cap_free(&img);
            set_title(ed, "not enough memory for the new capture");
        }
    }
    else if (st == CAP_FAILED)
        set_title(ed, msg);
}

static void do_dragout(struct editor *ed)
{
    struct Window *w = NULL;
    struct zk_dragout_cfg cfg;
    char name[64], path[512], why[300], msg[900];
    enum zk_dragout_result r;

    get(ed->win, MUIA_Window_Window, &w);
    if (!w)
        return;
    /* A shape still being drawn is finished as it is now. */
    if (ed->drawing)
        end_gesture(ed, FALSE);
    /* The dropped file gets a generated name, as a save would. */
    cap_new_name(name, sizeof name);
    memset(&cfg, 0, sizeof cfg);
    cfg.thumb = ed->shown;
    cfg.thumb_w = ed->base.width;
    cfg.thumb_h = ed->base.height;
    cfg.file_name = name;
    cfg.write_file = write_png;
    cfg.ctx = ed;
    cfg.temp_dir = "T:";
    r = zk_dragout_run(w, &cfg, path, sizeof path, why, sizeof why);
    switch (r)
    {
    case ZK_DRAGOUT_DRAWER:
    case ZK_DRAGOUT_APPWINDOW:
        snprintf(msg, sizeof msg, "%s", why);
        break;
    case ZK_DRAGOUT_CANCELLED:
        return;
    default:
        snprintf(msg, sizeof msg, "%s", why);
        break;
    }
    set_title(ed, msg);
}

static void show_pointer(struct editor *ed)
{
    LONG x = -1, y = -1;
    ULONG argb;

    get(ed->view, MUIA_ZkImageView_PointerX, &x);
    get(ed->view, MUIA_ZkImageView_PointerY, &y);
    if (x < 0 || y < 0)
        return;     /* keep the last colour, as Shottr does */
    argb = zk_imageview_pixel(ed->view, x, y);
    snprintf(ed->hexcode, sizeof ed->hexcode, "%02X%02X%02X",
        (unsigned)((argb >> 16) & 0xFF), (unsigned)((argb >> 8) & 0xFF), (unsigned)(argb & 0xFF));
    snprintf(ed->hex_text, sizeof ed->hex_text, "\33b#%s\33n\nTab to copy", ed->hexcode);
    set(ed->hex, MUIA_Text_Contents, (IPTR)ed->hex_text);
    /* MUI image spec 2: three 32-bit colour components. */
    snprintf(ed->swatch_spec, sizeof ed->swatch_spec, "2:%08x,%08x,%08x",
        (unsigned)(((argb >> 16) & 0xFF) * 0x01010101U), (unsigned)(((argb >> 8) & 0xFF) * 0x01010101U),
        (unsigned)((argb & 0xFF) * 0x01010101U));
    set(ed->swatch, MUIA_Background, (IPTR)ed->swatch_spec);
}

static void show_zoom(struct editor *ed)
{
    LONG shown = 100;

    get(ed->view, MUIA_ZkImageView_Shown, &shown);
    snprintf(ed->zoom_text, sizeof ed->zoom_text, "\33b%ld%%\33n\nZoom", (long)shown);
    set(ed->zoom, MUIA_Text_Contents, (IPTR)ed->zoom_text);
}

/* ---- drawing input ---- */

static int tool_kind(ULONG tool)
{
    switch (tool)
    {
    case TOOL_RECT: return ANNOT_RECT;
    case TOOL_ELLIPSE: return ANNOT_ELLIPSE;
    case TOOL_LINE: return ANNOT_LINE;
    case TOOL_ARROW: return ANNOT_ARROW;
    case TOOL_PENCIL: return ANNOT_PENCIL;
    }
    return -1;
}

static IPTR input_func(struct Hook *hook, Object *obj, struct zk_imageview_input_msg *m)
{
    struct editor *ed = hook->h_Data;
    struct annot_box before;

    (void)obj;
    switch (m->event)
    {
    case ZK_IV_PRESS:
        end_gesture(ed, FALSE);
        ed->pressed = TRUE;
        ed->press_x = m->x;
        ed->press_y = m->y;
        if (ed->tool == TOOL_SELECT)
            return 1;
        ed->drawing = annot_begin(&ed->annots, tool_kind(ed->tool), ed->colour, ed->width, m->x, m->y);
        if (ed->drawing)
            recompose(ed, annot_bounds(ed->drawing));
        return 1;
    case ZK_IV_MOVE:
        if (!ed->pressed)
            return 0;
        if (ed->tool == TOOL_SELECT)
        {
            LONG dx = m->x - ed->press_x, dy = m->y - ed->press_y;

            if (!ed->dragged && dx * dx + dy * dy >= 36)
            {
                /* The drag runs from the main loop, outside Zune's input
                   handling. */
                ed->dragged = TRUE;
                DoMethod(ed->app, MUIM_Application_ReturnID, ID_DRAGOUT);
            }
            return 1;
        }
        if (ed->drawing)
        {
            /* A pencil stroke redraws its newest segment, a shape all of
               itself (where it was and where it is now). */
            before = annot_tail_bounds(ed->drawing);
            annot_extend(ed->drawing, m->x, m->y);
            recompose(ed, annot_union(before, annot_tail_bounds(ed->drawing)));
        }
        return 1;
    case ZK_IV_RELEASE:
    case ZK_IV_CANCEL:
        end_gesture(ed, m->event == ZK_IV_CANCEL);
        return 1;
    }
    return 0;
}

/* ---- window ---- */

/* A two-line status cell; template fixes its width (the widest text it
   will show), so the row does not jump while the values change. */
static Object *status_text(const char *template)
{
    return TextObject,
        MUIA_Text_Contents, (IPTR)template,
        MUIA_Text_SetMin, TRUE,
        MUIA_Text_SetMax, TRUE,
        MUIA_Weight, 0,
    End;
}

static BOOL build(struct editor *ed)
{
    Object *ok, *cancel;

    ed->input_hook.h_Entry = (HOOKFUNC)HookEntry;
    ed->input_hook.h_SubEntry = (HOOKFUNC)input_func;
    ed->input_hook.h_Data = ed;

    ed->toolbar = zk_toolbar_new(tools, sizeof tools / sizeof tools[0], NULL);
    {
        struct TagItem tags[] =
        {
            { MUIA_ZkImageView_Background, 0x00202022 },
            { MUIA_ZkImageView_InputHook, (IPTR)&ed->input_hook },
            { TAG_DONE, 0 }
        };
        ed->view = zk_imageview_new(tags);
    }
    if (!ed->toolbar || !ed->view)
        return FALSE;

    ed->swatch = RectangleObject,
        MUIA_FixWidth, 18, MUIA_FixHeight, 18,
        MUIA_Frame, MUIV_Frame_Gauge,
        MUIA_Background, (IPTR)"2:00000000,00000000,00000000",
    End;
    ed->hex = status_text("\33b#000000\33n\nTab to copy");
    ed->size = status_text("\33b00000x00000\33n\nImage size");
    ed->zoom = status_text("\33b0000%\33n\nZoom");
    if (!ed->swatch || !ed->hex || !ed->size || !ed->zoom)
        return FALSE;
    zk_toolbar_add(ed->toolbar, HGroup, MUIA_Group_Spacing, 4,
        Child, VGroup, Child, VSpace(0), Child, ed->swatch, Child, VSpace(0), End,
        Child, ed->hex, End);
    /* Cells apart as in Shottr's status area. */
    zk_toolbar_add(ed->toolbar, HSpace(12));
    zk_toolbar_add(ed->toolbar, ed->size);
    zk_toolbar_add(ed->toolbar, HSpace(12));
    zk_toolbar_add(ed->toolbar, ed->zoom);

    ed->setwin = WindowObject,
        MUIA_Window_Title, (IPTR)"AROS Capture Settings",
        MUIA_Window_ID, MAKE_ID('A','C','S','T'),
        WindowContents, VGroup,
            Child, ColGroup(2),
                Child, Label2((IPTR)"Save folder"),
                Child, PopaslObject,
                    MUIA_Popstring_String, (IPTR)(ed->folder = StringObject,
                        StringFrame, MUIA_String_MaxLen, 255, MUIA_CycleChain, 1,
                        /* Room for a usual path; longer ones scroll in the field. */
                        MUIA_FixWidthTxt, (IPTR)"SYS:Pictures/Captures/Screenshots",
                        End),
                    MUIA_Popstring_Button, (IPTR)PopButton(MUII_PopDrawer),
                    ASLFR_DrawersOnly, TRUE,
                    ASLFR_TitleText, (IPTR)"Folder for saved captures",
                End,
            End,
            Child, HGroup,
                Child, ok = SimpleButton((IPTR)"_Save"),
                Child, HSpace(0),
                Child, cancel = SimpleButton((IPTR)"_Cancel"),
            End,
        End,
    End;

    ed->win = WindowObject,
        MUIA_Window_Title, (IPTR)"AROS Capture",
        MUIA_Window_ID, MAKE_ID('A','C','E','D'),
        MUIA_Window_Width, MUIV_Window_Width_Screen(75),
        MUIA_Window_Height, MUIV_Window_Height_Screen(75),
        WindowContents, VGroup,
            MUIA_Group_Spacing, 0,
            Child, ed->toolbar,
            Child, ed->view,
        End,
    End;

    ed->app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"AROS Capture",
        MUIA_Application_Base, (IPTR)"AROSCAPTURE",
        MUIA_Application_Description, (IPTR)"Screenshots with annotations",
        MUIA_Application_Version, (IPTR)"$VER: AROSCapture 0.1 (06.11.2026)",
        SubWindow, ed->win,
        SubWindow, ed->setwin,
    End;
    if (!ed->app)
        return FALSE;

    DoMethod(ed->win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
        ed->app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(ed->win, MUIM_Notify, MUIA_Window_InputEvent, (IPTR)"tab",
        ed->app, 2, MUIM_Application_ReturnID, ID_TAB);
    DoMethod(ed->win, MUIM_Notify, MUIA_Window_InputEvent, (IPTR)"ramiga z",
        ed->app, 2, MUIM_Application_ReturnID, ID_UNDO_KEY);
    DoMethod(ed->toolbar, MUIM_Notify, MUIA_ZkToolbar_Tool, MUIV_EveryTime,
        ed->app, 2, MUIM_Application_ReturnID, ID_TOOL);
    DoMethod(ed->toolbar, MUIM_Notify, MUIA_ZkToolbar_Pressed, MUIV_EveryTime,
        ed->app, 2, MUIM_Application_ReturnID, ID_PRESSED);
    DoMethod(ed->view, MUIM_Notify, MUIA_ZkImageView_PointerX, MUIV_EveryTime,
        ed->app, 2, MUIM_Application_ReturnID, ID_POINTER);
    DoMethod(ed->view, MUIM_Notify, MUIA_ZkImageView_PointerY, MUIV_EveryTime,
        ed->app, 2, MUIM_Application_ReturnID, ID_POINTER);
    DoMethod(ed->view, MUIM_Notify, MUIA_ZkImageView_Shown, MUIV_EveryTime,
        ed->app, 2, MUIM_Application_ReturnID, ID_ZOOM);
    DoMethod(ok, MUIM_Notify, MUIA_Pressed, FALSE,
        ed->app, 2, MUIM_Application_ReturnID, ID_SETTINGS_SAVE);
    DoMethod(cancel, MUIM_Notify, MUIA_Pressed, FALSE,
        ed->app, 2, MUIM_Application_ReturnID, ID_SETTINGS_CANCEL);
    DoMethod(ed->setwin, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
        ed->app, 2, MUIM_Application_ReturnID, ID_SETTINGS_CANCEL);
    return TRUE;
}

static void pressed(struct editor *ed)
{
    ULONG id = 0;

    get(ed->toolbar, MUIA_ZkToolbar_Pressed, &id);
    switch (id)
    {
    case ACT_CAPTURE: do_capture(ed); break;
    case ACT_COPY: do_copy(ed); break;
    case ACT_SAVE: do_save(ed); break;
    case ACT_UNDO:
        do_undo(ed);
        break;
    case ACT_SETTINGS:
        set(ed->folder, MUIA_String_Contents, (IPTR)ed->folder_path);
        set(ed->setwin, MUIA_Window_Open, TRUE);
        break;
    }
}

int editor_run(struct cap_image *img, LONG missing)
{
    struct editor *ed;
    ULONG sigs = 0;
    BOOL running = TRUE;
    int rc = RETURN_OK;

    if (!zk_toolbar_init() || !zk_imageview_init())
    {
        PutStr((STRPTR)"AROSCapture: cannot create the editor classes\n");
        zk_imageview_exit();
        zk_toolbar_exit();
        cap_free(img);
        return RETURN_FAIL;
    }
    ed = AllocVec(sizeof *ed, MEMF_ANY | MEMF_CLEAR);
    if (!ed)
    {
        cap_free(img);
        zk_imageview_exit();
        zk_toolbar_exit();
        return RETURN_FAIL;
    }
    ed->tool = TOOL_SELECT;
    ed->colour = 0xE53935;
    ed->width = 3;
    load_settings(ed);

    if (!build(ed) || !set_picture(ed, img, missing))
    {
        PutStr((STRPTR)"AROSCapture: cannot open the editor window\n");
        cap_free(img);
        rc = RETURN_FAIL;
        running = FALSE;
    }
    else
    {
        set(ed->toolbar, MUIA_ZkToolbar_Tool, TOOL_SELECT);
        set(ed->win, MUIA_Window_Open, TRUE);
        show_zoom(ed);
    }

    while (running)
    {
        ULONG id = DoMethod(ed->app, MUIM_Application_NewInput, (IPTR)&sigs);

        switch (id)
        {
        case MUIV_Application_ReturnID_Quit:
            running = FALSE;
            break;
        case ID_TOOL:
            get(ed->toolbar, MUIA_ZkToolbar_Tool, &ed->tool);
            break;
        case ID_PRESSED:
            pressed(ed);
            break;
        case ID_UNDO_KEY:
            do_undo(ed);
            break;
        case ID_POINTER:
            show_pointer(ed);
            break;
        case ID_ZOOM:
            show_zoom(ed);
            break;
        case ID_TAB:
            if (ed->hexcode[0])
            {
                char text[12];

                snprintf(text, sizeof text, "#%s", ed->hexcode);
                set_title(ed, clip_text(text) ? "colour copied" : "copying the colour failed");
            }
            break;
        case ID_DRAGOUT:
            do_dragout(ed);
            break;
        case ID_SETTINGS_SAVE:
        {
            STRPTR s = NULL;

            get(ed->folder, MUIA_String_Contents, &s);
            {
                char msg[400];

                if (!save_settings(ed, s ? (const char *)s : ""))
                    snprintf(msg, sizeof msg, "the settings could not be saved; captures still go to %s",
                        ed->folder_path);
                else
                    snprintf(msg, sizeof msg, "settings saved; captures go to %s", ed->folder_path);
                set_title(ed, msg);
            }
            set(ed->setwin, MUIA_Window_Open, FALSE);
            break;
        }
        case ID_SETTINGS_CANCEL:
            set(ed->setwin, MUIA_Window_Open, FALSE);
            break;
        }
        if (running && sigs)
        {
            sigs = Wait(sigs | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C)
                running = FALSE;
        }
    }

    if (ed->app)
    {
        set(ed->win, MUIA_Window_Open, FALSE);
        zk_imageview_set_image(ed->view, NULL, 0, 0);
        MUI_DisposeObject(ed->app);
    }
    if (ed->shown)
        FreeVec(ed->shown);
    cap_free(&ed->base);
    annot_clear(&ed->annots);
    FreeVec(ed);
    zk_imageview_exit();
    zk_toolbar_exit();
    return rc;
}
