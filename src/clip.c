/* Clipboard: a picture as a 24-bit ILBM, text as FTXT, both on unit 0.
   pictureclass has no DTM_COPY on AROS, so the IFF is written here. */
#include "clip.h"

#include <exec/memory.h>
#include <libraries/iffparse.h>
#include <proto/exec.h>
#include <proto/iffparse.h>

#include <string.h>

#define ID_ILBM MAKE_ID('I','L','B','M')
#define ID_BMHD MAKE_ID('B','M','H','D')
#define ID_BODY MAKE_ID('B','O','D','Y')
#define ID_FTXT MAKE_ID('F','T','X','T')
#define ID_CHRS MAKE_ID('C','H','R','S')

struct Library *IFFParseBase;

static struct IFFHandle *open_clip(void)
{
    struct IFFHandle *iff;

    IFFParseBase = OpenLibrary((CONST_STRPTR)"iffparse.library", 39);
    if (!IFFParseBase)
        return NULL;
    iff = AllocIFF();
    if (iff)
    {
        iff->iff_Stream = (IPTR)OpenClipboard(0);
        if (iff->iff_Stream)
        {
            InitIFFasClip(iff);
            if (OpenIFF(iff, IFFF_WRITE) == 0)
                return iff;
            CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
        }
        FreeIFF(iff);
    }
    CloseLibrary(IFFParseBase);
    IFFParseBase = NULL;
    return NULL;
}

static void close_clip(struct IFFHandle *iff)
{
    CloseIFF(iff);
    CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
    FreeIFF(iff);
    CloseLibrary(IFFParseBase);
    IFFParseBase = NULL;
}

static void put_be16(UBYTE *p, UWORD v)
{
    p[0] = (UBYTE)(v >> 8);
    p[1] = (UBYTE)v;
}

BOOL clip_picture(const UBYTE *argb, LONG w, LONG h)
{
    struct IFFHandle *iff;
    UBYTE bmhd[20];
    UBYTE *row;
    ULONG rowbytes = ((ULONG)(w + 15) / 16) * 2;
    BOOL ok = FALSE;
    LONG y;

    if (w <= 0 || h <= 0 || w > 65535 || h > 65535)
        return FALSE;
    row = AllocVec(rowbytes * 24, MEMF_ANY);
    if (!row)
        return FALSE;
    iff = open_clip();
    if (!iff)
    {
        FreeVec(row);
        return FALSE;
    }

    memset(bmhd, 0, sizeof bmhd);
    put_be16(bmhd, (UWORD)w);
    put_be16(bmhd + 2, (UWORD)h);
    bmhd[8] = 24;            /* planes */
    bmhd[9] = 0;             /* no masking */
    bmhd[10] = 0;            /* no compression */
    bmhd[14] = 1;            /* aspect 1:1 */
    bmhd[15] = 1;
    put_be16(bmhd + 16, (UWORD)w);
    put_be16(bmhd + 18, (UWORD)h);

    if (PushChunk(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN) == 0 &&
        PushChunk(iff, 0, ID_BMHD, sizeof bmhd) == 0 &&
        WriteChunkBytes(iff, bmhd, sizeof bmhd) == sizeof bmhd &&
        PopChunk(iff) == 0 &&
        PushChunk(iff, 0, ID_BODY, IFFSIZE_UNKNOWN) == 0)
    {
        ok = TRUE;
        /* Deep ILBM: per row, planes 0-7 red, 8-15 green, 16-23 blue,
           least significant bit first. */
        for (y = 0; y < h && ok; y++)
        {
            const UBYTE *src = argb + (ULONG)y * (ULONG)w * 4;
            LONG x;
            int plane;

            memset(row, 0, rowbytes * 24);
            for (x = 0; x < w; x++)
            {
                UBYTE bit = (UBYTE)(0x80 >> (x & 7));
                ULONG off = (ULONG)x >> 3;

                for (plane = 0; plane < 24; plane++)
                    if (src[x * 4 + 1 + plane / 8] & (1 << (plane & 7)))
                        row[plane * rowbytes + off] |= bit;
            }
            ok = WriteChunkBytes(iff, row, rowbytes * 24) == (LONG)(rowbytes * 24);
        }
        ok = ok && PopChunk(iff) == 0 && PopChunk(iff) == 0;
    }
    close_clip(iff);
    FreeVec(row);
    return ok;
}

BOOL clip_text(const char *text)
{
    struct IFFHandle *iff = open_clip();
    LONG len = (LONG)strlen(text);
    BOOL ok;

    if (!iff)
        return FALSE;
    ok = PushChunk(iff, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN) == 0 &&
         PushChunk(iff, 0, ID_CHRS, len) == 0 &&
         WriteChunkBytes(iff, (APTR)text, len) == len &&
         PopChunk(iff) == 0 && PopChunk(iff) == 0;
    close_clip(iff);
    return ok;
}
