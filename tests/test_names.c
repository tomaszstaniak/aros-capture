/* Host test for src/names.c: cc -I src tests/test_names.c src/names.c */
#include "names.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char *what, const char *got, const char *want)
{
    if (strcmp(got, want) != 0)
    {
        printf("FAIL %s: got \"%s\", want \"%s\"\n", what, got, want);
        failures++;
    }
}

int main(void)
{
    char b[64];
    struct cap_time t = { 2026, 11, 6, 1, 2, 3 };

    cap_join(b, sizeof b, "RAM:", "a.png");         expect("volume", b, "RAM:a.png");
    cap_join(b, sizeof b, "SYS:Pics", "a.png");     expect("dir", b, "SYS:Pics/a.png");
    cap_join(b, sizeof b, "SYS:Pics/", "a.png");    expect("slash", b, "SYS:Pics/a.png");
    cap_join(b, sizeof b, "", "a.png");             expect("empty", b, "a.png");
    cap_join(b, sizeof b, "Work:My Pictures", "a.png"); expect("space", b, "Work:My Pictures/a.png");
    if (cap_join(b, 8, "SYS:Pics", "a.png")) { puts("FAIL join overflow accepted"); failures++; }
    if (!cap_join(b, 15, "SYS:Pics", "a.png")) { puts("FAIL join exact fit refused"); failures++; }

    cap_name(b, sizeof b, &t, 1); expect("name", b, "Capture-2026-11-06_01-02-03.png");
    cap_name(b, sizeof b, &t, 2); expect("name2", b, "Capture-2026-11-06_01-02-03-2.png");
    if (cap_name(b, 20, &t, 1)) { puts("FAIL name overflow accepted"); failures++; }

    if (!cap_has_png_suffix("x.PNG") || !cap_has_png_suffix("a.png") ||
        cap_has_png_suffix("png") || cap_has_png_suffix("a.pngx") || cap_has_png_suffix("a.ppm"))
    { puts("FAIL png suffix"); failures++; }

    if (failures == 0)
        puts("test_names: all passed");
    return failures != 0;
}
