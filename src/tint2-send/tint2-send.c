#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Display *display = 0;

/* From wmctrl */
char *get_property(Window window, Atom xa_prop_type, const char *prop_name) {
    Atom xa_prop_name = XInternAtom(display, prop_name, False);

    Atom xa_ret_type;
    int ret_format;
    unsigned long ret_nitems;
    unsigned long ret_bytes_after;
    unsigned long tmp_size;
    unsigned char *ret_prop;
    if (XGetWindowProperty(display, window, xa_prop_name, 0, 1024,
                           False, xa_prop_type, &xa_ret_type, &ret_format,
                           &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
        return NULL;
    }

    if (xa_ret_type != xa_prop_type) {
        XFree(ret_prop);
        return NULL;
    }

    /* Correct 64 Architecture implementation of 32 bit data */
    tmp_size = (ret_format / 8) * ret_nitems;
    if (ret_format == 32)
        tmp_size *= sizeof(long) / 4;

    char *ret = (char *)calloc(1, tmp_size + 1);
    memcpy(ret, ret_prop, tmp_size);

    XFree(ret_prop);
    return ret;
}

int is_tint2(Window window)
{
    XWindowAttributes attr = {};
    if (!XGetWindowAttributes(display, window, &attr))
        return 0;
    // if (attr.map_state != IsViewable)
    //     return 0;

    char *wm_class = get_property(window, XA_STRING, "WM_NAME");
    if (!wm_class) {
        return 0;
    }
    int class_match = 0;
    if (strcmp(wm_class, "tint2") == 0) {
        class_match = 1;
    }
    free(wm_class);
    return class_match;
}

void handle_tint2_window(Window window, char *action, char **args)
{
    if (!is_tint2(window))
        return;
    if (strcmp(action, "show") == 0) {
        fprintf(stderr, "Showing tint2 window: %lx\n", window);
        XEvent event = {};
        event.xcrossing.type = EnterNotify;
        event.xcrossing.window = window;
        event.xcrossing.mode = NotifyNormal;
        event.xcrossing.detail = NotifyVirtual;
        event.xcrossing.same_screen = True;
        XSendEvent(display, window, False, 0, &event);
        XFlush(display);
    } else if (strcmp(action, "hide") == 0) {
        fprintf(stderr, "Hiding tint2 window: %lx\n", window);
        XEvent event = {};
        event.xcrossing.type = LeaveNotify;
        event.xcrossing.window = window;
        event.xcrossing.mode = NotifyNormal;
        event.xcrossing.detail = NotifyVirtual;
        event.xcrossing.same_screen = True;
        XSendEvent(display, window, False, 0, &event);
        XFlush(display);
    } else if (strcmp(action, "refresh-execp") == 0) {
        XEvent event = {};
        char *name = args[0];
        if (!name) {
            fprintf(stderr, "Error: missing execp name\n");
            return;
        }
        if (!name[0]) {
            fprintf(stderr, "Error: empty execp name\n");
            return;
        }
        if (strlen(name) > sizeof(event.xclient.data.b)) {
            fprintf(stderr, "Error: execp name bigger than %ld bytes\n", sizeof(event.xclient.data.b));
            return;
        }
        fprintf(stderr, "Refreshing execp '%s' for window: %lx\n", name, window);
        event.xclient.type = ClientMessage;
        event.xclient.window = window;
        event.xclient.send_event = True;
        event.xclient.message_type = XInternAtom(display, "_TINT2_REFRESH_EXECP", False);
        event.xclient.format = 8;
        strncpy(event.xclient.data.b, name, sizeof(event.xclient.data.b));
        XSendEvent(display, window, False, 0, &event);
        XFlush(display);
    } else {
        fprintf(stderr, "Error: unknown action %s\n", action);
    }
}

typedef void window_callback_t(Window window, char *action, char **args);

void walk_windows(Window node, window_callback_t *callback, char *action, char **args)
{
    callback(node, action, args);
    Window root = 0;
    Window parent = 0;
    Window *children = 0;
    unsigned int nchildren = 0;
    if (!XQueryTree(display, node,
                    &root, &parent, &children, &nchildren)) {
        return;
    }
    for (unsigned int i = 0; i < nchildren; i++) {
        walk_windows(children[i], callback, action, args);
    }
}

int main(int argc, char **argv)
{
    display = XOpenDisplay(NULL);
    if (!display ) {
        fprintf(stderr, "Failed to open X11 connection\n");
        exit(1);
    }

    argc--, argv++;
    if (!argc) {
        fprintf(stderr, "Usage: tint2-send [show|hide|refresh-execp]\n");
        exit(1);
    }
    char *action = argv[0];
    char **args = argv + 1;
    walk_windows(DefaultRootWindow(display), handle_tint2_window, action, args);

    return 0;
}
