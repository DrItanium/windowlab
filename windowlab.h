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
#include <list>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#ifdef XFT
#include <X11/Xft/Xft.h>
#endif


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
constexpr auto DEF_BORDERWIDTH = 2;
constexpr auto ACTIVE_SHADOW = 0x2000; // eg #fff becomes #ddd
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
constexpr auto APPLY_GRAVITY = 1;
constexpr auto REMOVE_GRAVITY = -1;

// modes to call get_incsize with
constexpr auto PIXELS = 0;
constexpr auto INCREMENTS = 1;

// modes for find_client
constexpr auto WINDOW = 0;
constexpr auto FRAME = 1;

// modes for remove_client
constexpr auto WITHDRAW = 0;
constexpr auto REMAP = 1;

// stuff for the menu file
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
    using Ptr = std::shared_ptr<Client>;
    Ptr next;
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
        void addToY(int value) noexcept {
            setY(getY() + value);
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
        void become(int x, int y, int w, int h) noexcept {
            setX(x);
            setY(y);
            setWidth(w);
            setHeight(h);
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
        const std::string& getCommand() const noexcept { return command; }
        const std::string& getLabel() const noexcept { return label; }
        constexpr auto getX() const noexcept { return x; }
        constexpr auto getWidth() const noexcept { return width; }
        void setX(int value) noexcept { x = value; }
        void setWidth(int value) noexcept { width = value; }
        std::string command, label;
        int x = 0;
        int width = 0;
};

// Below here are (mainly generated with cproto) declarations and prototypes for each file.

// main.c
extern Display *dsply;
extern Window root;
extern int screen;
using ClientPointer = std::shared_ptr<Client>;
extern ClientPointer head_client, focused_client, topmost_client, fullscreen_client;
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
void doEventLoop();

// client.c
ClientPointer find_client(Window, int);
void send_config(ClientPointer);
void remove_client(ClientPointer, int);
void redraw(ClientPointer);
void gravitate(ClientPointer, int);
#ifdef SHAPE
void set_shape(ClientPointer);
#endif
void check_focus(ClientPointer);
ClientPointer get_prev_focused();
void draw_hide_button(ClientPointer, GC *, GC *);
void draw_toggledepth_button(ClientPointer, GC *, GC *);
void draw_close_button(ClientPointer, GC *, GC *);

// new.c
void makeNewClient(Window);

// manage.c
void move(ClientPointer);
void raise_lower(ClientPointer);
void resize(ClientPointer, int, int);
void hide(ClientPointer);
void unhide(ClientPointer);
void toggle_fullscreen(ClientPointer);
void send_wm_delete(ClientPointer);
void write_titletext(ClientPointer, Window);

// misc.c
template<typename ... Args>
void err(Args&& ... parts) noexcept {
    std::cerr << "windowlab: ";
    (std::cerr << ... << parts);
    std::cerr << std::endl;
}

std::optional<std::string> getEnvironmentVariable(const std::string& name) noexcept;
std::string getEnvironmentVariable(const std::string& name, const std::string& defaultValue) noexcept;
void forkExec(const std::string&);
inline void forkExec(const MenuItem& item) noexcept { forkExec(item.getCommand()); }

void sig_handler(int);
int handle_xerror(Display *, XErrorEvent *);
int ignore_xerror(Display *, XErrorEvent *);
int send_xmessage(Window, Atom, long);
std::tuple<int, int> getMousePosition();
void fix_position(ClientPointer);
void refix_position(ClientPointer, XConfigureRequestEvent *);
#ifdef DEBUG
void show_event(XEvent);
void dump(ClientPointer);
void dump_clients(void);
#endif

Window createWindow(Display* disp, Window parent, const Rect& rect, unsigned int borderWidth, int depth, unsigned int _class, Visual* v, unsigned long valueMask, XSetWindowAttributes* attributes) noexcept;

// taskbar.c
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
    private:
        Taskbar() = default;
    private:
        void drawMenubar();
        unsigned int updateMenuItem(int mousex);
        void drawMenuItem(unsigned int index, unsigned int active);

    public:
        void make() noexcept;
    private:
        bool _made = false;
        Window _taskbar;
#if XFT
        XftDraw* _tbxftdraw = nullptr;
#endif

};

// menufile.c
class Menu final {
    public:
        static Menu& instance() noexcept;
        void clear() noexcept { _menuItems.clear(); }
        std::optional<MenuItem> at(std::size_t index) noexcept;
        const std::vector<MenuItem>& getMenuItems() const noexcept { return _menuItems; }
        std::size_t size() const noexcept { return _menuItems.size(); }
        void requestMenuItemUpdate() noexcept { _updateMenuItems = true; }
        constexpr bool shouldRepopulate() const noexcept { return _updateMenuItems; }
        auto begin() const noexcept { return _menuItems.begin(); }
        auto end() const noexcept { return _menuItems.end(); }
        auto cbegin() const noexcept { return _menuItems.begin(); }
        auto cend() const noexcept { return _menuItems.end(); }
        auto begin() noexcept { return _menuItems.begin(); }
        auto end() noexcept { return _menuItems.end(); }
        void populate() noexcept;
    private:
        Menu() = default;
    private:
        std::vector<MenuItem> _menuItems;
        bool _updateMenuItems = true;
};
const std::filesystem::path& getDefMenuRc() noexcept;

#endif /* WINDOWLAB_H */
