/* AROS Capture core: one capture flow shared by the Shell command, the
   window and Dockbar entries. */
#ifndef CAPTURE_CAPTURE_H
#define CAPTURE_CAPTURE_H

#include <exec/types.h>
#include <intuition/screens.h>

/* Pixels as read with RECTFMT_ARGB: four bytes A, R, G, B per pixel. */
struct cap_image
{
    UBYTE *pixels;
    LONG width, height;
};

enum cap_status
{
    CAP_OK = 0,
    CAP_CANCELLED,
    CAP_FAILED
};

struct cap_request
{
    BOOL region;              /* choose a rectangle after capturing */
    LONG delay;               /* seconds before capturing */
    CONST_STRPTR to;          /* exact output file, or NULL */
    CONST_STRPTR dir;         /* output directory for generated names */
    CONST_STRPTR pubscreen;   /* NULL: the default public screen */
    BOOL force;               /* replace an existing TO file */
    BOOL create_dir;          /* create dir and its parents when missing */
    LONG settle;              /* ticks (1/50 s) to wait after the delay */
};

struct cap_result
{
    enum cap_status status;
    char path[512];
    LONG width, height;
    LONG missing;             /* see-through windows not in the image */
    LONG ioerr;
    char message[768];
};

/* Runs one capture. Ctrl-C cancels during the delay; Esc or the right
   mouse button cancel the region choice. */
void cap_run(const struct cap_request *req, struct cap_result *res);

/* Delay, screen read and (with region) the area choice; out gets the
   chosen part, to be freed with cap_free. */
enum cap_status cap_take(const struct cap_request *req, struct cap_image *out,
    LONG *missing, char *msg, ULONG msgsize);

/* A free generated file name in dir (created with its parents when create
   is set). */
BOOL cap_unique_path(CONST_STRPTR dir, BOOL create, char *path, ULONG pathsize,
    char *msg, ULONG msgsize);

/* "Capture-<date>_<time>.png" for now, without a folder. */
void cap_new_name(char *buf, ULONG size);

/* Reads the whole screen bitmap. Returns FALSE when out of memory. */
BOOL cap_grab(struct Screen *scr, struct cap_image *img);
void cap_free(struct cap_image *img);

/* Counts composited (see-through) windows whose surfaces overlap the box:
   the display composes them, they are not part of the screen bitmap. Always
   0 on systems without composited layers. */
LONG cap_count_composited(struct Screen *scr, LONG x, LONG y, LONG w, LONG h);

/* The capture folder: the one chosen in Settings (ENV:AROSCapture/Folder),
   otherwise CAP_DEFAULT_FOLDER. */
#define CAP_DEFAULT_FOLDER "SYS:Pictures/Captures"
#define CAP_ENV_FOLDER "AROSCapture/Folder"
void cap_folder(char *buf, ULONG size);

/* Writes the image, or the given part of it, as a 24-bit PNG through
   png.datatype. The file is written under a temporary name and renamed
   when complete: an existing file at path is replaced only with replace
   (otherwise *ioerr is ERROR_OBJECT_EXISTS), and a failed write leaves any
   existing file untouched and no partial file behind. A replace keeps the
   old file until the new one has its name (replace.h); TRUE with *ioerr set
   means saved, with msg saying where an undeletable old copy stayed. */
BOOL cap_save_png(const struct cap_image *img, LONG x, LONG y, LONG w, LONG h,
    CONST_STRPTR path, BOOL replace, LONG *ioerr, char *msg, ULONG msgsize);

/* Region choice over a frozen copy of the screen (select.c). Returns CAP_OK
   with the box filled in, or CAP_CANCELLED. */
enum cap_status cap_select(struct Screen *scr, const struct cap_image *img,
    LONG *x, LONG *y, LONG *w, LONG *h, char *msg, ULONG msgsize);

#endif
