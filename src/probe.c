/* PROBE mode: records what the public read-back paths return, for the
   question whether a capture can include see-through (composited) windows.
   Writes into one directory: report.txt, screen.png (screen bitmap),
   per composited layer layer-<n>.png (its surface read through its RastPort)
   and layer-<n>-alpha.png (the alpha byte as grey), and composite.png (the
   screen bitmap with those surfaces laid over it by their alpha bytes, back
   to front; an all-zero alpha is treated as opaque). The composite is an
   experiment, not what the display shows: opacity and shapes are not read. */
#include "capture.h"
#include "names.h"
#include "probe.h"

#include <exec/memory.h>
#include <dos/dos.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>

#include <stdio.h>
#include <string.h>

#define MAX_LAYERS 32

struct probe_layer
{
    struct Layer *layer;
    LONG x, y, w, h;
    ULONG flags;
    char title[64];
};

static void report(BPTR fh, const char *fmt, LONG a, LONG b, LONG c, LONG d)
{
    char line[256];

    snprintf(line, sizeof line, fmt, (long)a, (long)b, (long)c, (long)d);
    FPuts(fh, (STRPTR)line);
}

static void fput(BPTR fh, const char *fmt, const char *arg)
{
    char line[600];

    snprintf(line, sizeof line, fmt, arg);
    FPuts(fh, (STRPTR)line);
}

static BOOL save_in(CONST_STRPTR dir, const char *file, const struct cap_image *img, BPTR fh)
{
    char path[512], msg[256];
    LONG err;

    if (!cap_join(path, sizeof path, (const char *)dir, file))
        return FALSE;
    if (cap_save_png(img, 0, 0, img->width, img->height, (CONST_STRPTR)path, TRUE, &err, msg, sizeof msg))
    {
        fput(fh, "saved %s\n", path);
        return TRUE;
    }
    fput(fh, "FAILED %s\n", msg);
    return FALSE;
}

LONG cap_probe(CONST_STRPTR pubscreen, CONST_STRPTR dir)
{
    struct Screen *scr;
    struct cap_image screen = { NULL, 0, 0 }, comp = { NULL, 0, 0 };
    struct probe_layer layers[MAX_LAYERS];
    LONG nlayers = 0, total = 0, i, rc = RETURN_OK;
    char path[512];
    BPTR fh;
    struct Layer *l;

    if (!cap_join(path, sizeof path, (const char *)dir, "report.txt") ||
        !(fh = Open((CONST_STRPTR)path, MODE_NEWFILE)))
    {
        PrintFault(IoErr(), (STRPTR)"AROSCapture PROBE: cannot create report.txt");
        return RETURN_FAIL;
    }
    scr = LockPubScreen(pubscreen);
    if (!scr)
    {
        FPuts(fh, (STRPTR)"no public screen\n");
        Close(fh);
        return RETURN_FAIL;
    }
    report(fh, "screen %ldx%ld depth %ld modeid 0x%08lx\n", scr->Width, scr->Height,
        GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH), GetVPModeID(&scr->ViewPort));
    report(fh, "graphics.library %ld.%ld, layers.library %ld.%ld\n",
        GfxBase->LibNode.lib_Version, GfxBase->LibNode.lib_Revision,
        LayersBase->lib_Version, LayersBase->lib_Revision);
#ifdef LAYERCOMPOSITED
    FPuts(fh, (STRPTR)"built with LAYERCOMPOSITED\n");
#else
    FPuts(fh, (STRPTR)"built without LAYERCOMPOSITED (SDK has no composited layers)\n");
#endif

    if (!cap_grab(scr, &screen))
    {
        FPuts(fh, (STRPTR)"out of memory for the screen copy\n");
        UnlockPubScreen(NULL, scr);
        Close(fh);
        return RETURN_FAIL;
    }

    /* Layers front to back; keep the composited ones. */
    LockLayerInfo(&scr->LayerInfo);
    for (l = scr->LayerInfo.top_layer; l; l = l->back)
    {
        struct Window *w = (struct Window *)l->Window;

        total++;
        report(fh, "layer %ld bounds %ld,%ld", total, l->bounds.MinX, l->bounds.MinY, 0);
        report(fh, "-%ld,%ld flags 0x%04lx", l->bounds.MaxX, l->bounds.MaxY, l->Flags, 0);
        fput(fh, " window \"%s\"\n", w && w->Title ? (const char *)w->Title : "");
#ifdef LAYERCOMPOSITED
        if ((l->Flags & LAYERCOMPOSITED) && nlayers < MAX_LAYERS)
        {
            struct probe_layer *p = &layers[nlayers++];

            p->layer = l;
            p->x = l->bounds.MinX;
            p->y = l->bounds.MinY;
            p->w = l->bounds.MaxX - l->bounds.MinX + 1;
            p->h = l->bounds.MaxY - l->bounds.MinY + 1;
            p->flags = l->Flags;
            snprintf(p->title, sizeof p->title, "%s", w && w->Title ? (const char *)w->Title : "");
        }
#endif
    }
    report(fh, "%ld layers, %ld composited\n", total, nlayers, 0, 0);

    save_in(dir, "screen.png", &screen, fh);

    comp.width = screen.width;
    comp.height = screen.height;
    comp.pixels = AllocVec((ULONG)comp.width * comp.height * 4, MEMF_ANY);
    if (comp.pixels)
        CopyMem(screen.pixels, comp.pixels, (ULONG)comp.width * comp.height * 4);

    /* Back to front, so that front layers end on top in the composite. */
    for (i = nlayers - 1; i >= 0; i--)
    {
        struct probe_layer *p = &layers[i];
        struct cap_image surf = { NULL, p->w, p->h }, alpha = { NULL, p->w, p->h };
        ULONG count = (ULONG)p->w * p->h, j, hist[4] = { 0, 0, 0, 0 };
        UBYTE amin = 255, amax = 0;
        char file[40];
        LONG x, y;

        surf.pixels = AllocVec(count * 4, MEMF_ANY | MEMF_CLEAR);
        alpha.pixels = AllocVec(count * 4, MEMF_ANY);
        if (!surf.pixels || !alpha.pixels)
        {
            FPuts(fh, (STRPTR)"out of memory for a layer surface\n");
            rc = RETURN_WARN;
            if (surf.pixels) FreeVec(surf.pixels);
            if (alpha.pixels) FreeVec(alpha.pixels);
            continue;
        }
        ReadPixelArray(surf.pixels, 0, 0, p->w * 4, p->layer->rp, 0, 0, p->w, p->h, RECTFMT_ARGB);
        for (j = 0; j < count; j++)
        {
            UBYTE a = surf.pixels[j * 4];

            if (a < amin) amin = a;
            if (a > amax) amax = a;
            hist[a == 0 ? 0 : a == 255 ? 3 : a < 128 ? 1 : 2]++;
            alpha.pixels[j * 4] = 255;
            alpha.pixels[j * 4 + 1] = alpha.pixels[j * 4 + 2] = alpha.pixels[j * 4 + 3] = a;
        }
        report(fh, "composited layer %ld at %ld,%ld size %ld", i + 1, p->x, p->y, p->w);
        report(fh, "x%ld flags 0x%04lx alpha min %ld max %ld", p->h, p->flags, amin, amax);
        report(fh, " histogram 0:%ld 1-127:%ld 128-254:%ld 255:%ld", hist[0], hist[1], hist[2], hist[3]);
        fput(fh, " \"%s\"\n", p->title);
        snprintf(file, sizeof file, "layer-%ld.png", (long)(i + 1));
        save_in(dir, file, &surf, fh);
        snprintf(file, sizeof file, "layer-%ld-alpha.png", (long)(i + 1));
        save_in(dir, file, &alpha, fh);

        if (comp.pixels)
        {
            for (y = 0; y < p->h; y++)
            {
                LONG dy = p->y + y;

                if (dy < 0 || dy >= comp.height)
                    continue;
                for (x = 0; x < p->w; x++)
                {
                    LONG dx = p->x + x;
                    UBYTE *s = surf.pixels + ((ULONG)y * p->w + x) * 4;
                    UBYTE *d;
                    ULONG a, k;

                    if (dx < 0 || dx >= comp.width)
                        continue;
                    d = comp.pixels + ((ULONG)dy * comp.width + dx) * 4;
                    a = amax == 0 ? 255 : s[0];
                    for (k = 1; k < 4; k++)
                        d[k] = (UBYTE)((s[k] * a + d[k] * (255 - a) + 127) / 255);
                }
            }
        }
        FreeVec(surf.pixels);
        FreeVec(alpha.pixels);
    }
    UnlockLayerInfo(&scr->LayerInfo);
    UnlockPubScreen(NULL, scr);

    if (comp.pixels)
    {
        save_in(dir, "composite.png", &comp, fh);
        FreeVec(comp.pixels);
    }
    cap_free(&screen);
    FPuts(fh, (STRPTR)"done\n");
    Close(fh);
    return rc;
}
