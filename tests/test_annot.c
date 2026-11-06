/* Host test for src/annot.c: cc -I src tests/test_annot.c src/annot.c -lm */
#include "annot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(c, what) do { if (!(c)) { printf("FAIL %s\n", what); failures++; } } while (0)

static const uint8_t *px(const uint8_t *b, int w, int x, int y)
{
    return b + ((size_t)y * w + x) * 4;
}

int main(void)
{
    enum { W = 64, H = 48 };
    uint8_t *base = malloc(W * H * 4), *out = malloc(W * H * 4);
    struct annot_list list = { NULL, 0, 0 };
    struct annot *a;
    struct annot_box all = { 0, 0, W, H }, b;
    int i;

    for (i = 0; i < W * H; i++)
    {
        base[i * 4] = 255; base[i * 4 + 1] = 10; base[i * 4 + 2] = 20; base[i * 4 + 3] = 30;
    }

    /* Rectangle outline, width 3, red: edges red, centre and outside untouched. */
    a = annot_begin(&list, ANNOT_RECT, 0xFF0000, 3, 10, 10);
    annot_extend(a, 40, 30);
    annot_compose(out, base, W, H, &list, all);
    CHECK(px(out, W, 25, 10)[1] == 255 && px(out, W, 25, 10)[2] == 0, "rect top edge red");
    CHECK(px(out, W, 40, 20)[1] == 255, "rect right edge red");
    CHECK(memcmp(px(out, W, 25, 20), px(base, W, 25, 20), 4) == 0, "rect inside untouched");
    CHECK(memcmp(px(out, W, 2, 2), px(base, W, 2, 2), 4) == 0, "outside untouched");
    CHECK(px(out, W, 25, 12)[1] < 255, "rect edge is 3 px, not more");

    /* Bounds contain every changed pixel. */
    b = annot_bounds(a);
    {
        int x, y, outside = 0;
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++)
                if (memcmp(px(out, W, x, y), px(base, W, x, y), 4) != 0 &&
                    (x < b.x || x >= b.x + b.w || y < b.y || y >= b.y + b.h))
                    outside++;
        CHECK(outside == 0, "changes inside bounds");
    }

    /* Undo restores the base exactly. */
    annot_pop(&list);
    annot_compose(out, base, W, H, &list, all);
    CHECK(memcmp(out, base, W * H * 4) == 0, "undo restores base");

    /* Arrow: tip pixel covered, tail covered, a far pixel not. */
    a = annot_begin(&list, ANNOT_ARROW, 0x00FF00, 2, 5, 40);
    annot_extend(a, 50, 5);
    annot_compose(out, base, W, H, &list, all);
    CHECK(px(out, W, 49, 6)[2] > 128, "arrow near tip green");
    CHECK(px(out, W, 6, 39)[2] > 100, "arrow tail green");
    CHECK(memcmp(px(out, W, 60, 45), px(base, W, 60, 45), 4) == 0, "arrow far untouched");

    /* Compose limited to a box leaves the rest of out alone. */
    memset(out, 0, W * H * 4);
    b.x = 0; b.y = 0; b.w = 8; b.h = 8;
    annot_compose(out, base, W, H, &list, b);
    CHECK(px(out, W, 20, 20)[0] == 0 && px(out, W, 3, 3)[0] == 255, "box-limited compose");

    /* Pencil grows and a click is empty. */
    annot_clear(&list);
    a = annot_begin(&list, ANNOT_PENCIL, 0x0000FF, 2, 1, 1);
    CHECK(annot_is_empty(a), "pencil click empty");
    for (i = 2; i < 200; i++)
        annot_extend(a, i % W, (i / W) + 1);
    CHECK(a->npoints == 199 && !annot_is_empty(a), "pencil points");
    a = annot_begin(&list, ANNOT_ELLIPSE, 0xFFFFFF, 2, 30, 30);
    annot_extend(a, 31, 30);
    CHECK(annot_is_empty(a), "tiny ellipse empty");

    /* Clip and union. */
    b.x = -5; b.y = -5; b.w = 20; b.h = 100;
    b = annot_clip(b, W, H);
    CHECK(b.x == 0 && b.y == 0 && b.w == 15 && b.h == H, "clip");
    {
        struct annot_box e = { 0, 0, 0, 0 }, c = { 3, 4, 5, 6 };
        struct annot_box u = annot_union(e, c);
        CHECK(u.x == 3 && u.w == 5, "union with empty");
    }

    /* Undo during a gesture (review 2026-11-06): the active shape is
       removed and the caller's pointer cleared, so the moves and the release
       that follow cannot touch freed points. */
    annot_clear(&list);
    {
        struct annot *active, *done;
        struct annot_box gone;

        done = annot_begin(&list, ANNOT_LINE, 0xFF0000, 2, 1, 1);
        annot_extend(done, 30, 30);
        active = annot_begin(&list, ANNOT_PENCIL, 0x00FF00, 2, 5, 5);
        for (i = 0; i < 100; i++)      /* grows past the first allocation */
            annot_extend(active, 5 + i % 40, 5 + i / 40);
        CHECK(annot_undo(&list, &active, &gone) == 1, "undo during gesture");
        CHECK(active == NULL, "undo clears the active pointer");
        CHECK(list.count == 1 && list.items[0].kind == ANNOT_LINE, "undo removed only the active shape");
        CHECK(gone.w > 0 && gone.h > 0, "undo reports the area to redraw");
        if (active)
            annot_extend(active, 9, 9); /* would be the use after free */
        CHECK(annot_undo(&list, &active, &gone) == 1 && list.count == 0, "undo of a finished shape");
        CHECK(annot_undo(&list, &active, &gone) == 0, "undo with nothing left");
    }

    /* Redrawing only the newest pencil segment per move gives the same
       pixels as one full redraw at the end (incremental path of the
       editor, 2026-11-06). */
    annot_clear(&list);
    {
        uint8_t *full = malloc(W * H * 4);
        struct annot_box prev;

        memcpy(out, base, W * H * 4);
        a = annot_begin(&list, ANNOT_PENCIL, 0x3366FF, 3, 3, 40);
        annot_compose(out, base, W, H, &list, annot_tail_bounds(a));
        for (i = 1; i < 120; i++)
        {
            prev = annot_tail_bounds(a);
            annot_extend(a, 3 + (i * 7) % 58, 40 - (i * 13) % 37);
            annot_compose(out, base, W, H, &list, annot_union(prev, annot_tail_bounds(a)));
        }
        annot_compose(full, base, W, H, &list, all);
        CHECK(memcmp(out, full, W * H * 4) == 0, "incremental pencil equals full redraw");
        free(full);
    }

    annot_clear(&list);
    free(base);
    free(out);
    if (!failures)
        puts("test_annot: all passed");
    return failures != 0;
}
