#define _POSIX_C_SOURCE 200809L

#include "window-clickthrough.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>

#include <stdint.h>
#include <stdlib.h>

Display *g_display;
int g_x_error;

int x_error_handler(Display *display, XErrorEvent *event)
{
    (void)display;
    (void)event;
    g_x_error = 1;
    return 0;
}

int window_exists(Window window)
{
    XWindowAttributes attributes;

    if (window == None) {
        return 0;
    }

    g_x_error = 0;
    Status result = XGetWindowAttributes(g_display, window, &attributes);
    XSync(g_display, False);
    return result != 0 && !g_x_error;
}

int set_token_property(Window window,
                       Atom token_atom,
                       const uint32_t token[TOKEN_WORDS])
{
    if (window == None) {
        return 0;
    }

    unsigned long property_data[TOKEN_WORDS];
    property_data[0] = token[0];
    property_data[1] = token[1];

    g_x_error = 0;
    XChangeProperty(g_display,
                    window,
                    token_atom,
                    XA_CARDINAL,
                    32,
                    PropModeReplace,
                    (unsigned char *)property_data,
                    TOKEN_WORDS);
    XSync(g_display, False);
    return g_x_error ? -1 : 0;
}

int window_has_token(Window window,
                     Atom token_atom,
                     const uint32_t token[TOKEN_WORDS])
{
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long item_count = 0;
    unsigned long bytes_after = 0;
    unsigned char *property_data = NULL;

    if (window == None) {
        return 0;
    }

    g_x_error = 0;
    int result = XGetWindowProperty(g_display,
                                    window,
                                    token_atom,
                                    0,
                                    TOKEN_WORDS,
                                    False,
                                    XA_CARDINAL,
                                    &actual_type,
                                    &actual_format,
                                    &item_count,
                                    &bytes_after,
                                    &property_data);
    XSync(g_display, False);

    int matches = 0;
    if (!g_x_error && result == Success && actual_type == XA_CARDINAL &&
        actual_format == 32 && item_count == TOKEN_WORDS && property_data != NULL) {
        unsigned long *values = (unsigned long *)property_data;
        matches = ((uint32_t)values[0] == token[0] && (uint32_t)values[1] == token[1]);
    }

    if (property_data != NULL) {
        XFree(property_data);
    }

    return matches;
}

void delete_token_property(Window window, Atom token_atom)
{
    if (window == None) {
        return;
    }

    g_x_error = 0;
    XDeleteProperty(g_display, window, token_atom);
    XSync(g_display, False);
}

int set_empty_input_shape(Window window)
{
    if (window == None) {
        return 0;
    }

    g_x_error = 0;
    XShapeCombineRectangles(g_display,
                            window,
                            ShapeInput,
                            0,
                            0,
                            NULL,
                            0,
                            ShapeSet,
                            Unsorted);
    XSync(g_display, False);
    return g_x_error ? -1 : 0;
}

void restore_default_input_shape(Window window)
{
    if (window == None) {
        return;
    }

    g_x_error = 0;
    XShapeCombineMask(g_display,
                      window,
                      ShapeInput,
                      0,
                      0,
                      None,
                      ShapeSet);
    XSync(g_display, False);
}

Window get_frame_window(Window client, Window root)
{
    Window returned_root = None;
    Window parent = None;
    Window *children = NULL;
    unsigned int child_count = 0;

    g_x_error = 0;
    Status result = XQueryTree(g_display,
                               client,
                               &returned_root,
                               &parent,
                               &children,
                               &child_count);
    XSync(g_display, False);

    if (children != NULL) {
        XFree(children);
    }

    if (g_x_error || result == 0 || parent == None || parent == root) {
        return None;
    }

    return parent;
}

static int has_wm_state(Window window, Atom wm_state_atom)
{
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long item_count = 0;
    unsigned long bytes_after = 0;
    unsigned char *property_data = NULL;

    g_x_error = 0;
    int result = XGetWindowProperty(g_display,
                                    window,
                                    wm_state_atom,
                                    0,
                                    0,
                                    False,
                                    AnyPropertyType,
                                    &actual_type,
                                    &actual_format,
                                    &item_count,
                                    &bytes_after,
                                    &property_data);
    XSync(g_display, False);

    if (property_data != NULL) {
        XFree(property_data);
    }

    return !g_x_error && result == Success && actual_type != None;
}

static Window find_client_descendant(Window window, Atom wm_state_atom)
{
    if (window == None) {
        return None;
    }

    if (has_wm_state(window, wm_state_atom)) {
        return window;
    }

    Window returned_root = None;
    Window returned_parent = None;
    Window *children = NULL;
    unsigned int child_count = 0;

    g_x_error = 0;
    Status result = XQueryTree(g_display,
                               window,
                               &returned_root,
                               &returned_parent,
                               &children,
                               &child_count);
    XSync(g_display, False);

    if (g_x_error || result == 0) {
        if (children != NULL) {
            XFree(children);
        }
        return window;
    }

    Window client = None;
    for (unsigned int index = 0; index < child_count && client == None; ++index) {
        client = find_client_descendant(children[index], wm_state_atom);
    }

    if (children != NULL) {
        XFree(children);
    }

    return client != None ? client : window;
}

Window select_window(Window root)
{
    Cursor cursor = XCreateFontCursor(g_display, XC_crosshair);
    int grab_status = XGrabPointer(g_display,
                                   root,
                                   False,
                                   ButtonPressMask,
                                   GrabModeAsync,
                                   GrabModeAsync,
                                   None,
                                   cursor,
                                   CurrentTime);

    if (grab_status != GrabSuccess) {
        XFreeCursor(g_display, cursor);
        fprintf(stderr, "%s: could not grab the pointer\n", PROGRAM_NAME);
        return None;
    }

    XEvent event;
    XWindowEvent(g_display, root, ButtonPressMask, &event);

    Window selected = None;
    if (event.xbutton.button == Button1) {
        selected = event.xbutton.subwindow;
    }

    XUngrabPointer(g_display, CurrentTime);
    XFreeCursor(g_display, cursor);
    XSync(g_display, False);

    if (event.xbutton.button != Button1) {
        return None;
    }

    if (selected == None || selected == root) {
        fprintf(stderr, "%s: no window selected\n", PROGRAM_NAME);
        return None;
    }

    Atom wm_state_atom = XInternAtom(g_display, "WM_STATE", False);
    return find_client_descendant(selected, wm_state_atom);
}
