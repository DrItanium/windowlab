/* WindowLab17 - An X11 window manager based off of windowlab but rewritten in C++17
 * Based off of "WindowLab - an X11 window manager by Nick Gravgaard"
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

#ifndef WINDOWLAB_H
#define WINDOWLAB_H

#define VERSION "1.40"
#define RELEASEDATE "2010-04-04"

#include <cerrno>
#include <climits>
#include <pwd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <functional>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#ifdef XFT
#include <X11/Xft/Xft.h>
#endif


#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
constexpr auto MaximumPathLength = PATH_MAX;

// here are the default settings - change to suit your taste

// if you aren't sure about DEF_FONT, change it to "fixed"; almost all X installations will have that available
#ifdef XFT
#define DEF_FONT "-bitstream-bitstream vera sans-medium-r-*-*-*-100-*-*-*-*-*-*"
#else
#define DEF_FONT "-b&h-lucida-medium-r-*-*-10-*-*-*-*-*-*-*"
#endif

// use named colours, #rgb, #rrggbb or #rrrgggbbb format
#define DEF_BORDER "#000"
#define DEF_TEXT "#000"
#define DEF_ACTIVE "#fd0"
#define DEF_INACTIVE "#aaa"
#define DEF_MENU "#ddd"
#define DEF_SELECTED "#aad"
#define DEF_EMPTY "#000"
#define DEF_BORDERWIDTH 2
#define ACTIVE_SHADOW 0x2000 // eg #fff becomes #ddd
constexpr auto SPACE = 3;

// change MODIFIER to None to remove the need to hold down a modifier key
// the Windows key should be Mod4Mask and the Alt key is Mod1Mask
#define MODIFIER Mod1Mask

// keys may be used by other apps, so change them here
#define KEY_CYCLEPREV XK_Tab
#define KEY_CYCLENEXT XK_q
#define KEY_FULLSCREEN XK_F11
#define KEY_TOGGLEZ XK_F12
// max time between clicks in double click
constexpr auto DEF_DBLCLKTIME = 400;

// a few useful masks made up out of X's basic ones. `ChildMask' is a silly name, but oh well.
#define ChildMask (SubstructureRedirectMask|SubstructureNotifyMask)
#define ButtonMask (ButtonPressMask|ButtonReleaseMask)
#define MouseMask (ButtonMask|PointerMotionMask)
#define KeyMask (KeyPressMask|KeyReleaseMask)

// false_v taken from https://quuxplusone.github.io/blog/2018/04/02/false-v/
template<typename...>
inline constexpr bool false_v = false;

template<typename T>
constexpr T ABS(T x) noexcept {
    if constexpr (std::is_integral_v<std::decay_t<T>>) {
        return x < 0 ? -x : x;
    } else {
        static_assert(false_v<T>, "ABS not implemented for given type!");
    }
}
#define ABS(x) (((x) < 0) ? -(x) : (x))

// shorthand for wordy function calls
#define setmouse(w, x, y) XWarpPointer(dsply, None, w, 0, 0, 0, 0, x, y)
#define ungrab() XUngrabPointer(dsply, CurrentTime)
#define grab(w, mask, curs) \
	(XGrabPointer(dsply, w, False, mask, GrabModeAsync, GrabModeAsync, None, curs, CurrentTime) == GrabSuccess)
#define grab_keysym(w, mask, keysym) \
	XGrabKey(dsply, XKeysymToKeycode(dsply, keysym), mask, w, True, GrabModeAsync, GrabModeAsync); \
	XGrabKey(dsply, XKeysymToKeycode(dsply, keysym), LockMask|mask, w, True, GrabModeAsync, GrabModeAsync); \
	if (numlockmask) \
	{ \
		XGrabKey(dsply, XKeysymToKeycode(dsply, keysym), numlockmask|mask, w, True, GrabModeAsync, GrabModeAsync); \
		XGrabKey(dsply, XKeysymToKeycode(dsply, keysym), numlockmask|LockMask|mask, w, True, GrabModeAsync, GrabModeAsync); \
	}

// I wanna know who the morons who prototyped these functions as implicit int are...
#define lower_win(c) ((void) XLowerWindow(dsply, (c)->frame))
#define raise_win(c) ((void) XRaiseWindow(dsply, (c)->frame))

template<typename T>
constexpr auto BORDERWIDTH(T) noexcept {
    return DEF_BORDERWIDTH;
}

int BARHEIGHT() noexcept;
// bar height

// minimum window width and height, enough for 3 buttons and a bit of titlebar
inline auto MINWINWIDTH() noexcept {
    return BARHEIGHT() * 4;
}

inline auto MINWINHEIGHT() noexcept {
    return BARHEIGHT() * 4;
}

// multipliers for calling gravitate
#define APPLY_GRAVITY 1
#define REMOVE_GRAVITY -1

// modes to call get_incsize with
#define PIXELS 0
#define INCREMENTS 1

// modes for find_client
#define WINDOW 0
#define FRAME 1

// modes for remove_client
#define WITHDRAW 0
#define REMAP 1

// stuff for the menu file
#define MAX_MENUITEMS 24
#define MAX_MENUITEMS_SIZE (sizeof(MenuItem) * MAX_MENUITEMS)
#define STR_SIZE 128
#define NO_MENU_LABEL "xterm"
#define NO_MENU_COMMAND "xterm"

/* This structure keeps track of top-level windows (hereinafter
 * 'clients'). The clients we know about (i.e. all that don't set
 * override-redirect) are kept track of in linked list starting at the
 * global pointer called, appropriately, 'clients'. 
 *
 * window and parent refer to the actual client window and the larger
 * frame into which we will reparent it respectively. trans is set to
 * None for regular windows, and the window's 'owner' for a transient
 * window. Currently, we don't actually do anything with the owner for
 * transients; it's just used as a boolean.
 *
 * ignore_unmap is for our own purposes and doesn't reflect anything
 * from X. Whenever we unmap a window intentionally, we increment
 * ignore_unmap. This way our unmap event handler can tell when it
 * isn't supposed to do anything. */

struct Client
{
	struct Client *next;
	char *name;
	XSizeHints *size;
	Window window, frame, trans;
	Colormap cmap;
	int x, y;
	int width, height;
	int ignore_unmap;
	unsigned int hidden;
	unsigned int was_hidden;
	unsigned int focus_order;
#ifdef SHAPE
	Bool has_been_shaped;
#endif
#ifdef XFT
	XftDraw *xftdraw;
#endif
    long getWMState() const noexcept;
    void setWMState(int) noexcept; 
};

struct Rect final {
    public:
        constexpr Rect() noexcept = default;
        constexpr Rect(int x, int y, int w, int h) noexcept : _x(x), _y(y), _width(w), _height(h) { }
        ~Rect() = default;
        constexpr auto getX() const noexcept { return _x; }
        constexpr auto getY() const noexcept { return _y; }
        constexpr auto getWidth() const noexcept { return _width; }
        constexpr auto getHeight() const noexcept { return _height; }
        void setX(int value) noexcept { _x = value; }
        void setX(int value, std::function<bool(int)> cond) noexcept {
            if (cond(_x)) {
                setX(value);
            }
        }
        void setY(int value) noexcept { _y = value; }
        void setY(int value, std::function<bool(int)> cond) noexcept {
            if (cond(_y)) {
                setY(value);
            }
        }
        void setWidth(int value) noexcept { _width = value; }
        void setWidth(int value, std::function<bool(int)> cond) noexcept { 
            if (cond(_width)) {
                setWidth(value);
            }
        }
        void setHeight(int value) noexcept { _height = value; }
        void setHeight(int value, std::function<bool(int)> cond) noexcept { 
            if (cond(_height)) {
                setHeight(value);
            }
        }
        void addToHeight(int accumulation) noexcept {
            setHeight(getHeight() + accumulation);
        }
        void addToWidth(int accumulation) noexcept {
            setWidth(getWidth() + accumulation);
        }
        void subtractFromHeight(int amount) noexcept {
            setHeight(getHeight() - amount);
        }
        void subtractFromWidth(int amount) noexcept {
            setWidth(getWidth() - amount);
        }
    private:

        int _x = 0;
        int _y = 0;
        int _width = 0;
        int _height = 0;
};

struct MenuItem final
{
    public:
        MenuItem() = default;
        MenuItem(const std::string& lbl, const std::string& cmd, int __x = 0, int w = 0) noexcept : command(cmd), label(lbl), x(__x), width(w) { }

        std::string command, label;
        int x = 0;
        int width = 0;
};

// Below here are (mainly generated with cproto) declarations and prototypes for each file.

// main.c
extern Display *dsply;
extern Window root;
extern int screen;
extern Client *head_client, *focused_client, *topmost_client, *fullscreen_client;
extern unsigned int in_taskbar, showing_taskbar, focus_count;
extern Rect fs_prevdims;
extern XFontStruct *font;
#ifdef XFT
extern XftFont *xftfont;
extern XftColor xft_detail;
#endif
extern GC border_gc, text_gc, active_gc, depressed_gc, inactive_gc, menu_gc, selected_gc, empty_gc;
extern XColor border_col, text_col, active_col, depressed_col, inactive_col, menu_col, selected_col, empty_col;
extern Cursor resize_curs;
extern Atom wm_state, wm_change_state, wm_protos, wm_delete, wm_cmapwins;
extern char *opt_font, *opt_border, *opt_text, *opt_active, *opt_inactive, *opt_menu, *opt_selected, *opt_empty;
#ifdef SHAPE
extern int shape, shape_event;
#endif
extern unsigned int numlockmask;

// events.c
extern void do_event_loop(void);

// client.c
extern Client *find_client(Window, int);
extern void set_wm_state(Client *, int);
extern long get_wm_state(Client *);
extern void send_config(Client *);
extern void remove_client(Client *, int);
extern void redraw(Client *);
extern void gravitate(Client *, int);
#ifdef SHAPE
extern void set_shape(Client *);
#endif
extern void check_focus(Client *);
extern Client *get_prev_focused(void);
extern void draw_hide_button(Client *, GC *, GC *);
extern void draw_toggledepth_button(Client *, GC *, GC *);
extern void draw_close_button(Client *, GC *, GC *);

// new.c
extern void make_new_client(Window);

// manage.c
extern void move(Client *);
extern void raise_lower(Client *);
extern void resize(Client *, int, int);
extern void hide(Client *);
extern void unhide(Client *);
extern void toggle_fullscreen(Client *);
extern void send_wm_delete(Client *);
extern void write_titletext(Client *, Window);

// misc.c
template<typename ... Args>
void err(Args&& ... parts) noexcept {
    std::cerr << "windowlab: ";
    (std::cerr << ... << parts);
    std::cerr << std::endl;
}

extern void err(const char *, ...);
extern void fork_exec(char *);
extern void sig_handler(int);
extern int handle_xerror(Display *, XErrorEvent *);
extern int ignore_xerror(Display *, XErrorEvent *);
extern int send_xmessage(Window, Atom, long);
extern void get_mouse_position(int *, int *);
extern void fix_position(Client *);
extern void refix_position(Client *, XConfigureRequestEvent *);
extern void copy_dims(Rect *, Rect *);
#ifdef DEBUG
extern void show_event(XEvent);
extern void dump(Client *);
extern void dump_clients(void);
#endif

class Taskbar final {
    public:
        static Taskbar& instance() noexcept;
        void cyclePrevious();
        void cycleNext();
        void leftClick(int);
        void rightClick(int);
        void rightClickRoot();
        void redraw();
        float getButtonWidth();
        Window& getWindow() noexcept { return _taskbar; }
    protected:
        Taskbar() = default;

    private:
        void make() noexcept;
    private:
        bool _made = false;
        Window _taskbar;
#if XFT
        XftDraw* _tbxftdraw = nullptr;
#endif

};
// taskbar.c
extern void make_taskbar(void);
extern void cycle_previous(void);
extern void cycle_next(void);
extern void lclick_taskbar(int);
extern void rclick_taskbar(int);
extern void rclick_root(void);
extern void redraw_taskbar(void);
extern float get_button_width(void);

// menufile.c
const std::filesystem::path& getDefMenuRc() noexcept;
using MenuItemList = std::vector<MenuItem>;
MenuItemList& getMenuItems() noexcept;
inline std::size_t getMenuItemCount() noexcept { return getMenuItems().size(); }
void clearMenuItems() noexcept;
void acquireMenuItems() noexcept;
bool shouldDoMenuItems() noexcept;
void requestMenuItems() noexcept;
#endif /* WINDOWLAB_H */
