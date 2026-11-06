/* Test aid (not part of the program): prints the pixel formats of the
   screen bitmap and of a WA_Opaqueness window's layer surface, and the ARGB
   that ReadPixelArray returns for a pen-filled pixel of each, then keeps the
   window open for SECONDS (default 20) so the display can be compared.
   Usage: pixfmt [OPACITY] [SECONDS] */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <graphics/layers.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

#include <stdio.h>
#include <stdlib.h>

static void show(const char *what, struct BitMap *bm)
{
    printf("%s: depth %lu pixfmt %lu bpp %lu iscyber %lu\n", what,
        (unsigned long)GetBitMapAttr(bm, BMA_DEPTH),
        (unsigned long)GetCyberMapAttr(bm, CYBRMATTR_PIXFMT),
        (unsigned long)GetCyberMapAttr(bm, CYBRMATTR_BPPIX),
        (unsigned long)GetCyberMapAttr(bm, CYBRMATTR_ISCYBERGFX));
}

static void pixel(const char *what, struct RastPort *rp, LONG x, LONG y)
{
    UBYTE p[4] = { 0, 0, 0, 0 };

    ReadPixelArray(p, 0, 0, 4, rp, x, y, 1, 1, RECTFMT_ARGB);
    printf("%s at %ld,%ld: A %u R %u G %u B %u\n", what, (long)x, (long)y, p[0], p[1], p[2], p[3]);
}

int main(int argc, char **argv)
{
    LONG opacity = argc > 1 ? atol(argv[1]) : 128, secs = argc > 2 ? atol(argv[2]) : 20;
    struct Screen *scr = LockPubScreen(NULL);
    struct Window *win;

    if (!scr)
        return 20;
    show("screen bitmap", scr->RastPort.BitMap);
    SetAPen(&scr->RastPort, 3);
    pixel("screen pixel", &scr->RastPort, 5, scr->Height - 5);
    win = OpenWindowTags(NULL, WA_PubScreen, (IPTR)scr, WA_Left, 100, WA_Top, 100,
        WA_Width, 240, WA_Height, 160, WA_Title, (IPTR)"pixfmt", WA_DragBar, TRUE,
        WA_Opaqueness, opacity, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!win)
    {
        printf("no window\n");
        return 20;
    }
#ifdef LAYERCOMPOSITED
    printf("layer flags 0x%04lx composited %d\n", (unsigned long)win->WLayer->Flags,
        (win->WLayer->Flags & LAYERCOMPOSITED) != 0);
#endif
    show("window rastport bitmap", win->RPort->BitMap);
    SetAPen(win->RPort, 3);
    RectFill(win->RPort, win->BorderLeft + 10, win->BorderTop + 10, win->BorderLeft + 120, win->BorderTop + 80);
    pixel("pen 3 content", win->RPort, win->BorderLeft + 20, win->BorderTop + 20);
    pixel("title bar", win->RPort, 60, 3);
    fflush(stdout);
    Delay(secs * 50);
    CloseWindow(win);
    return 0;
}
