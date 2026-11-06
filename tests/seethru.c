/* Test aid, not shipped: opens see-through windows on the default public
   screen for the composited read-back checks, then waits for Ctrl-C.
     seethru            three 200x150 windows: WA_Opaqueness 128 filled red
                        with FillPixelArray at 100,100; WA_Opaqueness 128
                        filled with pen 3 by RectFill at 100,320; and a
                        WA_AlphaSurface window (green, alpha ramp left to
                        right) at 400,100.
   On systems without composited layers the windows open as ordinary ones;
   it prints whether each layer got LAYERCOMPOSITED. */
#include <exec/types.h>
#include <dos/dos.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>

#include <stdio.h>

int main(void)
{
    struct Window *a, *b, *c;
    static UBYTE row[200 * 4];
    int x, y;

    a = OpenWindowTags(NULL, WA_Left, 100, WA_Top, 100, WA_InnerWidth, 200, WA_InnerHeight, 150,
        WA_Title, (IPTR)"seethru opacity", WA_DragBar, TRUE,
#ifdef WA_Opaqueness
        WA_Opaqueness, 128,
#endif
        TAG_DONE);
    b = OpenWindowTags(NULL, WA_Left, 400, WA_Top, 100, WA_InnerWidth, 200, WA_InnerHeight, 150,
        WA_Title, (IPTR)"seethru alpha", WA_DragBar, TRUE,
#ifdef WA_AlphaSurface
        WA_AlphaSurface, TRUE,
#endif
        TAG_DONE);
    c = OpenWindowTags(NULL, WA_Left, 100, WA_Top, 320, WA_InnerWidth, 200, WA_InnerHeight, 150,
        WA_Title, (IPTR)"seethru rectfill", WA_DragBar, TRUE,
#ifdef WA_Opaqueness
        WA_Opaqueness, 128,
#endif
        TAG_DONE);
    if (!a || !b || !c)
    {
        printf("seethru: window open failed a=%p b=%p c=%p\n", (void *)a, (void *)b, (void *)c);
        if (a) CloseWindow(a);
        if (b) CloseWindow(b);
        if (c) CloseWindow(c);
        return 20;
    }
    SetAPen(c->RPort, 3);
    RectFill(c->RPort, c->BorderLeft, c->BorderTop, c->BorderLeft + 199, c->BorderTop + 149);
    SetAPen(a->RPort, 0);
    FillPixelArray(a->RPort, a->BorderLeft, a->BorderTop, 200, 150, 0xFFFF0000);
    /* RECTFMT_ARGB is bytes A, R, G, B in memory order. */
    for (x = 0; x < 200; x++)
    {
        row[x * 4] = (UBYTE)(x * 255 / 199);
        row[x * 4 + 1] = 0;
        row[x * 4 + 2] = 255;
        row[x * 4 + 3] = 0;
    }
    for (y = 0; y < 150; y++)
        WritePixelArray(row, 0, 0, sizeof row, b->RPort, b->BorderLeft, b->BorderTop + y, 200, 1, RECTFMT_ARGB);
#ifdef LAYERCOMPOSITED
    printf("seethru: composited opacity=%d alpha=%d rectfill=%d\n",
        (a->WLayer->Flags & LAYERCOMPOSITED) != 0, (b->WLayer->Flags & LAYERCOMPOSITED) != 0,
        (c->WLayer->Flags & LAYERCOMPOSITED) != 0);
#else
    printf("seethru: SDK without composited layers\n");
#endif
    fflush(stdout);
    Wait(SIGBREAKF_CTRL_C);
    CloseWindow(c);
    CloseWindow(b);
    CloseWindow(a);
    return 0;
}
