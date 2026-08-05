#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "could not open X display\n");
        return 1;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window window = XCreateSimpleWindow(display,
                                        root,
                                        40,
                                        40,
                                        320,
                                        180,
                                        0,
                                        BlackPixel(display, screen),
                                        WhitePixel(display, screen));

    XStoreName(display, window, "Window Clickthrough Test");

    XClassHint class_hint = {
        .res_name = "window-clickthrough-test",
        .res_class = "WindowClickthroughTest",
    };
    XSetClassHint(display, window, &class_hint);

    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete, 1);

    XSelectInput(display, window, ExposureMask | StructureNotifyMask);
    XMapWindow(display, window);
    XFlush(display);

    printf("0x%lx\n", (unsigned long)window);
    fflush(stdout);

    for (;;) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == wm_delete) {
            break;
        }
        if (event.type == DestroyNotify) {
            break;
        }
    }

    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
