/* Host test for src/replace.c: cc -I src tests/test_replace.c src/replace.c
   A tiny in-memory file table; each operation can be made to fail by its
   call number, so every step of rep_publish is checked for losing a file. */
#include "replace.h"

#include <stdio.h>
#include <string.h>

#define NFILES 8

struct fs
{
    char name[NFILES][32];
    char data[NFILES][16];
    int calls, fail_mask;  /* bit n set: operation n+1 fails */
};

static int find(struct fs *fs, const char *n)
{
    int i;

    for (i = 0; i < NFILES; i++)
        if (fs->name[i][0] && strcmp(fs->name[i], n) == 0)
            return i;
    return -1;
}

static void put(struct fs *fs, const char *n, const char *d)
{
    int i;

    for (i = 0; i < NFILES; i++)
        if (!fs->name[i][0])
        {
            strcpy(fs->name[i], n);
            strcpy(fs->data[i], d);
            return;
        }
}

static long fs_rename(void *ctx, const char *from, const char *to)
{
    struct fs *fs = ctx;
    int i = find(fs, from);

    if (fs->fail_mask & (1 << fs->calls++))
        return 225;                    /* any error code */
    if (i < 0)
        return 205;                    /* object not found */
    if (find(fs, to) >= 0)
        return 203;                    /* object exists, as AROS Rename */
    strcpy(fs->name[i], to);
    return 0;
}

static long fs_remove(void *ctx, const char *path)
{
    struct fs *fs = ctx;
    int i = find(fs, path);

    if (fs->fail_mask & (1 << fs->calls++))
        return 222;
    if (i < 0)
        return 205;
    fs->name[i][0] = 0;
    return 0;
}

static const char *at(struct fs *fs, const char *n)
{
    int i = find(fs, n);

    return i < 0 ? "-" : fs->data[i];
}

static int failures;

static void check(int fail_mask, int want_state, const char *path_has,
                  const char *tmp_has, const char *aside_has)
{
    struct fs fs;
    struct rep_ops ops = { &fs, fs_rename, fs_remove };
    long err;
    int state;

    memset(&fs, 0, sizeof fs);
    fs.fail_mask = fail_mask;
    put(&fs, "a.png", "old");
    put(&fs, "a.png.tmp", "new");
    state = rep_publish(&ops, "a.png.tmp", "a.png", "a.png.old", &err);
    if (state != want_state || strcmp(at(&fs, "a.png"), path_has) ||
        strcmp(at(&fs, "a.png.tmp"), tmp_has) || strcmp(at(&fs, "a.png.old"), aside_has) ||
        (state != REP_DONE && err == 0) || (state == REP_DONE && err != 0))
    {
        printf("FAIL fail_mask %d: state %d (want %d), a.png=%s tmp=%s old=%s err=%ld\n",
               fail_mask, state, want_state, at(&fs, "a.png"), at(&fs, "a.png.tmp"),
               at(&fs, "a.png.old"), err);
        failures++;
    }
    /* Whatever happened, both pictures must still exist somewhere. */
    if (strcmp(at(&fs, "a.png"), "new") && strcmp(at(&fs, "a.png.tmp"), "new"))
    {
        printf("FAIL fail_mask %d: new picture lost\n", fail_mask);
        failures++;
    }
    if (state != REP_DONE && strcmp(at(&fs, "a.png"), "old") && strcmp(at(&fs, "a.png.old"), "old"))
    {
        printf("FAIL fail_mask %d: old picture lost before the new one was in place\n", fail_mask);
        failures++;
    }
}

int main(void)
{
    check(0, REP_DONE, "new", "-", "-");
    check(1, REP_NOT_ASIDE, "old", "new", "-");       /* moving the old file aside fails */
    check(2, REP_ROLLED_BACK, "old", "new", "-");     /* new file cannot take the name */
    check(2 | 4, REP_STRANDED, "-", "new", "old");    /* ... and the old one cannot go back */
    check(4, REP_DONE_OLD_LEFT, "new", "-", "old");   /* deleting the old file fails */

    if (failures == 0)
        puts("test_replace: all passed");
    return failures != 0;
}
