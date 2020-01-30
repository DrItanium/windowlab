/* WindowLab17 - An X11 window manager based off of windowlab but rewritten in C++17
 * Based off of "WindowLab - an X11 window manager by Nick Gravgaard"
 *
 * WindowLab17 Copyright (c) 2020 Joshua Scoggins
 * WindowLab Copyright (c) 2001-2010 Nick Gravgaard
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <signal.h>
#include <X11/cursorfont.h>
#include "windowlab.h"

Display *dsply = nullptr;
Window root;
int screen;
XFontStruct *font = nullptr;
XftFont *xftfont = nullptr;
XftColor xft_detail;
GC string_gc, border_gc, text_gc, active_gc, depressed_gc, inactive_gc, menu_gc, selected_gc, empty_gc;
XColor border_col, text_col, active_col, depressed_col, inactive_col, menu_col, selected_col, empty_col;
Cursor resize_curs;
Atom wm_state, wm_change_state, wm_protos, wm_delete, wm_cmapwins;
ClientPointer focused_client, topmost_client, fullscreen_client;
bool in_taskbar = false; // actually, we don't know yet
bool showing_taskbar = true;
unsigned int focus_count = 0;
Rect fs_prevdims;
std::string opt_font = DEF_FONT;
std::string opt_border = DEF_BORDER;
std::string opt_text = DEF_TEXT;
std::string opt_active = DEF_ACTIVE;
std::string opt_inactive = DEF_INACTIVE;
std::string opt_menu = DEF_MENU;
std::string opt_selected = DEF_SELECTED;
std::string opt_empty = DEF_EMPTY;
std::string opt_display;
#ifdef SHAPE
Bool shape;
int shape_event = 0;
#endif
unsigned int numlockmask = 0;

static void scan_wins(void);
static void setup_display(void);

int main(int argc, char **argv)
{
	int i;
	struct sigaction act;

#define OPT_STR(name, variable)	 \
	if (strcmp(argv[i], name) == 0 && i + 1 < argc) \
	{ \
		variable = argv[++i]; \
		continue; \
	}

	for (i = 1; i < argc; i++)
	{
		OPT_STR("-font", opt_font)
		OPT_STR("-border", opt_border)
		OPT_STR("-text", opt_text)
		OPT_STR("-active", opt_active)
		OPT_STR("-inactive", opt_inactive)
		OPT_STR("-menu", opt_menu)
		OPT_STR("-selected", opt_selected)
		OPT_STR("-empty", opt_empty)
		OPT_STR("-display", opt_display)
		if (strcmp(argv[i], "-about") == 0)
		{
			printf("WindowLab " VERSION " (" RELEASEDATE "), Copyright (c) 2001-2009 Nick Gravgaard\nWindowLab comes with ABSOLUTELY NO WARRANTY.\nThis is free software, and you are welcome to redistribute it\nunder certain conditions; view the LICENCE file for details.\n");
			exit(0);
		}
		// shouldn't get here; must be a bad option
		err("usage:\n  windowlab [options]\n\noptions are:\n  -font <font>\n  -border|-text|-active|-inactive|-menu|-selected|-empty <color>\n  -about\n  -display <display>");
		return 2;
	}

	act.sa_handler = sig_handler;
	act.sa_flags = 0;
	sigaction(SIGTERM, &act, nullptr);
	sigaction(SIGINT, &act, nullptr);
	sigaction(SIGHUP, &act, nullptr);
	sigaction(SIGCHLD, &act, nullptr);

	setup_display();
    Menu::instance().populate();
    // exploit the side effects
    Taskbar::instance().make();
	scan_wins();
	doEventLoop();
	return 1; // just another brick in the -Wall
}

static void scan_wins(void)
{
	unsigned int nwins, i;
	Window dummyw1, dummyw2, *wins;
	XWindowAttributes attr;

	XQueryTree(dsply, root, &dummyw1, &dummyw2, &wins, &nwins);
	for (i = 0; i < nwins; i++)
	{
		XGetWindowAttributes(dsply, wins[i], &attr);
		if (!attr.override_redirect && attr.map_state == IsViewable)
		{
			makeNewClient(wins[i]);
		}
	}
	XFree(wins);
}

static void setup_display(void)
{
	XColor dummyc;
	XGCValues gv;
	XSetWindowAttributes sattr;
	XModifierKeymap *modmap;
	int i, j;
#ifdef SHAPE
	int dummy;
#endif

	dsply = XOpenDisplay(opt_display.c_str());

	if (!dsply)
	{
		err("can't open display! check your DISPLAY variable.");
		exit(1);
	}

	XSetErrorHandler(handle_xerror);
	screen = DefaultScreen(dsply);
	root = RootWindow(dsply, screen);

	wm_state = XInternAtom(dsply, "WM_STATE", False);
	wm_change_state = XInternAtom(dsply, "WM_CHANGE_STATE", False);
	wm_protos = XInternAtom(dsply, "WM_PROTOCOLS", False);
	wm_delete = XInternAtom(dsply, "WM_DELETE_WINDOW", False);
	wm_cmapwins = XInternAtom(dsply, "WM_COLORMAP_WINDOWS", False);
	XAllocNamedColor(dsply, DefaultColormap(dsply, screen), opt_border.c_str(), &border_col, &dummyc);
	XAllocNamedColor(dsply, DefaultColormap(dsply, screen), opt_text.c_str(), &text_col, &dummyc);
	XAllocNamedColor(dsply, DefaultColormap(dsply, screen), opt_active.c_str(), &active_col, &dummyc);
	XAllocNamedColor(dsply, DefaultColormap(dsply, screen), opt_inactive.c_str(), &inactive_col, &dummyc);
	XAllocNamedColor(dsply, DefaultColormap(dsply, screen), opt_menu.c_str(), &menu_col, &dummyc);
	XAllocNamedColor(dsply, DefaultColormap(dsply, screen), opt_selected.c_str(), &selected_col, &dummyc);
	XAllocNamedColor(dsply, DefaultColormap(dsply, screen), opt_empty.c_str(), &empty_col, &dummyc);

	depressed_col.pixel = active_col.pixel;
	depressed_col.red = active_col.red - ACTIVE_SHADOW;
	depressed_col.green = active_col.green - ACTIVE_SHADOW;
	depressed_col.blue = active_col.blue - ACTIVE_SHADOW;
	depressed_col.red = depressed_col.red <= (USHRT_MAX - ACTIVE_SHADOW) ? depressed_col.red : 0;
	depressed_col.green = depressed_col.green <= (USHRT_MAX - ACTIVE_SHADOW) ? depressed_col.green : 0;
	depressed_col.blue = depressed_col.blue <= (USHRT_MAX - ACTIVE_SHADOW) ? depressed_col.blue : 0;
	XAllocColor(dsply, DefaultColormap(dsply, screen), &depressed_col);

	xft_detail.color.red = text_col.red;
	xft_detail.color.green = text_col.green;
	xft_detail.color.blue = text_col.blue;
	xft_detail.color.alpha = 0xffff;
	xft_detail.pixel = text_col.pixel;

	xftfont = XftFontOpenXlfd(dsply, DefaultScreen(dsply), opt_font.c_str());
	if (!xftfont)
	{
		err("font '%s' not found", opt_font);
		exit(1);
	}

#ifdef SHAPE
	shape = XShapeQueryExtension(dsply, &shape_event, &dummy);
#endif

	resize_curs = XCreateFontCursor(dsply, XC_fleur);

	/* find out which modifier is NumLock - we'll use this when grabbing every combination of modifiers we can think of */
	modmap = XGetModifierMapping(dsply);
	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < modmap->max_keypermod; j++)
		{
			if (modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dsply, XK_Num_Lock))
			{
				numlockmask = (1 << i);
#ifdef DEBUG
				fprintf(stderr, "setup_display() : XK_Num_Lock is (1<<0x%02x)\n", i);
#endif
			}
		}
	}
	XFree(modmap);

	gv.function = GXcopy;

	gv.foreground = border_col.pixel;
	gv.line_width = DEF_BORDERWIDTH;
	border_gc = XCreateGC(dsply, root, GCFunction|GCForeground|GCLineWidth, &gv);

	gv.foreground = text_col.pixel;
	gv.line_width = 1;

	text_gc = XCreateGC(dsply, root, GCFunction|GCForeground, &gv);

	gv.foreground = active_col.pixel;
	active_gc = XCreateGC(dsply, root, GCFunction|GCForeground, &gv);

	gv.foreground = depressed_col.pixel;
	depressed_gc = XCreateGC(dsply, root, GCFunction|GCForeground, &gv);

	gv.foreground = inactive_col.pixel;
	inactive_gc = XCreateGC(dsply, root, GCFunction|GCForeground, &gv);

	gv.foreground = menu_col.pixel;
	menu_gc = XCreateGC(dsply, root, GCFunction|GCForeground, &gv);

	gv.foreground = selected_col.pixel;
	selected_gc = XCreateGC(dsply, root, GCFunction|GCForeground, &gv);

	gv.foreground = empty_col.pixel;
	empty_gc = XCreateGC(dsply, root, GCFunction|GCForeground, &gv);

	sattr.event_mask = ChildMask|ColormapChangeMask|ButtonMask;
	XChangeWindowAttributes(dsply, root, CWEventMask, &sattr);

	grab_keysym(root, MODIFIER, KEY_CYCLEPREV);
	grab_keysym(root, MODIFIER, KEY_CYCLENEXT);
	grab_keysym(root, MODIFIER, KEY_FULLSCREEN);
	grab_keysym(root, MODIFIER, KEY_TOGGLEZ);
}
int BARHEIGHT() noexcept {
    return (xftfont->ascent + xftfont->descent + 2 * SPACE + 2);
}
