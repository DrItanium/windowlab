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
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#include <X11/Xft/Xft.h>


// here are the default settings - change to suit your taste

// if you aren't sure about DEF_FONT, change it to "fixed"; almost all X installations will have that available
#define DEF_FONT "-bitstream-bitstream vera sans-medium-r-*-*-*-100-*-*-*-*-*-*"

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
constexpr auto MODIFIER = Mod1Mask;

// keys may be used by other apps, so change them here
constexpr auto KEY_CYCLEPREV = XK_Tab;
constexpr auto KEY_CYCLENEXT = XK_q;
constexpr auto KEY_FULLSCREEN = XK_F11;
constexpr auto KEY_TOGGLEZ = XK_F12;
// max time between clicks in double click
constexpr auto DEF_DBLCLKTIME = 400;

// a few useful masks made up out of X's basic ones. `ChildMask' is a silly name, but oh well.
constexpr auto ChildMask = (SubstructureRedirectMask|SubstructureNotifyMask);
constexpr auto ButtonMask = (ButtonPressMask|ButtonReleaseMask);
constexpr auto MouseMask = (ButtonMask|PointerMotionMask);
constexpr auto KeyMask = (KeyPressMask|KeyReleaseMask);

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
void setmouse(Window w, int x, int y) noexcept;
void ungrab() noexcept;
bool grab(Window w, unsigned int mask, Cursor curs) noexcept;
void grab_keysym(Window w, unsigned int mask, KeySym keysym) noexcept;

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
class Rect;
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

struct Client {
    public:
        using Ptr = std::shared_ptr<Client>;
        using WeakPtr = std::weak_ptr<Client>;
        static void makeNew(Window) noexcept;
    public:
        long getWMState() const noexcept;
        void setWMState(int) noexcept; 
        /**
         * Return which button was clicked - this is a multiple of BARHEIGHT()
         * from the right hand side; We only care about 0, 1 and 2. 
         */
        unsigned int boxClicked(int x) const noexcept;
        void drawHideButton(GC* detail, GC* background) noexcept;
        void drawToggleDepthButton(GC* detail, GC* background) noexcept;
        void drawCloseButton(GC* detail , GC* background) noexcept;
        void drawButton(GC* detail, GC* background, unsigned int whichBox) noexcept;
        void lowerWindow() noexcept;
        void raiseWindow() noexcept;
        void sendConfig() noexcept;
        void reparent() noexcept;
#ifdef SHAPE
        void setShape() noexcept;
#endif
        void redraw() noexcept;
        void rememberHidden() noexcept;
        void forgetHidden() noexcept;
        Ptr sharedReference() const noexcept { return _selfReference.lock(); }
        void raiseLower() noexcept;
        void hide() noexcept;
        void unhide() noexcept;
        void gravitate(int multiplier) noexcept;
        void move() noexcept;
        void writeTitleText(Window) noexcept;
        auto getWindow() const noexcept { return _window; }
        auto getFrame() const noexcept { return _frame; }
        auto getTrans() const noexcept { return _trans; }
        void setFrame(Window frame) noexcept { _frame = frame; }
        void setTrans(Window trans) noexcept { _trans = trans; }
        const std::optional<std::string>& getName() const noexcept { return _name; }
        void setName(const std::string& name) noexcept { _name.emplace(name); }
        void setName(const std::optional<std::string>& name) noexcept { _name = name; }
        constexpr auto getFocusOrder() const noexcept { return _focus_order; }
        void setFocusOrder(unsigned int value) noexcept { _focus_order = value; }
        void incrementFocusOrder() noexcept { ++_focus_order; }
        XSizeHints* getSize() const noexcept { return _size; }
        void setSize(XSizeHints* value) noexcept { _size = value; }
        auto getColormap() const noexcept { return _cmap; }
        void setColormap(Colormap value) noexcept { _cmap = value; } 
        auto getXftDraw() const noexcept { return _xftdraw; }
        void setXftDraw(XftDraw* value) noexcept { _xftdraw = value; }
        constexpr auto isHidden() const noexcept { return _hidden; }
        void setHidden(bool value) noexcept { _hidden = value; }
        constexpr auto wasHidden() const noexcept { return _wasHidden; }
        void setWasHidden(bool value) noexcept { _wasHidden = value; }
        constexpr auto getIgnoreUnmap() const noexcept { return _ignoreUnmap; }
        void decrementIgnoreUnmap() noexcept { --_ignoreUnmap; }
        void incrementIgnoreUnmap() noexcept { ++_ignoreUnmap; }
        constexpr auto getX() const noexcept { return _x; }
        void setX(int value) noexcept { _x = value; }
        constexpr auto getY() const noexcept { return _y; }
        void setY(int value) noexcept { _y = value; }
        constexpr auto getWidth() const noexcept { return _width; }
        void setWidth(int value) noexcept { _width = value; }
        constexpr auto getHeight() const noexcept { return _height; }
        void setHeight(int value) noexcept { _height = value; }
        Rect getRect() const noexcept;
        void setDimensions(const Rect& r) noexcept;
        void setDimensions(int x, int y, int width, int height) noexcept;
        void resize(int, int);
        void fixPosition() noexcept;
        void refixPosition(XConfigureRequestEvent*);
#ifdef DEBUG
        void dump() const noexcept;
#endif
        void sendWMDelete() noexcept;
        void removeFromView() noexcept;
        ~Client();
    private:
        void setDimensions(XWindowAttributes& attr) noexcept;
        Client(Window w) noexcept : _window(w) { };
        void initPosition() noexcept;
        void drawLine(GC gc, int x1, int y1, int x2, int y2) noexcept;
        inline void drawLine(GC* gc, int x1, int y1, int x2, int y2) noexcept { drawLine(*gc, x1, y1, x2, y2); }
    private:
        Window _window;
        Window _frame;
        Window _trans;
        std::optional<std::string> _name;
	    unsigned int _focus_order = 0u;
#ifdef SHAPE
        Bool _hasBeenShaped = 0;
#endif
        XSizeHints* _size = nullptr;
        Colormap _cmap = 0;
        XftDraw* _xftdraw = nullptr;
        bool _hidden = false;
        bool _wasHidden = false;
        int _ignoreUnmap = 0;
        int _x = 0;
        int _y = 0;
        int _width = 0;
        int _height = 0;
        WeakPtr _selfReference;
};

struct Rect final {
    public:
        constexpr Rect() noexcept = default;
        constexpr Rect(int x, int y, int w, int h) noexcept : _x(x), _y(y), _width(w), _height(h) { }
        constexpr Rect(const Rect& r) noexcept : _x(r._x), _y(r._y), _width(r._width), _height(r._height) { }
        constexpr Rect& operator=(const Rect&) = default;
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
        void addToHeight(int accumulation = 1) noexcept {
            setHeight(getHeight() + accumulation);
        }
        void addToWidth(int accumulation = 1) noexcept {
            setWidth(getWidth() + accumulation);
        }
        void subtractFromHeight(int amount = 1) noexcept {
            setHeight(getHeight() - amount);
        }
        void subtractFromWidth(int amount = 1) noexcept {
            setWidth(getWidth() - amount);
        }
    private:
        int _x = 0;
        int _y = 0;
        int _width = 0;
        int _height = 0;
};

void forkExec(const std::string&);

struct MenuItem final {
    public:
        MenuItem() = default;
        MenuItem(const std::string& lbl, const std::string& cmd, int __x = 0, int w = 0) noexcept : _command(cmd), _label(lbl), _x(__x), _width(w) { }
        const std::string& getCommand() const noexcept { return _command; }
        const std::string& getLabel() const noexcept { return _label; }
        constexpr auto getX() const noexcept { return _x; }
        constexpr auto getWidth() const noexcept { return _width; }
        void setX(int value) noexcept { _x = value; }
        void setWidth(int value) noexcept { _width = value; }
        bool isEmpty() const noexcept { return _command.empty() && _label.empty(); }
        bool labelIsEmpty() const noexcept { return _label.empty(); }
        bool commandIsEmpty() const noexcept { return _command.empty(); }
        void forkExec() noexcept { ::forkExec(_command); }
    private:
        std::string _command;
        std::string _label;
        int _x = 0;
        int _width = 0;
};

// Below here are (mainly generated with cproto) declarations and prototypes for each file.

// main.c
extern Display *dsply;
extern Window root;
extern int screen;
using ClientPointer = typename Client::Ptr;
class ClientTracker final {
    public:
        static ClientTracker& instance() noexcept {
            static ClientTracker ct;
            return ct;
        }
    public:
        auto find(ClientPointer p) {
            return std::find(_clients.begin(), _clients.end(), p);
        }
        ClientPointer find(Window, int);
        void add(ClientPointer p) { _clients.emplace_back(p); }
        auto back() { return _clients.back(); }
        auto back() const { return _clients.back(); }
        auto front() { return _clients.front(); }
        auto front() const { return _clients.front(); }
        auto size() const noexcept { return _clients.size(); }
        auto at(std::size_t index) noexcept { return _clients[index]; }
        auto end() const noexcept { return _clients.end(); }
        auto end() noexcept { return _clients.end(); }
        auto begin() const noexcept { return _clients.begin(); }
        auto begin() noexcept { return _clients.begin(); }
        [[nodiscard]] bool empty() const noexcept { return _clients.empty(); }
        ClientPointer getPreviousFocused();
        /**
         * Visit each client and apply the given function to it.
         * @param fn The function to apply to each client
         * @return boolean value to signify if execution should terminate early (return true for it)
         */
        bool accept(std::function<bool(ClientPointer)> fn);
        void remove(ClientPointer, int);
        void checkFocus(ClientPointer c);
        auto getFocusedClient() const noexcept { return _focusedClient; }
        void setFocusedClient(ClientPointer p) noexcept { _focusedClient = p; }
        bool hasFocusedClient() const noexcept { return static_cast<bool>(_focusedClient); }
        auto getFullscreenClient() const noexcept { return _fullscreenClient; }
        void setFullscreenClient(ClientPointer p) noexcept { _fullscreenClient = p; }
        bool hasFullscreenClient() const noexcept { return static_cast<bool>(_fullscreenClient); }
        auto getTopmostClient() const noexcept { return _topmostClient; }
        void setTopmostClient(ClientPointer p) noexcept { _topmostClient = p; }
        bool hasTopmostClient() const noexcept { return static_cast<bool>(_topmostClient); }
#ifdef DEBUG
        void dump();
#endif
    public:
        ClientTracker(const ClientTracker&) = delete;
        ClientTracker(ClientTracker&&) = delete;
    private:
        ClientTracker() = default;
        bool remove(ClientPointer p) {
            if (auto loc = this->find(p); loc != _clients.end()) {
                _clients.erase(loc);
                return true;
            } else {
                return false;
            }
        }
    private:
        std::vector<ClientPointer> _clients;
        ClientPointer _focusedClient;
        ClientPointer _topmostClient;
        ClientPointer _fullscreenClient;

};
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
        XftDraw* _tbxftdraw = nullptr;
};

extern bool in_taskbar, showing_taskbar;
extern unsigned int focus_count;
extern Rect fs_prevdims;
extern XFontStruct *font;
extern XftFont *xftfont;
extern XftColor xft_detail;
extern GC border_gc, text_gc, active_gc, depressed_gc, inactive_gc, menu_gc, selected_gc, empty_gc;
extern XColor border_col, text_col, active_col, depressed_col, inactive_col, menu_col, selected_col, empty_col;
extern Cursor resize_curs;
extern Atom wm_state, wm_change_state, wm_protos, wm_delete, wm_cmapwins;
extern std::string opt_font, opt_border, opt_text, opt_active, opt_inactive, opt_menu, opt_selected, opt_empty;
#ifdef SHAPE
extern int shape, shape_event;
#endif
extern unsigned int numlockmask;

// events.c
void doEventLoop();

// manage.c
void toggle_fullscreen(ClientPointer);

// misc.c
template<typename ... Args>
void err(Args&& ... parts) noexcept {
    std::cerr << "windowlab: ";
    (std::cerr << ... << parts);
    std::cerr << std::endl;
}

template<typename ... Args>
void printToStderr(Args&& ... parts) noexcept {
    (std::cerr << ... << parts);
    std::cerr << std::endl;
}

std::optional<std::string> getEnvironmentVariable(const std::string& name) noexcept;
std::string getEnvironmentVariable(const std::string& name, const std::string& defaultValue) noexcept;

void sig_handler(int);
int handle_xerror(Display *, XErrorEvent *);
int ignore_xerror(Display *, XErrorEvent *);
int send_xmessage(Window, Atom, long);
std::tuple<int, int> getMousePosition();
#ifdef DEBUG
void showEvent(XEvent);
void dumpClients();
#endif

Window createWindow(Display* disp, Window parent, const Rect& rect, unsigned int borderWidth, int depth, unsigned int _class, Visual* v, unsigned long valueMask, XSetWindowAttributes* attributes) noexcept;
void drawString(XftDraw* d, XftColor* color, XftFont* font, int x, int y, const std::string& string);
std::tuple<Status, std::optional<std::string>> fetchName(Display* disp, Window w);

// taskbar.c

// menufile.c
class Menu final {
    public:
        static Menu& instance() noexcept;
        void clear() noexcept { _menuItems.clear(); }
        std::shared_ptr<MenuItem> at(std::size_t index) noexcept;
        const auto& getMenuItems() const noexcept { return _menuItems; }
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
        std::vector<std::shared_ptr<MenuItem>> _menuItems;
        bool _updateMenuItems = true;
};
const std::filesystem::path& getDefMenuRc() noexcept;

#endif /* WINDOWLAB_H */
