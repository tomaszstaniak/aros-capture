/* Region choice. The screen has already been captured; the choice is made
   over that frozen copy in a borderless window, so neither the window, the
   rectangle nor the hint can appear in the saved picture. */
#include "capture.h"

#include <exec/ports.h>
#include <devices/inputevent.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>

#include <stdio.h>
#include <string.h>

#define KEY_RETURN 0x44
#define KEY_ENTER  0x43
#define KEY_ESC    0x45

static const char hint[] = "Drag to choose an area  -  Return: whole screen  -  Esc or right button: cancel";

/* Inverts the outline of the box; drawing it twice restores the picture.
   Edges are filled without overlapping, so no corner is inverted twice. */
static void outline(struct RastPort *rp, LONG ax, LONG ay, LONG bx, LONG by)
{
    LONG x0 = ax < bx ? ax : bx, x1 = ax < bx ? bx : ax;
    LONG y0 = ay < by ? ay : by, y1 = ay < by ? by : ay;

    RectFill(rp, x0, y0, x1, y0);
    if (y1 > y0)
        RectFill(rp, x0, y1, x1, y1);
    if (y1 - y0 > 1)
    {
        RectFill(rp, x0, y0 + 1, x0, y1 - 1);
        if (x1 > x0)
            RectFill(rp, x1, y0 + 1, x1, y1 - 1);
    }
}

static LONG clamp(LONG v, LONG lo, LONG hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

enum cap_status cap_select(struct Screen *scr, const struct cap_image *img,
    LONG *ox, LONG *oy, LONG *ow, LONG *oh, char *msg, ULONG msgsize)
{
    struct Window *win;
    struct RastPort *rp;
    struct IntuiMessage *im;
    enum cap_status result = CAP_CANCELLED;
    BOOL done = FALSE, dragging = FALSE, drawn = FALSE, hinted = FALSE;
    LONG sx = 0, sy = 0, cx = 0, cy = 0;
    LONG hint_top = 0, hint_h = 0;

    win = OpenWindowTags(NULL,
        WA_CustomScreen, (IPTR)scr,
        WA_Left, 0, WA_Top, 0,
        WA_Width, img->width, WA_Height, img->height,
        WA_Borderless, TRUE,
        WA_Activate, TRUE,
        WA_RMBTrap, TRUE,
        WA_ReportMouse, TRUE,
        WA_SmartRefresh, TRUE,
        WA_NoCareRefresh, TRUE,
        WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY,
        TAG_DONE);
    if (!win)
    {
        snprintf(msg, msgsize, "Cannot open the area selection window");
        return CAP_FAILED;
    }
    rp = win->RPort;
    WritePixelArray(img->pixels, 0, 0, img->width * 4, rp, 0, 0,
        img->width, img->height, RECTFMT_ARGB);

    /* The hint goes at the bottom and disappears with the first press. */
    {
        LONG tw = TextLength(rp, (STRPTR)hint, sizeof hint - 1);
        LONG th = rp->TxHeight;
        LONG tx = (img->width - tw) / 2;

        if (tx < 4)
            tx = 4;
        hint_h = th + 8;
        hint_top = img->height - hint_h - 8;
        if (hint_top > 0)
        {
            SetDrMd(rp, JAM2);
            SetAPen(rp, 1);
            RectFill(rp, 0, hint_top, img->width - 1, hint_top + hint_h - 1);
            SetAPen(rp, 2);
            SetBPen(rp, 1);
            Move(rp, tx, hint_top + 4 + rp->TxBaseline);
            Text(rp, (STRPTR)hint, sizeof hint - 1);
            hinted = TRUE;
        }
    }
    SetDrMd(rp, COMPLEMENT);

    while (!done)
    {
        ULONG sigs = Wait((1UL << win->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);
        BOOL moved = FALSE;

        if (sigs & SIGBREAKF_CTRL_C)
            break;
        while (!done && (im = (struct IntuiMessage *)GetMsg(win->UserPort)))
        {
            ULONG class = im->Class;
            UWORD code = im->Code;
            LONG mx = clamp(im->MouseX, 0, img->width - 1);
            LONG my = clamp(im->MouseY, 0, img->height - 1);

            ReplyMsg((struct Message *)im);
            switch (class)
            {
            case IDCMP_MOUSEBUTTONS:
                if (code == SELECTDOWN)
                {
                    if (hinted)
                    {
                        /* Put the picture back where the hint was. */
                        SetDrMd(rp, JAM2);
                        WritePixelArray(img->pixels, 0, hint_top, img->width * 4, rp,
                            0, hint_top, img->width, hint_h, RECTFMT_ARGB);
                        SetDrMd(rp, COMPLEMENT);
                        hinted = FALSE;
                    }
                    dragging = TRUE;
                    sx = cx = mx;
                    sy = cy = my;
                    outline(rp, sx, sy, cx, cy);
                    drawn = TRUE;
                }
                else if (code == SELECTUP && dragging)
                {
                    if (drawn)
                        outline(rp, sx, sy, cx, cy);
                    drawn = FALSE;
                    dragging = FALSE;
                    cx = mx;
                    cy = my;
                    if (cx != sx && cy != sy)
                    {
                        *ox = sx < cx ? sx : cx;
                        *oy = sy < cy ? sy : cy;
                        *ow = (sx < cx ? cx - sx : sx - cx) + 1;
                        *oh = (sy < cy ? cy - sy : sy - cy) + 1;
                        result = CAP_OK;
                        done = TRUE;
                    }
                }
                else if (code == MENUDOWN)
                    done = TRUE;
                break;
            case IDCMP_MOUSEMOVE:
                if (dragging && (mx != cx || my != cy))
                {
                    if (!moved && drawn)
                        outline(rp, sx, sy, cx, cy);
                    moved = TRUE;
                    drawn = FALSE;
                    cx = mx;
                    cy = my;
                }
                break;
            case IDCMP_RAWKEY:
                if (code == KEY_ESC)
                    done = TRUE;
                else if ((code == KEY_RETURN || code == KEY_ENTER) && !dragging)
                {
                    *ox = 0;
                    *oy = 0;
                    *ow = img->width;
                    *oh = img->height;
                    result = CAP_OK;
                    done = TRUE;
                }
                break;
            }
        }
        if (!done && moved)
        {
            outline(rp, sx, sy, cx, cy);
            drawn = TRUE;
        }
    }

    /* Nothing more may arrive for this port once it is closed. */
    while ((im = (struct IntuiMessage *)GetMsg(win->UserPort)))
        ReplyMsg((struct Message *)im);
    CloseWindow(win);
    if (result == CAP_CANCELLED)
        snprintf(msg, msgsize, "Cancelled");
    return result;
}
