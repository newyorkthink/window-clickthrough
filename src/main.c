#define _POSIX_C_SOURCE 200809L

#include "window-clickthrough.h"

#include <X11/extensions/shape.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

static void print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage: %s [--restore|--status|--help|--version]\n"
            "\n"
            "Without arguments:\n"
            "  If no window is active, select one and enable mouse click-through.\n"
            "  If a window is active, restore its normal mouse input.\n"
            "\n"
            "Options:\n"
            "  --restore  Restore the currently active window, if any.\n"
            "  --status   Print active/inactive state.\n"
            "  --help     Show this help.\n"
            "  --version  Show the program version.\n",
            PROGRAM_NAME);
}

static int state_is_active(const struct state_record *state, Atom token_atom)
{
    return state->client != None &&
           window_exists(state->client) &&
           window_has_token(state->client, token_atom, state->token);
}

static int restore_active_window(const char *state_path,
                                 const struct state_record *state,
                                 Atom token_atom,
                                 int quiet_when_missing)
{
    int client_matches = window_has_token(state->client, token_atom, state->token);
    int frame_matches = window_has_token(state->frame, token_atom, state->token);

    if (client_matches) {
        restore_default_input_shape(state->client);
        delete_token_property(state->client, token_atom);
    }

    if (frame_matches) {
        restore_default_input_shape(state->frame);
        delete_token_property(state->frame, token_atom);
    }

    if (unlink(state_path) != 0 && errno != ENOENT) {
        perror("unlink state file");
        return -1;
    }

    if (client_matches || frame_matches) {
        printf("Mouse click-through disabled for window 0x%lx\n",
               (unsigned long)state->client);
    } else if (!quiet_when_missing) {
        printf("No active window; stale state removed\n");
    }

    return 0;
}

static int enable_clickthrough(const char *state_path,
                               Window root,
                               Atom token_atom)
{
    Window client = select_window(root);
    if (client == None) {
        return 0;
    }

    Window frame = get_frame_window(client, root);
    struct state_record state = {
        .version = STATE_VERSION,
        .client = client,
        .frame = frame,
        .token = {0, 0},
    };

    if (generate_token(state.token) != 0) {
        fprintf(stderr, "%s: could not generate a state token\n", PROGRAM_NAME);
        return -1;
    }

    if (set_token_property(client, token_atom, state.token) != 0) {
        fprintf(stderr, "%s: could not mark the selected window\n", PROGRAM_NAME);
        return -1;
    }

    if (frame != None && set_token_property(frame, token_atom, state.token) != 0) {
        delete_token_property(client, token_atom);
        fprintf(stderr, "%s: could not mark the window frame\n", PROGRAM_NAME);
        return -1;
    }

    if (set_empty_input_shape(client) != 0 ||
        (frame != None && set_empty_input_shape(frame) != 0)) {
        restore_default_input_shape(client);
        restore_default_input_shape(frame);
        delete_token_property(client, token_atom);
        delete_token_property(frame, token_atom);
        fprintf(stderr, "%s: could not enable mouse click-through\n", PROGRAM_NAME);
        return -1;
    }

    if (write_state_file(state_path, &state) != 0) {
        restore_default_input_shape(client);
        restore_default_input_shape(frame);
        delete_token_property(client, token_atom);
        delete_token_property(frame, token_atom);
        return -1;
    }

    printf("Mouse click-through enabled for window 0x%lx\n", (unsigned long)client);
    return 0;
}

int main(int argc, char **argv)
{
    enum action {
        ACTION_TOGGLE,
        ACTION_RESTORE,
        ACTION_STATUS,
    } requested_action = ACTION_TOGGLE;

    if (argc == 2) {
        if (strcmp(argv[1], "--restore") == 0) {
            requested_action = ACTION_RESTORE;
        } else if (strcmp(argv[1], "--status") == 0) {
            requested_action = ACTION_STATUS;
        } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_usage(stdout);
            return 0;
        } else if (strcmp(argv[1], "--version") == 0) {
            printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
            return 0;
        } else {
            print_usage(stderr);
            return 2;
        }
    } else if (argc > 2) {
        print_usage(stderr);
        return 2;
    }

    g_display = XOpenDisplay(NULL);
    if (g_display == NULL) {
        fprintf(stderr, "%s: could not connect to the X11 display\n", PROGRAM_NAME);
        return 1;
    }

    XSetErrorHandler(x_error_handler);

    int shape_event_base = 0;
    int shape_error_base = 0;
    int shape_major = 0;
    int shape_minor = 0;

    if (!XShapeQueryExtension(g_display, &shape_event_base, &shape_error_base) ||
        !XShapeQueryVersion(g_display, &shape_major, &shape_minor) ||
        shape_major < 1 || (shape_major == 1 && shape_minor < 1)) {
        fprintf(stderr,
                "%s: the X11 SHAPE extension with input regions is unavailable\n",
                PROGRAM_NAME);
        XCloseDisplay(g_display);
        return 1;
    }

    char state_path[PATH_MAX];
    char lock_path[PATH_MAX];
    if (build_runtime_paths(state_path,
                            sizeof(state_path),
                            lock_path,
                            sizeof(lock_path),
                            DisplayString(g_display)) != 0) {
        XCloseDisplay(g_display);
        return 1;
    }

    int lock_descriptor = lock_instance(lock_path);
    if (lock_descriptor < 0) {
        XCloseDisplay(g_display);
        return 1;
    }

    Atom token_atom = XInternAtom(g_display, "_WINDOW_CLICKTHROUGH_TOKEN", False);
    Window root = DefaultRootWindow(g_display);
    struct state_record state;
    int state_result = read_state_file(state_path, &state);

    if (state_result < 0) {
        fprintf(stderr, "%s: invalid state file removed\n", PROGRAM_NAME);
        if (unlink(state_path) != 0 && errno != ENOENT) {
            perror("unlink state file");
            close(lock_descriptor);
            XCloseDisplay(g_display);
            return 1;
        }
        state_result = 0;
    }

    int active = state_result == 1 && state_is_active(&state, token_atom);
    int exit_status = 0;

    if (requested_action == ACTION_STATUS) {
        if (active) {
            printf("active 0x%lx\n", (unsigned long)state.client);
            exit_status = 0;
        } else {
            if (state_result == 1) {
                unlink(state_path);
            }
            printf("inactive\n");
            exit_status = 1;
        }
    } else if (requested_action == ACTION_RESTORE) {
        if (state_result == 1) {
            exit_status = restore_active_window(state_path, &state, token_atom, 0) == 0 ? 0 : 1;
        } else {
            printf("No active window\n");
        }
    } else if (active) {
        exit_status = restore_active_window(state_path, &state, token_atom, 0) == 0 ? 0 : 1;
    } else {
        if (state_result == 1) {
            restore_active_window(state_path, &state, token_atom, 1);
        }
        exit_status = enable_clickthrough(state_path, root, token_atom) == 0 ? 0 : 1;
    }

    close(lock_descriptor);
    XCloseDisplay(g_display);
    return exit_status;
}
