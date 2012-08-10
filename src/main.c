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

void explain_property_failure(Display *display, Atom property)
{
    char *prop_name = XGetAtomName(display, property);
    fprintf(stderr, "couldn't get property %s\n", prop_name);
    XFree(prop_name);
}

void get_window_class_name(Display *display, Window win, char **win_class,
    char **win_name)
{
    XClassHint class_hint;
    Status result = XGetClassHint(display, win, &class_hint);
    if (result != 0) {
        if (class_hint.res_class && win_class)
            *win_class = class_hint.res_class;
        if (class_hint.res_name&& win_name)
            *win_name = class_hint.res_name;
    } else
        fprintf(stderr, "couldn't get property WM_CLASS %d\n", result);
}

int get_window_pid(Display *display, Window win)
{
    int pid = 0;
    Atom property = XInternAtom(display, "_NET_WM_PID", False);
    Atom type = XA_CARDINAL;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = 0;
    int result = XGetWindowProperty(display, win, property,
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

void get_list_stacking(Display *display, Window root_win)
{
    Atom property = XInternAtom(display, "_NET_CLIENT_LIST_STACKING", False);
    Atom type = XA_WINDOW;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    Window *list_stacking = 0;
    int result = XGetWindowProperty(display, root_win, property,
        0, 65536, False, type, &actual_type, &actual_format, &nitems,
        &bytes_after, (unsigned char**)(&list_stacking));
    if (result == Success) {
        if (list_stacking) {
            for (unsigned int i = 0; i < nitems; i++) {
                char *win_name = NULL, *win_class = NULL;
                get_window_class_name(display, list_stacking[i], &win_class,
                    &win_name);
                int pid = get_window_pid(display, list_stacking[i]);
                fprintf(stdout, "\"%s\", \"%s\", %d\n", win_class, win_name, pid);
                if (win_name)
                    XFree(win_name);
                if (win_class)
                    XFree(win_class);
            }
            XFree(list_stacking);
        }
    } else
        explain_property_failure(display, property);
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
        fprintf(stderr, "couldn't connect to X server %s\n", display_name);
        exit(EXIT_FAILURE);
    }

    int screen_count = XScreenCount(display);
    Window *root_wins = calloc(screen_count, sizeof(Window*));
    for (int i = 0; i < screen_count; i++) {
        root_wins[i] = XRootWindow(display, i);

        get_list_stacking(display, root_wins[i]);
    }

    free(root_wins);

    /* Disconnect from X server */
    XCloseDisplay(display);

    exit(0);
}
