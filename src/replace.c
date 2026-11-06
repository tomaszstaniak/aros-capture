#include "replace.h"

int rep_publish(const struct rep_ops *ops, const char *tmp, const char *path,
                const char *aside, long *err)
{
    long e;

    *err = 0;
    if ((e = ops->rename(ops->ctx, path, aside)) != 0)
    {
        *err = e;
        return REP_NOT_ASIDE;
    }
    if ((e = ops->rename(ops->ctx, tmp, path)) != 0)
    {
        *err = e;
        if (ops->rename(ops->ctx, aside, path) != 0)
            return REP_STRANDED;
        return REP_ROLLED_BACK;
    }
    if ((e = ops->remove(ops->ctx, aside)) != 0)
    {
        *err = e;
        return REP_DONE_OLD_LEFT;
    }
    return REP_DONE;
}
