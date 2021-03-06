/*
 * Copyright 2011 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <verto-libevent.h>
#include <verto-module.h>

#include <event2/event_compat.h>

/* This is technically not exposed in any headers, but it is exported from
 * the binary. Without it, we can't provide compatibility with libevent's
 * sense of "global." */
extern struct event_base *event_global_current_base_;

static void
libevent_ctx_free(void *priv)
{
    if (priv != event_global_current_base_)
        event_base_free(priv);
}

static void
libevent_ctx_run(void *priv)
{
    event_base_dispatch(priv);
}

static void
libevent_ctx_run_once(void *priv)
{
    event_base_loop(priv, EVLOOP_ONCE);
}

static void
libevent_ctx_break(void *priv)
{
    event_base_loopbreak(priv);
}

static void
libevent_ctx_reinitialize(void *priv)
{
    event_reinit(priv);
}

static void
libevent_callback(evutil_socket_t socket, short type, void *data)
{
    verto_fire(data);
}

static void *
libevent_ctx_add(void *ctx, const verto_ev *ev, verto_ev_flag *flags)
{
    struct event *priv = NULL;
    struct timeval *timeout = NULL;
    struct timeval tv;
    int libeventflags = 0;

    if (*flags & VERTO_EV_FLAG_PERSIST)
        libeventflags |= EV_PERSIST;

    switch (verto_get_type(ev)) {
    case VERTO_EV_TYPE_IO:
        if (*flags & VERTO_EV_FLAG_IO_READ)
            libeventflags |= EV_READ;
        if (*flags & VERTO_EV_FLAG_IO_WRITE)
            libeventflags |= EV_WRITE;
        priv = event_new(ctx, verto_get_fd(ev), libeventflags,
                         libevent_callback, (void *) ev);
        break;
    case VERTO_EV_TYPE_TIMEOUT:
        timeout = &tv;
        tv.tv_sec = verto_get_interval(ev) / 1000;
        tv.tv_usec = verto_get_interval(ev) % 1000 * 1000;
        priv = event_new(ctx, -1, EV_TIMEOUT | libeventflags,
                         libevent_callback, (void *) ev);
        break;
    case VERTO_EV_TYPE_SIGNAL:
        priv = event_new(ctx, verto_get_signal(ev),
                         EV_SIGNAL | libeventflags,
                         libevent_callback, (void *) ev);
        break;
    case VERTO_EV_TYPE_IDLE:
    case VERTO_EV_TYPE_CHILD:
    default:
        return NULL; /* Not supported */
    }

    if (!priv)
        return NULL;

    if (*flags & VERTO_EV_FLAG_PRIORITY_HIGH)
        event_priority_set(priv, 0);
    else if (*flags & VERTO_EV_FLAG_PRIORITY_MEDIUM)
        event_priority_set(priv, 1);
    else if (*flags & VERTO_EV_FLAG_PRIORITY_LOW)
        event_priority_set(priv, 2);

    event_add(priv, timeout);
    return priv;
}

static void
libevent_ctx_del(void *ctx, const verto_ev *ev, void *evpriv)
{
    event_del(evpriv);
    event_free(evpriv);
}

VERTO_MODULE(libevent, event_base_init,
             VERTO_EV_TYPE_IO |
             VERTO_EV_TYPE_TIMEOUT |
             VERTO_EV_TYPE_SIGNAL);

verto_ctx *
verto_new_libevent(void)
{
    return verto_convert_libevent(event_base_new());
}

verto_ctx *
verto_default_libevent(void)
{
    if (!event_global_current_base_)
        event_global_current_base_ = event_init();
    return verto_convert_libevent(event_global_current_base_);
}

verto_ctx *
verto_convert_libevent(struct event_base* base)
{
    event_base_priority_init(base, 3);
    return verto_convert(libevent, base);
}
