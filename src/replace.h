/* Replacing an existing file with a complete new one. AROS Rename refuses
   an existing target and handlers offer no common atomic replace, so the
   old file is first renamed aside, the new one takes the name, and only
   then the old one is deleted. If the new one cannot take the name, the old
   one is renamed back. At no point does a step delete the only copy of
   either picture. Not crash-safe: a crash between the steps leaves the old
   file under its aside name. Pure C over injected file operations, so every
   failure path can be tested on the build host. */
#ifndef CAPTURE_REPLACE_H
#define CAPTURE_REPLACE_H

struct rep_ops
{
    void *ctx;
    /* Both return 0 on success, otherwise an error code (a DOS error on
       AROS). */
    long (*rename)(void *ctx, const char *from, const char *to);
    long (*remove)(void *ctx, const char *path);
};

enum rep_state
{
    REP_DONE,          /* new file under path, old one deleted */
    REP_DONE_OLD_LEFT, /* new file under path, old one still under aside */
    REP_NOT_ASIDE,     /* old file could not be moved: nothing changed */
    REP_ROLLED_BACK,   /* new file kept as tmp, old one back under path */
    REP_STRANDED       /* old file under aside, new one under tmp, path free */
};

/* Publishes tmp as path, where path exists. aside is an unused name for the
   old file on the same volume. Returns the state; *err gets the error of the
   step that failed (for REP_DONE_OLD_LEFT the delete's error). The caller
   deletes tmp only for REP_NOT_ASIDE and REP_ROLLED_BACK. */
int rep_publish(const struct rep_ops *ops, const char *tmp, const char *path,
                const char *aside, long *err);

#endif
