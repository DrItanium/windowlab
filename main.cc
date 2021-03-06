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

XFontStruct *font = nullptr;
XftFont *xftfont = nullptr;
XftColor xft_detail;
GC string_gc, border_gc, text_gc, active_gc, depressed_gc, inactive_gc, menu_gc, selected_gc, empty_gc;
XColor border_col, text_col, active_col, depressed_col, inactive_col, menu_col, selected_col, empty_col;
Cursor resize_curs;
Atom wm_state, wm_change_state, wm_protos, wm_delete, wm_cmapwins;
std::string opt_font = DEF_FONT;
std::string opt_border = DEF_BORDER;
std::string opt_text = DEF_TEXT;
std::string opt_active = DEF_ACTIVE;
std::string opt_inactive = DEF_INACTIVE;
std::string opt_menu = DEF_MENU;
std::string opt_selected = DEF_SELECTED;
std::string opt_empty = DEF_EMPTY;
std::string opt_display;
Bool shape;
int shape_event = 0;

static void scanWindows(void);
static void setup_display(void);

int main(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
        std::string currArg(argv[i]);
#define X(name, variable)	 \
    if (currArg == name && (i + 1 < argc)) { \
		variable = argv[++i]; \
		continue; \
	}
		X("-font", opt_font)
		X("-border", opt_border)
		X("-text", opt_text)
		X("-active", opt_active)
		X("-inactive", opt_inactive)
		X("-menu", opt_menu)
		X("-selected", opt_selected)
		X("-empty", opt_empty)
		X("-display", opt_display)
#undef X
        if (currArg == "-about") {
            std::cout << "WindowLab17 " << VERSION << "(" << RELEASEDATE << ")" << std::endl;;
            std::cout << "WindowLab Original Code, Copyright (c) 2001-2009 Nick Gravgaard" << std::endl;
            std::cout << "WindowLab17, Copyright (c) 2020 Joshua Scoggins" << std::endl;
            std::cout << "WindowLab17 comes with ABSOLUTELY NO WARRANTY." << std::endl;
            std::cout << "This is free software, and you are welcome to redistribute it" << std::endl;
            std::cout << "under certain conditions; view the LICENCE file for details." << std::endl;
			exit(0);
        }
		// shouldn't get here; must be a bad option
		err("usage:\n  windowlab [options]\n\noptions are:\n  -font <font>\n  -border|-text|-active|-inactive|-menu|-selected|-empty <color>\n  -about\n  -display <display>");
		return 2;
	}
    struct sigaction act;
	act.sa_handler = signalHandler;
	act.sa_flags = 0;
	sigaction(SIGTERM, &act, nullptr);
	sigaction(SIGINT, &act, nullptr);
	sigaction(SIGHUP, &act, nullptr);
	sigaction(SIGCHLD, &act, nullptr);

	setup_display();
    Menu::instance().populate();
    // exploit the side effects
    Taskbar::instance().make();
	scanWindows();
	doEventLoop();
	return 0; // just another brick in the -Wall
}

static void
scanWindows() {
	unsigned int nwins = 0;
	Window dummyw1, dummyw2, *wins;
	XWindowAttributes attr;
    auto& dm = DisplayManager::instance();
    dm.queryTree(&dummyw1, &dummyw2, &wins, &nwins);
	for (unsigned int i = 0; i < nwins; i++) {
        dm.getWindowAttributes(wins[i], attr);
		if (!attr.override_redirect && attr.map_state == IsViewable) {
            Client::makeNew(wins[i]);
		}
	}
	XFree(wins);
}

DisplayManager& 
DisplayManager::instance() noexcept {
    static bool _mustInit = true;
    static DisplayManager _dsp;
    if (_mustInit) {
        _mustInit = false;
        _dsp._display = XOpenDisplay(opt_display.c_str());
        if (!_dsp.getDisplay()) {
            err("can't open display! check your DISPLAY variable.");
            exit(1);
        }
        _dsp._screen = DefaultScreen(_dsp._display);
        _dsp._root = RootWindow(_dsp._display, _dsp._screen);
    }
    return _dsp;
}



static void setup_display() {
	XSetWindowAttributes sattr;
	int dummy;
    // do the initial setup after this point
    auto& dm = DisplayManager::instance();

    dm.setErrorHandler(handleXError);
	wm_state = dm.internAtom("WM_STATE", False);
	wm_change_state = dm.internAtom("WM_CHANGE_STATE", False);
	wm_protos = dm.internAtom("WM_PROTOCOLS", False);
	wm_delete = dm.internAtom("WM_DELETE_WINDOW", False);
	wm_cmapwins = dm.internAtom("WM_COLORMAP_WINDOWS", False);
	dm.allocNamedColorFromDefaultColormap(opt_border, border_col);
	dm.allocNamedColorFromDefaultColormap(opt_text, text_col);
	dm.allocNamedColorFromDefaultColormap(opt_active, active_col);
	dm.allocNamedColorFromDefaultColormap(opt_inactive, inactive_col);
	dm.allocNamedColorFromDefaultColormap(opt_menu, menu_col);
	dm.allocNamedColorFromDefaultColormap(opt_selected, selected_col);
	dm.allocNamedColorFromDefaultColormap(opt_empty, empty_col);

	depressed_col.pixel = active_col.pixel;
	depressed_col.red = active_col.red - ACTIVE_SHADOW;
	depressed_col.green = active_col.green - ACTIVE_SHADOW;
	depressed_col.blue = active_col.blue - ACTIVE_SHADOW;
	depressed_col.red = depressed_col.red <= (USHRT_MAX - ACTIVE_SHADOW) ? depressed_col.red : 0;
	depressed_col.green = depressed_col.green <= (USHRT_MAX - ACTIVE_SHADOW) ? depressed_col.green : 0;
	depressed_col.blue = depressed_col.blue <= (USHRT_MAX - ACTIVE_SHADOW) ? depressed_col.blue : 0;
    dm.allocColorFromDefaultColormap(depressed_col);

	xft_detail.color.red = text_col.red;
	xft_detail.color.green = text_col.green;
	xft_detail.color.blue = text_col.blue;
	xft_detail.color.alpha = 0xffff;
	xft_detail.pixel = text_col.pixel;

	xftfont = XftFontOpenXlfd(dm.getDisplay(), dm.getDefaultScreen(), opt_font.c_str());
	if (!xftfont) {
        err("font '", opt_font, "' not found");
		exit(1);
	}

	shape = XShapeQueryExtension(dm.getDisplay(), &shape_event, &dummy);

	resize_curs = XCreateFontCursor(dm.getDisplay(), XC_fleur);

	/* find out which modifier is NumLock - we'll use this when grabbing every combination of modifiers we can think of */
    auto modmap = dm.getModifierMapping();
	for (auto i = 0; i < 8; i++) {
		for (auto j = 0; j < modmap->max_keypermod; j++) {
			if (modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dm.getDisplay(), XK_Num_Lock)) {
                dm.setNumLockMask((1 << i));
                if constexpr (debugActive()) {
                    std::cerr << "setup_display() : XK_Num_lock is (1<<0x" << i << ")" << std::endl;
                }
			}
		}
	}
	XFree(modmap);

	XGCValues gv;
	gv.function = GXcopy;

	gv.foreground = border_col.pixel;
	gv.line_width = DEF_BORDERWIDTH;
	border_gc = dm.createGCForRoot(GCFunction|GCForeground|GCLineWidth, gv);

	gv.foreground = text_col.pixel;
	gv.line_width = 1;

	text_gc = dm.createGCForRoot(GCFunction|GCForeground, gv);

	gv.foreground = active_col.pixel;
	active_gc = dm.createGCForRoot(GCFunction|GCForeground, gv);

	gv.foreground = depressed_col.pixel;
	depressed_gc = dm.createGCForRoot(GCFunction|GCForeground, gv);

	gv.foreground = inactive_col.pixel;
	inactive_gc = dm.createGCForRoot(GCFunction|GCForeground, gv);

	gv.foreground = menu_col.pixel;
	menu_gc = dm.createGCForRoot(GCFunction|GCForeground, gv);

	gv.foreground = selected_col.pixel;
	selected_gc = dm.createGCForRoot(GCFunction|GCForeground, gv);

	gv.foreground = empty_col.pixel;
	empty_gc = dm.createGCForRoot(GCFunction|GCForeground, gv);

	sattr.event_mask = ChildMask|ColormapChangeMask|ButtonMask;
	XChangeWindowAttributes(dm.getDisplay(), dm.getRoot(), CWEventMask, &sattr);

    dm.grabKeysym(MODIFIER, KEY_CYCLEPREV);
	dm.grabKeysym(MODIFIER, KEY_CYCLENEXT);
	dm.grabKeysym(MODIFIER, KEY_FULLSCREEN);
	dm.grabKeysym(MODIFIER, KEY_TOGGLEZ);
}
int getBarHeight() noexcept {
    return (xftfont->ascent + xftfont->descent + 2 * SPACE + 2);
}
