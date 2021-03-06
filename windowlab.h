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

#define VERSION "140.17"
#define RELEASEDATE "2020-03-03"

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
#include <X11/extensions/shape.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>


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


constexpr auto getBorderWidth() noexcept {
    return DEF_BORDERWIDTH;
}

int getBarHeight() noexcept;
// bar height

// minimum window width and height, enough for 3 buttons and a bit of titlebar
inline auto getMinWinWidth() noexcept {
    return getBarHeight() * 4;
}

inline auto getMinWinHeight() noexcept {
    return getBarHeight() * 4;
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
/// @todo redo this with tag dispatching
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
         * Return which button was clicked - this is a multiple of getBarHeight()
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
        void setShape() noexcept;
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
        void dump() const noexcept;
        void sendWMDelete() noexcept;
        void removeFromView() noexcept;
        ~Client();
    private:
        void setDimensions(XWindowAttributes& attr) noexcept;
        Client(Window w) noexcept : _window(w) { };
        void initPosition() noexcept;
        void drawLine(GC gc, int x1, int y1, int x2, int y2) noexcept;
        void drawRectangle(GC gc, int x, int y, unsigned int width, unsigned int height) noexcept;
        void fillRectangle(GC gc, int x, int y, unsigned int width, unsigned int height) noexcept;
        inline void drawLine(GC* gc, int x1, int y1, int x2, int y2) noexcept { drawLine(*gc, x1, y1, x2, y2); }
    private:
        Window _window;
        Window _frame;
        Window _trans;
        std::optional<std::string> _name;
	    unsigned int _focus_order = 0u;
        Bool _hasBeenShaped = 0;
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

class Rect final {
    public:
        constexpr Rect() noexcept = default;
        constexpr Rect(int x, int y, int w = 0, int h = 0) noexcept : _x(x), _y(y), _width(w), _height(h) { }
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
        explicit operator XRectangle() const {
            XRectangle out;
            out.x = _x;
            out.y = _y;
            out.width = _width;
            out.height = _height;
            return out;
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
class DisplayManager final {
    public:
        static DisplayManager& instance() noexcept;
        Display* getDisplay() const noexcept { return _display; }
        void setDisplay(Display* disp) noexcept { _display = disp; }
        Window getRoot() const noexcept { return _root; }
        void setRoot(Window w) noexcept { _root = w; }
        auto getScreen() const noexcept { return _screen; }
        void setScreen(int screen) noexcept { _screen = screen; }
        auto getDefaultScreen() noexcept {
            return DefaultScreen(_display);
        }
        void grabServer() noexcept {
            XGrabServer(_display);
        }
        void ungrabServer() noexcept {
            XUngrabServer(_display);
        }
        void setMouse(Window w, int x, int y) noexcept {
            XWarpPointer(_display, None, w, 0, 0, 0, 0, x, y);
        }
        void ungrab() noexcept {
            XUngrabPointer(_display, CurrentTime);
        }
        bool grab(Window w, unsigned int mask, Cursor curs) noexcept {
            return grabPointer(w, false, mask, GrabModeAsync, GrabModeAsync, None, curs, CurrentTime) == GrabSuccess;
        }
        auto grab(unsigned int mask, Cursor curs) noexcept {
            return grab(_root, mask, curs);
        }

        void grabKeysym(Window w, unsigned int mask, KeySym keysym) noexcept;
        inline auto changeProperty(Window w, Atom property, Atom type, int format, int mode, unsigned char* data, int nelements) noexcept {
            return XChangeProperty(_display, w, property, type, format, mode, data, nelements);
        }
        inline auto getWindowProperty(Window w, Atom property, long longOffset, long longLength, Bool shouldDelete, Atom reqType, Atom* actualTypeReturn, int* actualFormatReturn, unsigned long* nitemsReturn, unsigned long* bytesAfterReturn, unsigned char** propReturn) noexcept {
            return XGetWindowProperty(_display, w, property, longOffset, longLength, shouldDelete, reqType, actualTypeReturn, actualFormatReturn, nitemsReturn, bytesAfterReturn, propReturn);
        }
        template<typename T>
        inline auto sendEvent(Window w, Bool propagate, long eventMask, T& eventSend) noexcept {
            return XSendEvent(_display, w, propagate, eventMask, (XEvent*)&eventSend);
        }

        inline auto reparentWindow(Window w, Window parent, int x, int y) noexcept {
            return XReparentWindow(_display, w, parent, x, y);
        }

        auto getWidth() const noexcept {
            return DisplayWidth(_display, _screen);
        }
        auto getHeight() const noexcept {
            return DisplayHeight(_display, _screen);
        }

        auto getDimensions() const noexcept {
            return std::make_tuple(getWidth(), getHeight());
        }
        auto getDefaultDepth() const noexcept {
            return DefaultDepth(_display, _screen);
        }

        template<typename T>
        auto unmapWindow(T thing) noexcept {
            return XUnmapWindow(_display, thing);
        }

        auto mapWindow(Window w) noexcept {
            return XMapWindow(_display, w);
        }

        auto mapRaised(Window w) noexcept {
            return XMapRaised(_display, w);
        }

        auto sync(Bool discard) noexcept {
            return XSync(_display, discard);
        }

        auto moveResizeWindow(Window w, int x, int y, unsigned int width, unsigned int height) noexcept {
            return XMoveResizeWindow(_display, w, x, y, width, height);
        }
        auto moveResizeWindow(Window w, const Rect& r) noexcept {
            return moveResizeWindow(w, r.getX(), r.getY(), r.getWidth(), r.getHeight());
        }
        auto getDefaultColormap() const noexcept {
            return DefaultColormap(_display, _screen);
        }
        auto allocNamedColor(Colormap colormap, const std::string& colorName, XColor& screenDefReturn, XColor& exactDefReturn) noexcept {
            return XAllocNamedColor(_display, colormap, colorName.c_str(), &screenDefReturn, &exactDefReturn);
        }
        auto allocNamedColor(Colormap colormap, const std::string& colorName, XColor& screenDefReturn) noexcept {
            XColor tmp;
            return allocNamedColor(colormap, colorName, screenDefReturn, tmp);
        }
        auto allocNamedColorFromDefaultColormap(const std::string& colorName, XColor& screenDefReturn, XColor& exactDefReturn) noexcept {
            return allocNamedColor(getDefaultColormap(), colorName, screenDefReturn, exactDefReturn);
        }
        auto allocNamedColorFromDefaultColormap(const std::string& colorName, XColor& screenDefReturn) noexcept {
            return allocNamedColor(getDefaultColormap(), colorName, screenDefReturn);
        }
        auto internAtom(const std::string& str, Bool onlyIfExists) noexcept {
            return XInternAtom(_display, str.c_str(), onlyIfExists);
        }

        auto allocColor(Colormap cm, XColor& screenInOut) noexcept {
            return XAllocColor(_display, cm, &screenInOut);
        }
        auto allocColorFromDefaultColormap(XColor& screenInOut) noexcept {
            return allocColor(getDefaultColormap(), screenInOut);
        }

        template<typename T>
        auto createGC(T drawable, unsigned long valueMask, XGCValues& values) noexcept {
            return XCreateGC(_display, drawable, valueMask, &values);
        }

        auto createGCForRoot(unsigned long valueMask, XGCValues& values) noexcept {
            return createGC(_root, valueMask, values);
        }

        auto getDefaultVisual() const noexcept {
            return DefaultVisual(_display, _screen);
        }

        auto getModifierMapping() noexcept {
            return XGetModifierMapping(_display);
        }
        auto createWindow(Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int borderWidth, int depth, unsigned int _class, Visual* v, unsigned long valueMask, XSetWindowAttributes& attributes) noexcept {
            return XCreateWindow(_display, parent, x, y, width, height, borderWidth, depth, _class, v, valueMask, &attributes);
        }
        auto createWindow(int x, int y, unsigned int width, unsigned int height, unsigned int borderWidth, int depth, unsigned int _class, Visual* v, unsigned long valueMask, XSetWindowAttributes& attributes) noexcept {
            return createWindow(_root, x, y, width, height, borderWidth, depth, _class, v, valueMask, attributes);
        }
        auto createWindow(Window parent, const Rect& rect, unsigned int borderWidth, int depth, unsigned int _class, Visual* v, unsigned long valueMask, XSetWindowAttributes& attributes) noexcept {
            return createWindow(parent, rect.getX(), rect.getY(), rect.getWidth(), rect.getHeight(), borderWidth, depth, _class, v, valueMask, attributes);
        }
        auto createWindow(const Rect& rect, unsigned int borderWidth, int depth, unsigned int _class, Visual* v, unsigned long valueMask, XSetWindowAttributes& attributes) noexcept {
            return createWindow(_root, rect, borderWidth, depth, _class, v, valueMask, attributes);
        }
        auto putbackEvent(XEvent& ev) noexcept {
            return XPutBackEvent(_display, &ev);
        }

        auto drawRectangle(Drawable d, GC gc, int x, int y, unsigned int width, unsigned int height) noexcept {
            return XDrawRectangle(_display, d, gc, x, y, width, height);
        }
        auto drawLine(Drawable d, GC gc, int x1, int y1, int x2, int y2) noexcept {
            return XDrawLine(_display, d, gc, x1, y1, x2, y2);
        }
        auto fillRectangle(Drawable d, GC gc, int x, int y, unsigned int width, unsigned int height) noexcept {
            return XFillRectangle(_display, d, gc, x, y, width, height);
        }

        auto destroyWindow(Window w) noexcept {
            return XDestroyWindow(_display, w);
        }

        template<typename Fn>
        auto setErrorHandler(Fn fn) noexcept {
            return XSetErrorHandler(fn);
        }

        int grabPointer(Window grabWindow, bool ownerEvents, unsigned int eventMask, int pointerMode, int keyboardMode, Window confineTo, Cursor cursor, Time time) noexcept {
            return XGrabPointer(_display, grabWindow, ownerEvents ? True : False, eventMask, pointerMode, keyboardMode, confineTo, cursor, time);
        }
        auto grabPointer(bool ownerEvents, unsigned int eventMask, int pointerMode, int keyboardMode, Window confineTo, Cursor cursor, Time time) noexcept { 
            return grabPointer(_root, ownerEvents, eventMask, pointerMode, keyboardMode, confineTo, cursor, time);
        }
        void maskEvent(long eventMask, XEvent& ev) noexcept {
            XMaskEvent(_display, eventMask, &ev);
        }

        void raiseWindow(Window w) noexcept {
            // I agree with Nick Gravgaard, who is the moron who marked this X function as implicit int return...
            (void) XRaiseWindow(_display, w);
        }
        void lowerWindow(Window w) noexcept {
            // I agree with Nick Gravgaard, who is the moron who marked this X function as implicit int return...
            (void) XLowerWindow(_display, w);
        }

        void grabKeysym(unsigned int mask, KeySym keysym) noexcept;


        constexpr auto getNumLockMask() const noexcept { return _numLockMask; }
        void setNumLockMask(unsigned int value) noexcept { _numLockMask = value; }
        std::tuple<int, int> getMousePosition() noexcept;

        auto resizeWindow(Window w, unsigned int width, unsigned int height) noexcept {
            return XResizeWindow(_display, w, width, height);
        }

        auto setInputFocus(Window focus, int revertTo = RevertToNone, Time time = CurrentTime) noexcept {
            return XSetInputFocus(_display, focus, revertTo, time);
        }

        auto free(GC gc) noexcept {
            return XFreeGC(_display, gc);
        }
        auto free(Cursor curs) noexcept {
            return XFreeCursor(_display, curs);
        }
        void free(XFontStruct* font) noexcept {
            if (font) {
                XFreeFont(_display, font);
            }
        }

        auto queryTree(Window w, Window* rootReturn, Window* parentReturn, Window** childrenReturn, unsigned int* numberOfChildrenReturn) noexcept {
            return XQueryTree(_display, w, rootReturn, parentReturn, childrenReturn, numberOfChildrenReturn);
        }
        auto queryTree(Window* rootReturn, Window* parentReturn, Window** childrenReturn, unsigned int* numberOfChildrenReturn) noexcept {
            return queryTree(_root, rootReturn, parentReturn, childrenReturn, numberOfChildrenReturn);
        }
        auto killClient(XID resource) noexcept {
            return XKillClient(_display, resource);
        }
        auto moveWindow(Window w, int x, int y) noexcept {
            return XMoveWindow(_display, w, x, y);
        }
        auto allowEvents(int eventMode, Time time = CurrentTime) noexcept {
            return XAllowEvents(_display, eventMode, time);
        }
        auto clearWindow(Window w) noexcept {
            return XClearWindow(_display, w);
        }
        auto configureWindow(Window w, unsigned int valueMask, XWindowChanges& values) noexcept {
            return XConfigureWindow(_display, w, valueMask, &values);
        }
        auto connectionNumber() const noexcept { return ConnectionNumber(_display); }
        auto pending() noexcept {
            return XPending(_display);
        }
        auto nextEvent(XEvent* evt) noexcept {
            XNextEvent(_display, evt);
        }
        auto getWindowAttributes(Window w, XWindowAttributes& windowAttributesReturn) noexcept {
            return XGetWindowAttributes(_display, w, &windowAttributesReturn);
        }

        auto installColormap(Colormap map) noexcept {
            return XInstallColormap(_display, map);
        }
        auto installColormap() noexcept {
            return installColormap(getDefaultColormap());
        }
        auto getPending() noexcept {
            return XPending(_display);
        }
        auto addToSaveSet(Window w) noexcept {
            return XAddToSaveSet(_display, w);
        }
        auto removeFromSaveSet(Window w) noexcept {
            return XRemoveFromSaveSet(_display, w);
        }
        auto setWindowBorderWidth(Window w, unsigned int width) noexcept {
            return XSetWindowBorderWidth(_display, w, width);
        }

        template<typename T>
        auto selectInput(Window w, T mask) noexcept {
            return XSelectInput(_display, w, mask);
        }

        auto getWMNormalHints(Window w, XSizeHints* hintsReturn, long& suppliedReturn) noexcept {
            return XGetWMNormalHints(_display, w, hintsReturn, &suppliedReturn);
        }
        auto getWMNormalHints(Window w, XSizeHints* hintsReturn) noexcept {
            long dummy = 0;
            return getWMNormalHints(w, hintsReturn, dummy);
        }
        auto getWMHints(Window w) noexcept {
            return XGetWMHints(_display, w);
        }
        auto getTransientForHint(Window w, Window& propWindowReturn) noexcept {
            return XGetTransientForHint(_display, w, &propWindowReturn);
        }

        auto allocSizeHints() noexcept {
            return XAllocSizeHints();
        }

        auto keycodeToKeysym(KeyCode keycode, unsigned int group = 0, unsigned int level = 0) noexcept {
            return XkbKeycodeToKeysym(_display, keycode, group, level);
        }


    private:
        DisplayManager() = default;
        //DisplayManager(Display* disp, Window r, int s) : _display(disp), _root(r), _screen(s) { }
    private:
        Display* _display = nullptr;
        Window _root = 0;
        int _screen = 0;
        unsigned int _numLockMask = 0;
};
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
        inline void withdraw(ClientPointer c) { remove(c, WITHDRAW); }
        inline void remap(ClientPointer c) { remove(c, REMAP); }
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
        void dump();
        constexpr const Rect& getFullscreenPreviousDimensions() const noexcept { return _fullscreenPreviousDimensions; }
        void setFullscreenPreviousDimensions(
                int x, std::function<bool(int)> xcond,
                int y, std::function<bool(int)> ycond,
                int width, std::function<bool(int)> wcond,
                int height, std::function<bool(int)> hcond) {
            _fullscreenPreviousDimensions.setX(x, xcond);
            _fullscreenPreviousDimensions.setY(y, ycond);
            _fullscreenPreviousDimensions.setWidth(width, wcond);
            _fullscreenPreviousDimensions.setHeight(height, hcond);
        }
        void setFullscreenPreviousDimensions(const Rect& other) noexcept {
            _fullscreenPreviousDimensions = other;
        }
        void toggleFullscreen() noexcept;

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
        Rect _fullscreenPreviousDimensions;
        unsigned int _focusCount = 0;

};
class Taskbar final {
    public:
        static Taskbar& instance() noexcept;
        static inline void performRedraw() noexcept { instance().redraw(); }
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
        void drawMenuItem(unsigned int index, bool active);

    public:
        void make() noexcept;
        constexpr bool showingTaskbar() const noexcept { return _showing; }
        constexpr bool insideTaskbar() const noexcept { return _inside; }
        void setShowingTaskbar(bool value) noexcept { _showing = value; }
        void setInsideTaskbar(bool value) noexcept { _inside = value; }
    private:
        bool _made = false;
        Window _taskbar;
        XftDraw* _tbxftdraw = nullptr;
        bool _showing = true;
        bool _inside = false;
};

extern XFontStruct *font;
extern XftFont *xftfont;
extern XftColor xft_detail;
extern GC border_gc, text_gc, active_gc, depressed_gc, inactive_gc, menu_gc, selected_gc, empty_gc;
extern XColor border_col, text_col, active_col, depressed_col, inactive_col, menu_col, selected_col, empty_col;
extern Cursor resize_curs;
extern Atom wm_state, wm_change_state, wm_protos, wm_delete, wm_cmapwins;
extern int shape, shape_event;

// events.c
void doEventLoop();

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

void signalHandler(int);
int handleXError(Display *, XErrorEvent *);
int sendXMessage(Window, Atom, long);
void showEvent(XEvent);
void dumpClients();

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
constexpr auto debugActive() noexcept {
#ifdef DEBUG
    return true;
#else
    return false;
#endif
}

#endif /* WINDOWLAB_H */
