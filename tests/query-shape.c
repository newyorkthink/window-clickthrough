#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s WINDOW_ID\n", argv[0]);
        return 2;
    }

    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(argv[1], &end, 0);
    if (errno != 0 || end == argv[1] || *end != '\0') {
        fprintf(stderr, "invalid window id\n");
        return 2;
    }

    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "could not open X display\n");
        return 1;
    }

    int count = 0;
    int ordering = 0;
    XRectangle *rectangles = XShapeGetRectangles(display,
                                                  (Window)parsed,
                                                  ShapeInput,
                                                  &count,
                                                  &ordering);
    if (rectangles != NULL) {
        XFree(rectangles);
    }

    printf("%d\n", count);
    XCloseDisplay(display);
    return 0;
}
