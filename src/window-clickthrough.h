#ifndef WINDOW_CLICKTHROUGH_H
#define WINDOW_CLICKTHROUGH_H

#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define PROGRAM_NAME "window-clickthrough"
#define PROGRAM_VERSION "1.0.0"
#define STATE_VERSION 1
#define TOKEN_WORDS 2

struct state_record {
    int version;
    Window client;
    Window frame;
    uint32_t token[TOKEN_WORDS];
};

extern Display *g_display;
extern int g_x_error;

int x_error_handler(Display *display, XErrorEvent *event);

int build_runtime_paths(char *state_path,
                        size_t state_path_size,
                        char *lock_path,
                        size_t lock_path_size,
                        const char *display_name);
int lock_instance(const char *lock_path);
int generate_token(uint32_t token[TOKEN_WORDS]);
int write_state_file(const char *state_path, const struct state_record *state);
int read_state_file(const char *state_path, struct state_record *state);

int window_exists(Window window);
int set_token_property(Window window,
                       Atom token_atom,
                       const uint32_t token[TOKEN_WORDS]);
int window_has_token(Window window,
                     Atom token_atom,
                     const uint32_t token[TOKEN_WORDS]);
void delete_token_property(Window window, Atom token_atom);
int set_empty_input_shape(Window window);
void restore_default_input_shape(Window window);
Window get_frame_window(Window client, Window root);
Window select_window(Window root);

#endif
