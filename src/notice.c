#include "notice.h"

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include <string.h>

#define PAD 8

void cap_notice(CONST_STRPTR pubscreen, const char *text, LONG ticks)
{
    struct Screen *scr = LockPubScreen(pubscreen);
    struct DrawInfo *dri;
    struct Window *win;
    struct RastPort *rp;
    struct TextFont *font;
    ULONG len = strlen(text), shown, w, h;
    LONG left;

    if (!scr)
        return;
    dri = GetScreenDrawInfo(scr);
    font = OpenFont(scr->Font);
    if (!dri || !font)
    {
        if (font) CloseFont(font);
        if (dri) FreeScreenDrawInfo(scr, dri);
        UnlockPubScreen(NULL, scr);
        return;
    }
    /* As much of the text as fits in the screen width. */
    {
        struct RastPort trp;

        InitRastPort(&trp);
        SetFont(&trp, font);
        for (shown = len; shown > 0 && (LONG)TextLength(&trp, (STRPTR)text, shown) + 2 * PAD > (LONG)scr->Width; shown--)
            ;
        w = TextLength(&trp, (STRPTR)text, shown) + 2 * PAD;
    }
    h = font->tf_YSize + 2 * PAD;
    /* At the top, under the screen title: docks usually sit at the bottom. */
    left = ((LONG)scr->Width - (LONG)w) / 2;
    win = OpenWindowTags(NULL,
        WA_PubScreen, (IPTR)scr,
        WA_Left, left < 0 ? 0 : left,
        WA_Top, scr->BarHeight + 1 + PAD,
        WA_Width, w, WA_Height, h,
        WA_Borderless, TRUE, WA_Activate, FALSE, WA_RMBTrap, TRUE,
        WA_IDCMP, IDCMP_MOUSEBUTTONS,
        TAG_DONE);
    if (win)
    {
        ULONG sig = 1UL << win->UserPort->mp_SigBit;
        LONG waited;
        BOOL done = FALSE;

        rp = win->RPort;
        SetFont(rp, font);
        SetAPen(rp, dri->dri_Pens[SHINEPEN]);
        RectFill(rp, 0, 0, w - 1, h - 1);
        SetAPen(rp, dri->dri_Pens[SHADOWPEN]);
        Move(rp, 0, h - 1); Draw(rp, 0, 0); Draw(rp, w - 1, 0); Draw(rp, w - 1, h - 1); Draw(rp, 0, h - 1);
        SetAPen(rp, dri->dri_Pens[TEXTPEN]);
        SetDrMd(rp, JAM1);
        Move(rp, PAD, PAD + font->tf_Baseline);
        Text(rp, (STRPTR)text, shown);
        /* Until the time is up or the notice is clicked. */
        for (waited = 0; waited < ticks && !done; waited += 5)
        {
            struct IntuiMessage *im;

            Delay(5);
            if (SetSignal(0, sig) & sig)
                while ((im = (struct IntuiMessage *)GetMsg(win->UserPort)))
                {
                    if (im->Class == IDCMP_MOUSEBUTTONS)
                        done = TRUE;
                    ReplyMsg((struct Message *)im);
                }
        }
        CloseWindow(win);
    }
    CloseFont(font);
    FreeScreenDrawInfo(scr, dri);
    UnlockPubScreen(NULL, scr);
}
