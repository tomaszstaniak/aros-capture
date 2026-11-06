/* Annotations drawn over a capture. Pure C on ARGB buffers (four bytes A, R,
   G, B per pixel), so drawing is the same in the editor view, the saved file
   and the clipboard, and can be tested on the build host. */
#ifndef CAPTURE_ANNOT_H
#define CAPTURE_ANNOT_H

#include <stddef.h>
#include <stdint.h>

enum annot_kind
{
    ANNOT_RECT,
    ANNOT_ELLIPSE,
    ANNOT_LINE,
    ANNOT_ARROW,
    ANNOT_PENCIL
};

struct annot
{
    int kind;              /* enum annot_kind */
    uint32_t rgb;          /* 0x00RRGGBB */
    int width;             /* stroke width in picture pixels */
    int npoints;           /* 2 for shapes (start, end); more for pencil */
    int cap;               /* allocated points */
    int *xy;               /* x0, y0, x1, y1, ... */
};

struct annot_box
{
    int x, y, w, h;        /* w or h 0: empty */
};

struct annot_list
{
    struct annot *items;
    int count, cap;
};

/* A new annotation with its first point; NULL when out of memory. The list
   owns it. */
struct annot *annot_begin(struct annot_list *list, int kind, uint32_t rgb, int width, int x, int y);
/* Moves the end point (shapes) or appends a point (pencil). 0 when out of
   memory. */
int annot_extend(struct annot *a, int x, int y);
/* Removes the last annotation; 0 when there is none. Never use it on the
   annotation being drawn while a pointer to it is kept: use annot_undo. */
int annot_pop(struct annot_list *list);
/* Undo. When *active (the annotation being drawn, always the last one) is
   set, that gesture is cancelled: the annotation is removed and *active is
   cleared. Otherwise the last finished annotation is removed. Fills *gone
   with the pixels to draw again. 0 when there was nothing to undo. */
int annot_undo(struct annot_list *list, struct annot **active, struct annot_box *gone);
void annot_clear(struct annot_list *list);

/* The pixels an annotation can touch, including its stroke and arrow head. */
struct annot_box annot_bounds(const struct annot *a);
/* The pixels the latest change can have touched: for a pencil stroke the
   newest segment only, for shapes the whole shape. Redraw the union of the
   previous and the new tail bounds after annot_extend. */
struct annot_box annot_tail_bounds(const struct annot *a);
struct annot_box annot_union(struct annot_box a, struct annot_box b);
/* Limits a box to a w x h picture. */
struct annot_box annot_clip(struct annot_box b, int w, int h);

/* Copies base into out within box, then draws every annotation of the list
   over it, all limited to box. Both buffers are w x h ARGB. */
void annot_compose(uint8_t *out, const uint8_t *base, int w, int h,
                   const struct annot_list *list, struct annot_box box);

/* True when the annotation is too small to keep (a click without a drag). */
int annot_is_empty(const struct annot *a);

#endif
