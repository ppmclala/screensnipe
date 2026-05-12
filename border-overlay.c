#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <X11/Xft/Xft.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#define BTN_SIZE  40   /* diameter of circular buttons */
#define ICON_SIZE 14   /* white square / X line span inside button */
#define BTN_GAP    6   /* gap between buttons */


static Display *dpy;
static Window   overlay_win, border_win, stop_win, cancel_win;
static Pixmap   bg_pixmap;

static void cleanup(int sig) {
    (void)sig;
    if (dpy) {
        if (overlay_win) XDestroyWindow(dpy, overlay_win);
        if (border_win)  XDestroyWindow(dpy, border_win);
        if (stop_win)    XDestroyWindow(dpy, stop_win);
        if (cancel_win)  XDestroyWindow(dpy, cancel_win);
        if (bg_pixmap)   XFreePixmap(dpy, bg_pixmap);
        XFlush(dpy);
        XCloseDisplay(dpy);
    }
    exit(0);
}

static void make_circular(Window win, int size) {
    Pixmap mask = XCreatePixmap(dpy, win, (unsigned)size, (unsigned)size, 1);
    GC mgc = XCreateGC(dpy, mask, 0, NULL);
    XSetForeground(dpy, mgc, 0);
    XFillRectangle(dpy, mask, mgc, 0, 0, (unsigned)size, (unsigned)size);
    XSetForeground(dpy, mgc, 1);
    XFillArc(dpy, mask, mgc, 0, 0, (unsigned)size, (unsigned)size, 0, 360*64);
    XFreeGC(dpy, mgc);
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreePixmap(dpy, mask);
}

static void draw_stop_icon(GC gc) {
    XClearWindow(dpy, stop_win);
    int off = (BTN_SIZE - ICON_SIZE) / 2;
    XFillRectangle(dpy, stop_win, gc, off, off, ICON_SIZE, ICON_SIZE);
    XFlush(dpy);
}

static void draw_cancel_icon(GC gc) {
    XClearWindow(dpy, cancel_win);
    int pad = (BTN_SIZE - ICON_SIZE) / 2;
    XSetLineAttributes(dpy, gc, 3, LineSolid, CapRound, JoinRound);
    XDrawLine(dpy, cancel_win, gc, pad, pad, BTN_SIZE-pad, BTN_SIZE-pad);
    XDrawLine(dpy, cancel_win, gc, BTN_SIZE-pad, pad, pad, BTN_SIZE-pad);
    XFlush(dpy);
}

static void draw_countdown(XftDraw *draw, XftFont *font,
                            XftColor *color, int n, int w, int h) {
    XClearWindow(dpy, overlay_win);
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", n);
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, font, (FcChar8 *)buf, strlen(buf), &ext);
    int tx = (w - ext.width) / 2 - ext.x;
    int ty = (h + ext.height) / 2;
    XftDrawStringUtf8(draw, color, font, tx, ty, (FcChar8 *)buf, strlen(buf));
    XFlush(dpy);
}

int main(int argc, char *argv[]) {
    if (argc < 7) {
        fprintf(stderr, "Usage: %s X Y W H BORDER_PX DELAY [TARGET_PID]\n", argv[0]);
        return 1;
    }

    int   x          = atoi(argv[1]);
    int   y          = atoi(argv[2]);
    int   w          = atoi(argv[3]);
    int   h          = atoi(argv[4]);
    int   border     = atoi(argv[5]);
    int   delay      = atoi(argv[6]);
    pid_t target_pid = (argc > 7) ? (pid_t)atoi(argv[7]) : 0;

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open display\n"); return 1; }

    int    screen = DefaultScreen(dpy);
    Window root   = RootWindow(dpy, screen);

    /* --- Build dimmed background pixmap from a screen snapshot --- */
    XImage *snap = XGetImage(dpy, root, x, y, (unsigned)w, (unsigned)h,
                             AllPlanes, ZPixmap);

    /* Darken each pixel to 35% brightness */
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            unsigned long pixel = XGetPixel(snap, px, py);
            unsigned long r = ((pixel >> 16) & 0xFF) * 35 / 100;
            unsigned long g = ((pixel >>  8) & 0xFF) * 35 / 100;
            unsigned long b = ( pixel        & 0xFF) * 35 / 100;
            XPutPixel(snap, px, py, (r << 16) | (g << 8) | b);
        }
    }

    bg_pixmap = XCreatePixmap(dpy, root, (unsigned)w, (unsigned)h,
                              DefaultDepth(dpy, screen));
    GC bg_gc = XCreateGC(dpy, bg_pixmap, 0, NULL);
    XPutImage(dpy, bg_pixmap, bg_gc, snap, 0, 0, 0, 0, (unsigned)w, (unsigned)h);
    XDestroyImage(snap);
    XFreeGC(dpy, bg_gc);

    XSetWindowAttributes attrs = {0};
    attrs.override_redirect = True;

    /* --- Countdown overlay --- */
    attrs.background_pixmap = bg_pixmap;
    attrs.event_mask        = ExposureMask;
    overlay_win = XCreateWindow(dpy, root,
                                x, y, (unsigned)w, (unsigned)h, 0,
                                DefaultDepth(dpy, screen), InputOutput,
                                DefaultVisual(dpy, screen),
                                CWOverrideRedirect | CWBackPixmap | CWEventMask,
                                &attrs);

    /* --- Border (hollow rectangle outside recording area) --- */
    int bx = x - border, by = y - border;
    int bw = w + 2*border, bh = h + 2*border;
    attrs.background_pixel = 0x33CC33;
    attrs.event_mask       = 0;
    border_win = XCreateWindow(dpy, root,
                               bx, by, (unsigned)bw, (unsigned)bh, 0,
                               DefaultDepth(dpy, screen), InputOutput,
                               DefaultVisual(dpy, screen),
                               CWOverrideRedirect | CWBackPixel, &attrs);

    XRectangle outer = { 0, 0, (unsigned short)bw, (unsigned short)bh };
    XRectangle inner = { (short)border, (short)border,
                         (unsigned short)(bw - 2*border),
                         (unsigned short)(bh - 2*border) };
    XShapeCombineRectangles(dpy, border_win, ShapeBounding, 0, 0,
                            &outer, 1, ShapeSet,      YXBanded);
    XShapeCombineRectangles(dpy, border_win, ShapeBounding, 0, 0,
                            &inner, 1, ShapeSubtract, YXBanded);

    /* --- Buttons: stop (red) and cancel (dark gray), top-right above area --- */
    int btn_y  = y - border - BTN_SIZE - 4;
    int stop_x = x + w - BTN_SIZE;
    int cncl_x = stop_x - BTN_SIZE - BTN_GAP;

    attrs.event_mask = ButtonPressMask | ExposureMask;

    attrs.background_pixel = 0xCC2222;
    stop_win = XCreateWindow(dpy, root,
                             stop_x, btn_y, BTN_SIZE, BTN_SIZE, 0,
                             DefaultDepth(dpy, screen), InputOutput,
                             DefaultVisual(dpy, screen),
                             CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);
    make_circular(stop_win, BTN_SIZE);

    attrs.background_pixel = 0x555555;
    cancel_win = XCreateWindow(dpy, root,
                               cncl_x, btn_y, BTN_SIZE, BTN_SIZE, 0,
                               DefaultDepth(dpy, screen), InputOutput,
                               DefaultVisual(dpy, screen),
                               CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);
    make_circular(cancel_win, BTN_SIZE);

    /* Map everything; buttons appear only after countdown */
    XMapRaised(dpy, border_win);
    if (delay > 0) XMapRaised(dpy, overlay_win);
    else { XMapRaised(dpy, stop_win); XMapRaised(dpy, cancel_win); }
    XFlush(dpy);

    /* Xft for countdown number */
    XftFont *xft_font = XftFontOpenName(dpy, screen, "Sans:size=72:weight=bold");
    if (!xft_font) xft_font = XftFontOpenName(dpy, screen, "Sans:size=48");
    XftDraw *xft_draw = XftDrawCreate(dpy, overlay_win,
                                      DefaultVisual(dpy, screen),
                                      DefaultColormap(dpy, screen));
    XftColor xft_white;
    XftColorAllocName(dpy, DefaultVisual(dpy, screen),
                      DefaultColormap(dpy, screen), "white", &xft_white);

    /* GC for button icons (white) */
    GC icon_gc = XCreateGC(dpy, stop_win, 0, NULL);
    XSetForeground(dpy, icon_gc, WhitePixel(dpy, screen));

    signal(SIGTERM, cleanup);
    signal(SIGINT,  cleanup);

    int xfd = ConnectionNumber(dpy);
    XEvent ev;

    /* --- Countdown phase --- */
    int countdown = delay;
    while (countdown > 0) {
        draw_countdown(xft_draw, xft_font, &xft_white, countdown, w, h);

        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        fd_set fds; FD_ZERO(&fds); FD_SET(xfd, &fds);
        select(xfd + 1, &fds, NULL, NULL, &tv);

        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (ev.type == Expose && ev.xexpose.window == overlay_win)
                draw_countdown(xft_draw, xft_font, &xft_white, countdown, w, h);
        }
        countdown--;
    }

    /* --- Transition to recording phase --- */
    XUnmapWindow(dpy, overlay_win);
    XMapRaised(dpy, stop_win);
    XMapRaised(dpy, cancel_win);
    XFlush(dpy);

    /* --- Recording phase --- */
    while (1) {
        XNextEvent(dpy, &ev);
        if (ev.type == Expose) {
            if (ev.xexpose.window == stop_win)   draw_stop_icon(icon_gc);
            if (ev.xexpose.window == cancel_win) draw_cancel_icon(icon_gc);
        } else if (ev.type == ButtonPress) {
            if (ev.xbutton.window == stop_win && target_pid > 0)
                kill(target_pid, SIGUSR1);
            else if (ev.xbutton.window == cancel_win && target_pid > 0)
                kill(target_pid, SIGUSR2);
            cleanup(0);
        }
    }

    return 0;
}
