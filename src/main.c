/*
 * Copyright 2012 Daniil Ivanov <daniil.ivanov@gmail.com>
 *
 * This file is part of XStat.
 *
 * XStat is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * XStat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with XStat. If not, see http://www.gnu.org/licenses/.
 */

#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

static Atom active_window_prop = None;
static Window active_window = None;
static Time activation_time = CurrentTime;

void explain_property_failure(Display *display, const Atom property)
{
    char *prop_name = XGetAtomName(display, property);
    fprintf(stderr, "couldn't get property %s\n", prop_name);
    XFree(prop_name);
}

void get_window_class_name(Display *display, const Window window,
    char **win_class, char **win_name)
{
    XClassHint class_hint;
    Status result = XGetClassHint(display, window, &class_hint);
    if (result != 0) {
        if (class_hint.res_class && win_class)
            *win_class = class_hint.res_class;
        if (class_hint.res_name&& win_name)
            *win_name = class_hint.res_name;
    } else
        fprintf(stderr, "couldn't get property WM_CLASS %d\n", result);
}

Time get_time(Display *display, const Window window)
{
    Time time = CurrentTime;
    Atom property = XInternAtom(display, "_NET_WM_USER_TIME", False);
    Atom type = XA_CARDINAL;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = 0;
    int result = XGetWindowProperty(display, window, property,
        0, 65536, False, type, &actual_type, &actual_format, &nitems,
        &bytes_after, &data);
    if (result == Success) {
        if (data) {
            time = *(Time*)data;
            XFree(data);
        }
    } else
        explain_property_failure(display, property);

    return time;
}

int get_window_pid(Display *display, const Window window)
{
    int pid = 0;
    Atom property = XInternAtom(display, "_NET_WM_PID", False);
    Atom type = XA_CARDINAL;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = 0;
    int result = XGetWindowProperty(display, window, property,
        0, 65536, False, type, &actual_type, &actual_format, &nitems,
        &bytes_after, &data);
    if (result == Success) {
        if (data) {
            pid = *(int*)data;
            XFree(data);
        }
    } else
        explain_property_failure(display, property);

    return pid;
}

void show_window_info(Display *display, const Window window)
{
    char *win_name = NULL, *win_class = NULL;
    get_window_class_name(display, window, &win_class, &win_name);
    int pid = get_window_pid(display, window);
    fprintf(stdout, "\"%s\", \"%s\", %d\n", win_class, win_name, pid);
    if (win_name)
        XFree(win_name);
    if (win_class)
        XFree(win_class);
}

Window get_active_window(Display *display, const Window root_win)
{
    Atom type = XA_WINDOW;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data;
    int result = XGetWindowProperty(display, root_win, active_window_prop,
        0, 65536, False, type, &actual_type, &actual_format, &nitems,
        &bytes_after, &data);
    if (result == Success) {
        if (data) {
            Window window = *(Window*)data;
            XFree(data);
            if (window != None) {
                return window;
            }
        }
    } else
        explain_property_failure(display, active_window_prop);

    return None;
}

void handle_event(Display *display)
{
    if (XPending(display) > 0) {
        XEvent event;

        /* Get current event of X display */
        int result = XNextEvent(display, &event);
        if (result == Success && event.type == PropertyNotify) {
            if (event.xproperty.atom == active_window_prop) {
                Window win = get_active_window(event.xproperty.display,
                    event.xproperty.window);
                if (win != None && win != active_window) {
                    show_window_info(display, win);
                    fprintf(stderr, "Duration %li\n",
                        event.xproperty.time - activation_time);
                    active_window = win;
                    activation_time = event.xproperty.time;
                }
            }
        } else {
            fprintf(stderr, "Couldn't fetch event, error code %d\n", result);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc > 1)
        (void)(argv[1]);

    char *display_name = NULL;
    Display *display = NULL;

    /* Connect to X server */
    display = XOpenDisplay(display_name);
    if (display == NULL) {
        fprintf(stderr, "Couldn't connect to X server %s\n", display_name);
        exit(EXIT_FAILURE);
    }

    active_window_prop = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);

    /* Get root windows for every screen */
    int screen_count = XScreenCount(display);
    Window *root_wins = calloc(screen_count, sizeof(Window*));
    for (int i = 0; i < screen_count; i++) {
        root_wins[i] = XRootWindow(display, i);

        active_window = get_active_window(display, root_wins[i]);
        activation_time = get_time(display, active_window);

        int result = XSelectInput(display, root_wins[i], PropertyChangeMask);
        if (result == 0)
            fprintf(stderr, "Failed to attach handler to window #%i\n", i);
    }
    free(root_wins);

    while (True)
        handle_event(display);

    /* Disconnect from X server */
    XCloseDisplay(display);

    exit(0);
}
