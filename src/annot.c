#include "annot.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SS 4   /* supersampling per axis for coverage */

struct annot *annot_begin(struct annot_list *list, int kind, uint32_t rgb, int width, int x, int y)
{
    struct annot *a;

    if (list->count == list->cap)
    {
        int cap = list->cap ? list->cap * 2 : 16;
        struct annot *n = realloc(list->items, (size_t)cap * sizeof *n);

        if (!n)
            return NULL;
        list->items = n;
        list->cap = cap;
    }
    a = &list->items[list->count];
    memset(a, 0, sizeof *a);
    a->cap = kind == ANNOT_PENCIL ? 64 : 2;
    a->xy = malloc((size_t)a->cap * 2 * sizeof(int));
    if (!a->xy)
        return NULL;
    a->kind = kind;
    a->rgb = rgb;
    a->width = width < 1 ? 1 : width;
    a->xy[0] = a->xy[2] = x;
    a->xy[1] = a->xy[3] = y;
    a->npoints = kind == ANNOT_PENCIL ? 1 : 2;
    list->count++;
    return a;
}

int annot_extend(struct annot *a, int x, int y)
{
    if (a->kind != ANNOT_PENCIL)
    {
        a->xy[2] = x;
        a->xy[3] = y;
        return 1;
    }
    if (a->xy[(a->npoints - 1) * 2] == x && a->xy[(a->npoints - 1) * 2 + 1] == y)
        return 1;
    if (a->npoints == a->cap)
    {
        int cap = a->cap * 2;
        int *n = realloc(a->xy, (size_t)cap * 2 * sizeof(int));

        if (!n)
            return 0;
        a->xy = n;
        a->cap = cap;
    }
    a->xy[a->npoints * 2] = x;
    a->xy[a->npoints * 2 + 1] = y;
    a->npoints++;
    return 1;
}

int annot_pop(struct annot_list *list)
{
    if (list->count == 0)
        return 0;
    list->count--;
    free(list->items[list->count].xy);
    return 1;
}

int annot_undo(struct annot_list *list, struct annot **active, struct annot_box *gone)
{
    gone->x = gone->y = gone->w = gone->h = 0;
    if (list->count == 0)
    {
        *active = NULL;
        return 0;
    }
    /* The active annotation is the last one: annot_begin appends, and
       nothing else is added while a gesture runs. */
    *gone = annot_bounds(&list->items[list->count - 1]);
    *active = NULL;
    return annot_pop(list);
}

void annot_clear(struct annot_list *list)
{
    while (annot_pop(list))
        ;
    free(list->items);
    list->items = NULL;
    list->cap = 0;
}

int annot_is_empty(const struct annot *a)
{
    if (a->kind == ANNOT_PENCIL)
        return a->npoints < 2;
    return abs(a->xy[2] - a->xy[0]) < 2 && abs(a->xy[3] - a->xy[1]) < 2;
}

static int head_len(const struct annot *a)
{
    return a->width * 4 + 6;
}

struct annot_box annot_bounds(const struct annot *a)
{
    int minx = a->xy[0], maxx = a->xy[0], miny = a->xy[1], maxy = a->xy[1], i, pad;
    struct annot_box b;

    for (i = 1; i < a->npoints; i++)
    {
        int x = a->xy[i * 2], y = a->xy[i * 2 + 1];

        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
    }
    pad = a->width / 2 + 2;
    if (a->kind == ANNOT_ARROW)
        pad += head_len(a);
    b.x = minx - pad;
    b.y = miny - pad;
    b.w = maxx - minx + 2 * pad + 1;
    b.h = maxy - miny + 2 * pad + 1;
    return b;
}

struct annot_box annot_tail_bounds(const struct annot *a)
{
    struct annot_box b;
    int n = a->npoints, x0, y0, x1, y1, pad;

    if (a->kind != ANNOT_PENCIL || n < 2)
        return annot_bounds(a);
    x0 = a->xy[(n - 2) * 2];
    y0 = a->xy[(n - 2) * 2 + 1];
    x1 = a->xy[(n - 1) * 2];
    y1 = a->xy[(n - 1) * 2 + 1];
    pad = a->width / 2 + 2;
    b.x = (x0 < x1 ? x0 : x1) - pad;
    b.y = (y0 < y1 ? y0 : y1) - pad;
    b.w = (x0 < x1 ? x1 - x0 : x0 - x1) + 2 * pad + 1;
    b.h = (y0 < y1 ? y1 - y0 : y0 - y1) + 2 * pad + 1;
    return b;
}

struct annot_box annot_union(struct annot_box a, struct annot_box b)
{
    struct annot_box r;
    int x1, y1;

    if (a.w <= 0 || a.h <= 0)
        return b;
    if (b.w <= 0 || b.h <= 0)
        return a;
    r.x = a.x < b.x ? a.x : b.x;
    r.y = a.y < b.y ? a.y : b.y;
    x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w;
    y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
    r.w = x1 - r.x;
    r.h = y1 - r.y;
    return r;
}

struct annot_box annot_clip(struct annot_box b, int w, int h)
{
    int x1 = b.x + b.w, y1 = b.y + b.h;

    if (b.x < 0) b.x = 0;
    if (b.y < 0) b.y = 0;
    if (x1 > w) x1 = w;
    if (y1 > h) y1 = h;
    b.w = x1 > b.x ? x1 - b.x : 0;
    b.h = y1 > b.y ? y1 - b.y : 0;
    return b;
}

/* ---- coverage drawing ---- */

struct seg
{
    float ax, ay, bx, by;
    float minx, miny, maxx, maxy;   /* bounds including the stroke */
};

struct shape
{
    struct seg *segs;
    int nsegs, cap;
    float half;              /* half stroke width */
    float tri[6];            /* filled arrow head, when has_tri */
    int has_tri;
};

static int add_seg(struct shape *s, float ax, float ay, float bx, float by)
{
    if (s->nsegs == s->cap)
    {
        int cap = s->cap ? s->cap * 2 : 32;
        struct seg *n = realloc(s->segs, (size_t)cap * sizeof *n);

        if (!n)
            return 0;
        s->segs = n;
        s->cap = cap;
    }
    s->segs[s->nsegs].ax = ax;
    s->segs[s->nsegs].ay = ay;
    s->segs[s->nsegs].bx = bx;
    s->segs[s->nsegs].by = by;
    s->segs[s->nsegs].minx = (ax < bx ? ax : bx) - s->half;
    s->segs[s->nsegs].maxx = (ax < bx ? bx : ax) + s->half;
    s->segs[s->nsegs].miny = (ay < by ? ay : by) - s->half;
    s->segs[s->nsegs].maxy = (ay < by ? by : ay) + s->half;
    s->nsegs++;
    return 1;
}

/* Picture pixel (x, y) covers x..x+1; its centre is x+0.5. Shapes use
   pixel centres for their points. */
static void build(const struct annot *a, struct shape *s)
{
    float x0 = a->xy[0] + 0.5f, y0 = a->xy[1] + 0.5f;
    float x1 = a->xy[2] + 0.5f, y1 = a->xy[3] + 0.5f;
    int i;

    memset(s, 0, sizeof *s);
    s->half = a->width / 2.0f;
    switch (a->kind)
    {
    case ANNOT_RECT:
        add_seg(s, x0, y0, x1, y0);
        add_seg(s, x1, y0, x1, y1);
        add_seg(s, x1, y1, x0, y1);
        add_seg(s, x0, y1, x0, y0);
        break;
    case ANNOT_ELLIPSE:
    {
        float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        float rx = fabsf(x1 - x0) / 2, ry = fabsf(y1 - y0) / 2;
        int n = 64;
        float px = cx + rx, py = cy;

        for (i = 1; i <= n; i++)
        {
            float t = 6.2831853f * i / n;
            float qx = cx + rx * cosf(t), qy = cy + ry * sinf(t);

            add_seg(s, px, py, qx, qy);
            px = qx;
            py = qy;
        }
        break;
    }
    case ANNOT_LINE:
        add_seg(s, x0, y0, x1, y1);
        break;
    case ANNOT_ARROW:
    {
        float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
        float hl = (float)head_len(a), hw = hl * 0.6f;

        if (len < 1.0f)
        {
            add_seg(s, x0, y0, x1, y1);
            break;
        }
        dx /= len;
        dy /= len;
        if (hl > len)
            hl = len;
        /* The shaft stops inside the head, so its round cap does not poke
           out of the tip. */
        add_seg(s, x0, y0, x1 - dx * hl * 0.8f, y1 - dy * hl * 0.8f);
        s->tri[0] = x1;
        s->tri[1] = y1;
        s->tri[2] = x1 - dx * hl - dy * hw;
        s->tri[3] = y1 - dy * hl + dx * hw;
        s->tri[4] = x1 - dx * hl + dy * hw;
        s->tri[5] = y1 - dy * hl - dx * hw;
        s->has_tri = 1;
        break;
    }
    case ANNOT_PENCIL:
        if (a->npoints == 1)
            add_seg(s, x0, y0, x0, y0);
        for (i = 1; i < a->npoints; i++)
            add_seg(s, a->xy[(i - 1) * 2] + 0.5f, a->xy[(i - 1) * 2 + 1] + 0.5f,
                a->xy[i * 2] + 0.5f, a->xy[i * 2 + 1] + 0.5f);
        break;
    }
}

static int in_tri(const float *t, float x, float y)
{
    float d1 = (x - t[2]) * (t[1] - t[3]) - (t[0] - t[2]) * (y - t[3]);
    float d2 = (x - t[4]) * (t[3] - t[5]) - (t[2] - t[4]) * (y - t[5]);
    float d3 = (x - t[0]) * (t[5] - t[1]) - (t[4] - t[0]) * (y - t[1]);
    int neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;

    return !(neg && pos);
}

static int covered(const struct shape *s, float x, float y)
{
    int i;
    float h2 = s->half * s->half;

    if (s->has_tri && in_tri(s->tri, x, y))
        return 1;
    for (i = 0; i < s->nsegs; i++)
    {
        const struct seg *g = &s->segs[i];
        float dx, dy, ll, t, ex, ey;

        if (x < g->minx || x > g->maxx || y < g->miny || y > g->maxy)
            continue;
        dx = g->bx - g->ax;
        dy = g->by - g->ay;
        ll = dx * dx + dy * dy;
        t = ll == 0 ? 0 : ((x - g->ax) * dx + (y - g->ay) * dy) / ll;

        if (t < 0) t = 0;
        if (t > 1) t = 1;
        ex = g->ax + t * dx - x;
        ey = g->ay + t * dy - y;
        if (ex * ex + ey * ey <= h2)
            return 1;
    }
    return 0;
}

static void draw(uint8_t *out, int w, const struct annot *a, struct annot_box clip)
{
    struct shape s;
    struct annot_box b = annot_bounds(a);
    int x, y;
    uint8_t r = (uint8_t)(a->rgb >> 16), g = (uint8_t)(a->rgb >> 8), bl = (uint8_t)a->rgb;

    b.x = b.x > clip.x ? b.x : clip.x;
    b.y = b.y > clip.y ? b.y : clip.y;
    {
        int x1 = annot_bounds(a).x + annot_bounds(a).w, y1 = annot_bounds(a).y + annot_bounds(a).h;

        if (x1 > clip.x + clip.w) x1 = clip.x + clip.w;
        if (y1 > clip.y + clip.h) y1 = clip.y + clip.h;
        b.w = x1 - b.x;
        b.h = y1 - b.y;
    }
    if (b.w <= 0 || b.h <= 0)
        return;
    build(a, &s);
    /* Only the segments that reach the box matter: a long pencil stroke
       redrawn for its newest segment keeps a handful of them. */
    {
        int i, k = 0;

        for (i = 0; i < s.nsegs; i++)
            if (s.segs[i].maxx >= b.x && s.segs[i].minx <= b.x + b.w &&
                s.segs[i].maxy >= b.y && s.segs[i].miny <= b.y + b.h)
                s.segs[k++] = s.segs[i];
        s.nsegs = k;
    }
    for (y = b.y; y < b.y + b.h; y++)
        for (x = b.x; x < b.x + b.w; x++)
        {
            int n = 0, sx, sy;
            uint8_t *p;

            for (sy = 0; sy < SS; sy++)
                for (sx = 0; sx < SS; sx++)
                    n += covered(&s, x + (sx + 0.5f) / SS, y + (sy + 0.5f) / SS);
            if (!n)
                continue;
            p = out + ((size_t)y * (size_t)w + (size_t)x) * 4;
            p[0] = 255;
            p[1] = (uint8_t)((r * n + p[1] * (SS * SS - n) + SS * SS / 2) / (SS * SS));
            p[2] = (uint8_t)((g * n + p[2] * (SS * SS - n) + SS * SS / 2) / (SS * SS));
            p[3] = (uint8_t)((bl * n + p[3] * (SS * SS - n) + SS * SS / 2) / (SS * SS));
        }
    free(s.segs);
}

void annot_compose(uint8_t *out, const uint8_t *base, int w, int h,
                   const struct annot_list *list, struct annot_box box)
{
    int y, i;

    box = annot_clip(box, w, h);
    if (box.w <= 0 || box.h <= 0)
        return;
    for (y = box.y; y < box.y + box.h; y++)
        memcpy(out + ((size_t)y * (size_t)w + (size_t)box.x) * 4,
               base + ((size_t)y * (size_t)w + (size_t)box.x) * 4, (size_t)box.w * 4);
    for (i = 0; i < list->count; i++)
        draw(out, w, &list->items[i], box);
}
