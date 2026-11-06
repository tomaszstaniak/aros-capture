/* Host tool: renders an SVG to a PAM (RGB_ALPHA) with the svgicon renderer,
   the same one that draws the toolbar icons on AROS.
   usage: svgraster in.svg size out.pam */
#include "svgicon.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    char *src;
    SvgIconDocument *doc;
    uint8_t *px;
    unsigned size, i;
    SvgIconStatus st;

    if (argc != 4 || (size = (unsigned)atoi(argv[2])) == 0)
    {
        fprintf(stderr, "usage: svgraster in.svg size out.pam\n");
        return 2;
    }
    if (!(f = fopen(argv[1], "rb")) || fseek(f, 0, SEEK_END) || (n = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET))
        return perror(argv[1]), 1;
    if (!(src = malloc((size_t)n)) || fread(src, 1, (size_t)n, f) != (size_t)n)
        return perror(argv[1]), 1;
    fclose(f);
    if ((st = SvgIconLoad(src, (size_t)n, &doc)) != SVGICON_OK)
        return fprintf(stderr, "%s: %s\n", argv[1], SvgIconStatusName(st)), 1;
    if ((st = SvgIconRender(doc, size, size, &px)) != SVGICON_OK)
        return fprintf(stderr, "render: %s\n", SvgIconStatusName(st)), 1;
    if (!(f = fopen(argv[3], "wb")))
        return perror(argv[3]), 1;
    fprintf(f, "P7\nWIDTH %u\nHEIGHT %u\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", size, size);
    for (i = 0; i < size * size; i++)
    {
        uint8_t rgba[4] = { px[i * 4 + 1], px[i * 4 + 2], px[i * 4 + 3], px[i * 4] };
        fwrite(rgba, 1, 4, f);
    }
    fclose(f);
    SvgIconFreePixels(px);
    SvgIconFree(doc);
    free(src);
    return 0;
}
