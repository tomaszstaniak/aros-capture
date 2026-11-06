#include "capture.h"
#include "replace.h"
#include "names.h"

#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <utility/date.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <proto/datatypes.h>
#include <proto/utility.h>

#include <stdio.h>
#include <string.h>

BOOL cap_grab(struct Screen *scr, struct cap_image *img)
{
    ULONG bytes = (ULONG)scr->Width * (ULONG)scr->Height * 4;

    img->width = scr->Width;
    img->height = scr->Height;
    img->pixels = AllocVec(bytes, MEMF_ANY);
    if (!img->pixels)
        return FALSE;
    /* The screen's own RastPort has no layer: this reads the screen bitmap,
       including the windows drawn into it. */
    ReadPixelArray(img->pixels, 0, 0, img->width * 4, &scr->RastPort,
        0, 0, img->width, img->height, RECTFMT_ARGB);
    /* The first byte is the bitmap's pad byte, not transparency (0 on the
       tested v1 system): a screenshot is opaque. */
    {
        ULONG i, n = (ULONG)img->width * (ULONG)img->height;

        for (i = 0; i < n; i++)
            img->pixels[i * 4] = 255;
    }
    return TRUE;
}

void cap_free(struct cap_image *img)
{
    if (img->pixels)
        FreeVec(img->pixels);
    img->pixels = NULL;
}

LONG cap_count_composited(struct Screen *scr, LONG x, LONG y, LONG w, LONG h)
{
    LONG n = 0;
#ifdef LAYERCOMPOSITED
    struct Layer *l;

    LockLayerInfo(&scr->LayerInfo);
    for (l = scr->LayerInfo.top_layer; l; l = l->back)
    {
        if (!(l->Flags & LAYERCOMPOSITED) || !IS_VISIBLE(l))
            continue;
        if (l->bounds.MaxX < x || l->bounds.MinX >= x + w ||
            l->bounds.MaxY < y || l->bounds.MinY >= y + h)
            continue;
        n++;
    }
    UnlockLayerInfo(&scr->LayerInfo);
#else
    (void)scr; (void)x; (void)y; (void)w; (void)h;
#endif
    return n;
}

static void fail_message(char *msg, ULONG size, CONST_STRPTR what, CONST_STRPTR path, LONG err)
{
    char reason[100];

    reason[0] = 0;
    if (err)
        Fault(err, NULL, (STRPTR)reason, sizeof reason);
    if (reason[0])
        snprintf(msg, size, "%s %s: %s", what, path, reason);
    else
        snprintf(msg, size, "%s %s", what, path);
}

static LONG file_size(CONST_STRPTR path)
{
    BPTR lock = Lock(path, SHARED_LOCK);
    struct FileInfoBlock *fib;
    LONG size = -1;

    if (!lock)
        return -1;
    fib = AllocDosObject(DOS_FIB, NULL);
    if (fib)
    {
        if (Examine(lock, fib))
            size = fib->fib_Size;
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return size;
}

/* The temporary name a capture is written to before it gets its real name:
   "<path>.<task>-<n>.tmp", in the same folder, so the final Rename never
   crosses volumes. */
static BOOL temp_name(CONST_STRPTR path, const char *suffix, char *out, ULONG size)
{
    static ULONG counter;
    ULONG i;

    for (i = 0; i < 100; i++)
    {
        BPTR lock;
        int n = snprintf(out, size, "%s.%lx-%lu.%s", (const char *)path,
            (unsigned long)((IPTR)FindTask(NULL) & 0xFFFFFFFF), (unsigned long)++counter, suffix);

        if (n <= 0 || (ULONG)n >= size)
            return FALSE;
        lock = Lock((CONST_STRPTR)out, SHARED_LOCK);
        if (!lock)
            return TRUE;
        UnLock(lock);
    }
    return FALSE;
}

/* Writes the picture into the new file fh. Returns the DOS error, 0 on
   success; *what names the step that failed. */
static LONG write_png_to(Object *dto, BPTR fh, CONST_STRPTR tmp, const char **what)
{
    struct dtWrite dtw;
    IPTR written;
    BOOL closed;
    LONG err;

    dtw.MethodID = DTM_WRITE;
    dtw.dtw_GInfo = NULL;
    dtw.dtw_FileHandle = fh;
    dtw.dtw_Mode = DTWM_RAW;
    dtw.dtw_AttrList = NULL;
    SetIoErr(0);
    written = DoMethodA(dto, (Msg)&dtw);
#ifdef CAPTURE_TEST_FAIL_WRITE
    /* Test build: every write fails after the file was created. */
    written = 0;
    SetIoErr(ERROR_DISK_FULL);
#endif
    err = IoErr();
    closed = Close(fh);
    if (!closed && !err)
        err = IoErr();
    if (!written)
    {
        *what = "Writing failed for";
        return err ? err : ERROR_WRITE_PROTECTED;
    }
    if (!closed)
    {
        *what = "Closing failed for";
        return err ? err : ERROR_WRITE_PROTECTED;
    }
    if (file_size(tmp) <= 0)
    {
        *what = "Nothing was written to";
        return err ? err : ERROR_DISK_FULL;
    }
    return 0;
}

static BOOL is_drawer(CONST_STRPTR path);

static long dos_rename(void *ctx, const char *from, const char *to)
{
    (void)ctx;
#ifdef CAPTURE_TEST_FAIL_PUBLISH
    /* Test build: the new file cannot take the name, and with
       CAPTURE_TEST_FAIL_RESTORE the old one cannot go back either. */
    if (strstr(from, ".tmp"))
        return ERROR_DISK_WRITE_PROTECTED;
#ifdef CAPTURE_TEST_FAIL_RESTORE
    if (strstr(from, ".old"))
        return ERROR_DISK_WRITE_PROTECTED;
#endif
#endif
    return Rename((CONST_STRPTR)from, (CONST_STRPTR)to) ? 0 : (IoErr() ? IoErr() : ERROR_OBJECT_IN_USE);
}

static long dos_remove(void *ctx, const char *path)
{
    (void)ctx;
    return DeleteFile((CONST_STRPTR)path) ? 0 : (IoErr() ? IoErr() : ERROR_OBJECT_IN_USE);
}

/* FORCE: replaces the existing path with the complete tmp. */
static BOOL replace_with(const char *tmp, CONST_STRPTR path, LONG *ioerr, char *msg, ULONG msgsize)
{
    struct rep_ops ops = { NULL, dos_rename, dos_remove };
    char aside[600];
    long err;

    if (!temp_name(path, "old", aside, sizeof aside))
    {
        *ioerr = ERROR_LINE_TOO_LONG;
        snprintf(msg, msgsize, "The file name is too long: %s", (const char *)path);
        DeleteFile((CONST_STRPTR)tmp);
        return FALSE;
    }
    /* A drawer that appeared under the name is not replaced by a file. */
    if (is_drawer(path))
    {
        *ioerr = ERROR_OBJECT_EXISTS;
        snprintf(msg, msgsize, "%s is a drawer; it was kept", (const char *)path);
        DeleteFile((CONST_STRPTR)tmp);
        return FALSE;
    }
    switch (rep_publish(&ops, tmp, (const char *)path, aside, &err))
    {
    case REP_DONE:
        return TRUE;
    case REP_DONE_OLD_LEFT:
        /* Saved; only the old copy could not be deleted. Returns TRUE with
           *ioerr set and msg naming where the old copy is. */
        *ioerr = err;
        snprintf(msg, msgsize, "the replaced file could not be deleted and is kept as %s", aside);
        return TRUE;
    case REP_NOT_ASIDE:
        *ioerr = err;
        fail_message(msg, msgsize, (CONST_STRPTR)"Cannot replace (the old file stays)", path, err);
        DeleteFile((CONST_STRPTR)tmp);
        return FALSE;
    case REP_ROLLED_BACK:
        *ioerr = err;
        fail_message(msg, msgsize, (CONST_STRPTR)"Cannot replace (the old file was put back)", path, err);
        DeleteFile((CONST_STRPTR)tmp);
        return FALSE;
    default: /* REP_STRANDED: delete nothing, say where both are. */
        *ioerr = err;
        snprintf(msg, msgsize, "Cannot replace %s and cannot put the old file back: the old picture is %s,"
            " the new one is %s", (const char *)path, aside, tmp);
        return FALSE;
    }
}

BOOL cap_save_png(const struct cap_image *img, LONG x, LONG y, LONG w, LONG h,
    CONST_STRPTR path, BOOL replace, LONG *ioerr, char *msg, ULONG msgsize)
{
    Object *dto;
    struct BitMapHeader *bmhd = NULL;
    struct pdtBlitPixelArray blit;
    char tmp[600];
    const char *what = "";
    BPTR fh;
    LONG row, err;

    *ioerr = 0;
    if (msgsize)
        msg[0] = 0;
    if (!temp_name(path, "tmp", tmp, sizeof tmp))
    {
        *ioerr = ERROR_LINE_TOO_LONG;
        snprintf(msg, msgsize, "The file name is too long: %s", (const char *)path);
        return FALSE;
    }
    dto = NewDTObject((APTR)NULL,
        DTA_SourceType, DTST_RAM,
        DTA_BaseName, (IPTR)"png",
        PDTA_DestMode, PMODE_V43,
        TAG_DONE);
    if (!dto)
    {
        *ioerr = IoErr();
        fail_message(msg, msgsize, (CONST_STRPTR)"PNG support (png.datatype) is not available for", path, *ioerr);
        return FALSE;
    }
    GetDTAttrs(dto, PDTA_BitMapHeader, (IPTR)&bmhd, TAG_DONE);
    if (!bmhd)
    {
        DisposeDTObject(dto);
        snprintf(msg, msgsize, "png.datatype gave no picture header for %s", (const char *)path);
        return FALSE;
    }
    bmhd->bmh_Width = w;
    bmhd->bmh_Height = h;
    bmhd->bmh_Depth = 24;
    bmhd->bmh_PageWidth = w;
    bmhd->bmh_PageHeight = h;

    blit.MethodID = PDTM_WRITEPIXELARRAY;
    blit.pbpa_PixelFormat = PBPAFMT_ARGB;
    blit.pbpa_PixelArrayMod = w * 4;
    blit.pbpa_Left = 0;
    blit.pbpa_Width = w;
    blit.pbpa_Height = 1;
    for (row = 0; row < h; row++)
    {
        blit.pbpa_PixelData = img->pixels + (((ULONG)(y + row) * (ULONG)img->width) + (ULONG)x) * 4;
        blit.pbpa_Top = row;
        DoMethodA(dto, (Msg)&blit);
    }

    /* The picture goes to a temporary file first, and only a complete one
       gets the real name: a failed write never leaves a partial file under
       that name nor destroys a file that was there. */
    fh = Open((CONST_STRPTR)tmp, MODE_NEWFILE);
    if (!fh)
    {
        *ioerr = IoErr();
        fail_message(msg, msgsize, (CONST_STRPTR)"Cannot create", path, *ioerr);
        DisposeDTObject(dto);
        return FALSE;
    }
    err = write_png_to(dto, fh, (CONST_STRPTR)tmp, &what);
    DisposeDTObject(dto);
    if (err)
    {
        *ioerr = err;
        fail_message(msg, msgsize, (CONST_STRPTR)what, path, err);
        DeleteFile((CONST_STRPTR)tmp);
        return FALSE;
    }

    /* Rename refuses an existing name, so without replace a file that
       appeared after the check (during the delay or the area choice) is
       kept. With replace, see replace.h: the old file is kept until the new
       one has its name. */
    if (Rename((CONST_STRPTR)tmp, path))
        return TRUE;
    err = IoErr();
    if (!replace || err != ERROR_OBJECT_EXISTS)
    {
        *ioerr = err;
        if (err == ERROR_OBJECT_EXISTS)
            snprintf(msg, msgsize, "%s was created by something else while capturing; it was kept",
                (const char *)path);
        else
            fail_message(msg, msgsize, (CONST_STRPTR)"Cannot give the name", path, err);
        DeleteFile((CONST_STRPTR)tmp);
        return FALSE;
    }
    return replace_with(tmp, path, ioerr, msg, msgsize);
}

static void now(struct cap_time *t)
{
    struct DateStamp ds;
    struct ClockData cd;

    DateStamp(&ds);
    Amiga2Date((ULONG)ds.ds_Days * 86400 + (ULONG)ds.ds_Minute * 60 + (ULONG)ds.ds_Tick / TICKS_PER_SECOND, &cd);
    t->year = cd.year;
    t->month = cd.month;
    t->day = cd.mday;
    t->hour = cd.hour;
    t->min = cd.min;
    t->sec = cd.sec;
}

void cap_folder(char *buf, ULONG size)
{
    if (GetVar((CONST_STRPTR)CAP_ENV_FOLDER, (STRPTR)buf, size, GVF_GLOBAL_ONLY) <= 0 || !buf[0])
        snprintf(buf, size, "%s", CAP_DEFAULT_FOLDER);
}

void cap_new_name(char *buf, ULONG size)
{
    struct cap_time t;

    now(&t);
    if (!cap_name(buf, size, &t, 1))
        snprintf(buf, size, "Capture.png");
}

/* TRUE when path names an existing drawer (or volume). */
static BOOL is_drawer(CONST_STRPTR path)
{
    BPTR lock = Lock(path, SHARED_LOCK);
    struct FileInfoBlock *fib;
    BOOL dir = FALSE;

    if (!lock)
        return FALSE;
    fib = AllocDosObject(DOS_FIB, NULL);
    if (fib && Examine(lock, fib))
        dir = fib->fib_DirEntryType > 0;
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return dir;
}

static BOOL exists(CONST_STRPTR path)
{
    BPTR lock = Lock(path, SHARED_LOCK);

    if (lock)
    {
        UnLock(lock);
        return TRUE;
    }
    return FALSE;
}

/* Creates every missing directory of path (below its volume or assign). */
static BOOL make_dirs(CONST_STRPTR dir, struct cap_result *res)
{
    char part[512];
    const char *s = (const char *)dir;
    size_t len = strlen(s), i;
    const char *colon = strchr(s, ':');

    if (len >= sizeof part)
        return FALSE;
    for (i = colon ? (size_t)(colon - s) + 1 : 0; i <= len; i++)
    {
        BPTR lock;

        if (i < len && s[i] != '/')
            continue;
        if (i == 0 || s[i - 1] == ':' || s[i - 1] == '/')
            continue;
        memcpy(part, s, i);
        part[i] = 0;
        lock = Lock((CONST_STRPTR)part, SHARED_LOCK);
        if (!lock)
            lock = CreateDir((CONST_STRPTR)part);
        if (!lock)
        {
            res->ioerr = IoErr();
            fail_message(res->message, sizeof res->message, (CONST_STRPTR)"Cannot create the folder",
                (CONST_STRPTR)part, res->ioerr);
            return FALSE;
        }
        UnLock(lock);
    }
    return TRUE;
}

/* Checks that dir is an existing directory. */
static BOOL usable_dir(CONST_STRPTR dir, struct cap_result *res)
{
    BPTR lock = Lock(dir, SHARED_LOCK);
    struct FileInfoBlock *fib;
    BOOL ok = FALSE;

    if (!lock)
    {
        res->ioerr = IoErr();
        fail_message(res->message, sizeof res->message, (CONST_STRPTR)"Cannot use the folder", dir, res->ioerr);
        return FALSE;
    }
    fib = AllocDosObject(DOS_FIB, NULL);
    if (fib)
    {
        if (Examine(lock, fib))
            ok = fib->fib_DirEntryType > 0;
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    if (!ok)
        snprintf(res->message, sizeof res->message, "%s is not a folder", (const char *)dir);
    return ok;
}

static BOOL choose_path(const struct cap_request *req, struct cap_result *res)
{
    struct cap_time t;
    char file[64];
    unsigned seq;

    if (req->to)
    {
        if (strlen((const char *)req->to) >= sizeof res->path)
        {
            snprintf(res->message, sizeof res->message, "The file name is too long");
            return FALSE;
        }
        strcpy(res->path, (const char *)req->to);
        if (is_drawer(req->to))
        {
            snprintf(res->message, sizeof res->message,
                "%s is a drawer; TO needs a file name (DIR saves into a drawer)", res->path);
            return FALSE;
        }
        if (!req->force && exists(req->to))
        {
            snprintf(res->message, sizeof res->message,
                "%s already exists (use FORCE to replace it)", res->path);
            return FALSE;
        }
        return TRUE;
    }
    if (req->create_dir && !exists(req->dir) && !make_dirs(req->dir, res))
        return FALSE;
    if (!usable_dir(req->dir, res))
        return FALSE;
    now(&t);
    for (seq = 1; seq < 1000; seq++)
    {
        if (!cap_name(file, sizeof file, &t, seq) ||
            !cap_join(res->path, sizeof res->path, (const char *)req->dir, file))
        {
            snprintf(res->message, sizeof res->message, "The folder name is too long");
            return FALSE;
        }
        if (!exists((CONST_STRPTR)res->path))
            return TRUE;
    }
    snprintf(res->message, sizeof res->message, "No free file name in %s", (const char *)req->dir);
    return FALSE;
}

static BOOL wait_delay(LONG seconds)
{
    LONG i;

    for (i = 0; i < seconds * 10; i++)
    {
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
            return FALSE;
        Delay(TICKS_PER_SECOND / 10);
    }
    return !(SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C);
}

enum cap_status cap_take(const struct cap_request *req, struct cap_image *out,
    LONG *missing, char *msg, ULONG msgsize)
{
    struct Screen *scr;
    struct cap_image img = { NULL, 0, 0 };
    enum cap_status st;
    LONG x = 0, y = 0, w, h, row;

    out->pixels = NULL;
    out->width = out->height = 0;
    *missing = 0;
    if (req->delay > 0 && !wait_delay(req->delay))
    {
        snprintf(msg, msgsize, "Cancelled");
        return CAP_CANCELLED;
    }
    if (req->settle > 0)
        Delay(req->settle);

    scr = LockPubScreen(req->pubscreen);
    if (!scr)
    {
        snprintf(msg, msgsize, "Cannot find the public screen %s",
            req->pubscreen ? (const char *)req->pubscreen : "(default)");
        return CAP_FAILED;
    }
    if (!cap_grab(scr, &img))
    {
        snprintf(msg, msgsize, "Not enough memory for a %dx%d capture",
            (int)scr->Width, (int)scr->Height);
        UnlockPubScreen(NULL, scr);
        return CAP_FAILED;
    }
    w = img.width;
    h = img.height;
    if (req->region)
    {
        st = cap_select(scr, &img, &x, &y, &w, &h, msg, msgsize);
        if (st != CAP_OK)
        {
            cap_free(&img);
            UnlockPubScreen(NULL, scr);
            return st;
        }
    }
    *missing = cap_count_composited(scr, x, y, w, h);
    UnlockPubScreen(NULL, scr);

    if (x == 0 && y == 0 && w == img.width && h == img.height)
    {
        *out = img;
        return CAP_OK;
    }
    /* Keep only the chosen part: rows move towards the start in place. */
    for (row = 0; row < h; row++)
        memmove(img.pixels + (ULONG)row * (ULONG)w * 4,
            img.pixels + (((ULONG)(y + row) * (ULONG)img.width) + (ULONG)x) * 4, (ULONG)w * 4);
    img.width = w;
    img.height = h;
    *out = img;
    return CAP_OK;
}

void cap_run(const struct cap_request *req, struct cap_result *res)
{
    struct cap_image img;
    BOOL saved;
    int tries;

    memset(res, 0, sizeof *res);
    res->status = CAP_FAILED;

    /* Check the destination first: a capture that cannot be saved is
       pointless, and the user learns it before the delay. */
    if (!choose_path(req, res))
        return;
    res->status = cap_take(req, &img, &res->missing, res->message, sizeof res->message);
    if (res->status != CAP_OK)
        return;
    res->status = CAP_FAILED;

    /* A generated name was checked before the delay. If it is taken by the
       time the file gets it, the next free name is used; TO is never
       replaced without FORCE (cap_save_png refuses it). */
    for (tries = 0;; tries++)
    {
        saved = cap_save_png(&img, 0, 0, img.width, img.height, (CONST_STRPTR)res->path, req->to && req->force,
            &res->ioerr, res->message, sizeof res->message);
        if (saved || req->to || res->ioerr != ERROR_OBJECT_EXISTS || tries >= 5 || !choose_path(req, res))
            break;
    }
    /* FORCE is the way out only for a TO file. */
    if (!saved && req->to && !req->force && res->ioerr == ERROR_OBJECT_EXISTS && !is_drawer(req->to))
    {
        size_t n = strlen(res->message);

        if (n + 30 < sizeof res->message)
            strcpy(res->message + n, " (use FORCE to replace it)");
    }
    if (saved)
    {
        char note[sizeof res->message];
        int n;

        /* A note from a FORCE replace whose old copy stayed. */
        memcpy(note, res->ioerr ? res->message : "", res->ioerr ? sizeof note : 1);
        note[sizeof note - 1] = 0;
        res->status = CAP_OK;
        res->width = img.width;
        res->height = img.height;
        if (note[0])
        {
            n = snprintf(res->message, sizeof res->message, "Saved %dx%d to %s",
                (int)img.width, (int)img.height, res->path);
            if (n > 0 && (size_t)n + 2 < sizeof res->message)
            {
                /* Cut to fit; the start of the note names the kept file. */
                size_t room = sizeof res->message - n - 3, len = strlen(note);

                if (len > room)
                    len = room;
                memcpy(res->message + n, "; ", 2);
                memcpy(res->message + n + 2, note, len);
                res->message[n + 2 + len] = 0;
            }
        }
        else if (res->missing > 0)
            snprintf(res->message, sizeof res->message,
                "Saved %dx%d to %s; %d see-through window(s) are not included",
                (int)img.width, (int)img.height, res->path, (int)res->missing);
        else
            snprintf(res->message, sizeof res->message, "Saved %dx%d to %s",
                (int)img.width, (int)img.height, res->path);
    }
    cap_free(&img);
}

BOOL cap_unique_path(CONST_STRPTR dir, BOOL create, char *path, ULONG pathsize, char *msg, ULONG msgsize)
{
    struct cap_request req;
    struct cap_result *res = AllocVec(sizeof *res, MEMF_ANY | MEMF_CLEAR);
    BOOL ok;

    if (!res)
    {
        snprintf(msg, msgsize, "Out of memory");
        return FALSE;
    }
    memset(&req, 0, sizeof req);
    req.dir = dir;
    req.create_dir = create;
    ok = choose_path(&req, res);
    if (ok)
        snprintf(path, pathsize, "%s", res->path);
    else
        snprintf(msg, msgsize, "%s", res->message);
    FreeVec(res);
    return ok;
}
