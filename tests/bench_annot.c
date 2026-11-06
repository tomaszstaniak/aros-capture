/* Host measurement for the pencil path: cc -O2 -I src tests/bench_annot.c src/annot.c -lm
   Simulates a 400-point stroke on a 1024x768 picture with the editor's
   redraw pattern: one compose per pointer move. */
#include "annot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

int main(void)
{
    enum { W = 1024, H = 768, N = 400 };
    uint8_t *base = calloc(W * H, 4), *out = calloc(W * H, 4);
    struct annot_list list = { NULL, 0, 0 };
    struct annot *a;
    clock_t t0;
    int i;

    a = annot_begin(&list, ANNOT_PENCIL, 0xFF0000, 3, 512, 384);
    t0 = clock();
    for (i = 1; i < N; i++)
    {
        double ang = i * 0.05, r = 300 - i * 0.6;
        annot_extend(a, 512 + (int)(r * cos(ang)), 384 + (int)(r * 0.7 * sin(ang)));
        annot_compose(out, base, W, H, &list, annot_tail_bounds(a));
    }
    printf("bench_annot: %d moves, %.3f s CPU (%.2f ms per move)\n", N - 1,
        (double)(clock() - t0) / CLOCKS_PER_SEC,
        1000.0 * (clock() - t0) / CLOCKS_PER_SEC / (N - 1));
    annot_clear(&list);
    free(base);
    free(out);
    return 0;
}
