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

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "windowlab.h"
#include <optional>
#include <string>

static void quitNicely();

std::optional<std::string>
getEnvironmentVariable(const std::string& varName) noexcept {
    if (auto result = ::getenv(varName.c_str()); result) {
        return std::make_optional(result);
    } else {
        return std::nullopt;
    }
}
std::string
getEnvironmentVariable(const std::string& varName, const std::string& defaultValue) noexcept {
    if (auto result = ::getenv(varName.c_str()); result) {
        return result;
    } else {
        return defaultValue;
    }
}

void forkExec(const std::string& cmd) {
	pid_t pid = fork();

	switch (pid) {
  		case 0:
            {
                setsid();
                auto envShell = getEnvironmentVariable("SHELL", "/bin/sh");
                std::filesystem::path envShellPath(envShell);
                auto envShellName = envShellPath.filename();
                if (envShellName.empty()) {
                    envShellName = envShell;
                }

                execlp(envShell.c_str(), envShellName.c_str(), "-c", cmd.c_str(), nullptr);
                err("exec failed, cleaning up child");
                exit(1);
                break;
            }
		case -1:
			err("can't fork");
			break;
	}
}

void signalHandler(int signal)
{
	pid_t pid;
	int status;

	switch (signal)
	{
		case SIGINT:
		case SIGTERM:
			quitNicely();
			break;
		case SIGHUP:
            Menu::instance().populate();
			break;
		case SIGCHLD:
			while ((pid = waitpid(-1, &status, WNOHANG)) != 0) {
				if ((pid == -1) && (errno != EINTR)) {
					break;
				} else {
					continue;
				}
			}
			break;
	}
}

int handleXError(Display *dsply, XErrorEvent *e)
{
    auto& clients = ClientTracker::instance();
	auto c = clients.find(e->resourceid, WINDOW);

	if (e->error_code == BadAccess && e->resourceid == DisplayManager::instance().getRoot()) {
		err("root window unavailable (maybe another wm is running?)");
		exit(1);
	} else {
		char msg[255] = { 0 };
		XGetErrorText(dsply, e->error_code, msg, sizeof msg);
        err("X error (", e->resourceid, "): ", msg);
	}

	if (c) {
        clients.withdraw(c);
	}
	return 0;
}


/* Currently, only send_wm_delete uses this one... */

int sendXMessage(Window w, Atom a, long x)
{
	XClientMessageEvent e;

	e.type = ClientMessage;
	e.window = w;
	e.message_type = a;
	e.format = 32;
	e.data.l[0] = x;
	e.data.l[1] = CurrentTime;

	return XSendEvent(DisplayManager::instance().getDisplay(), w, False, NoEventMask, (XEvent *)&e);
}


std::tuple<int, int>
DisplayManager::getMousePosition() noexcept {
    Window mouseRoot, mouseWin;
    int winX = 0;
    int winY = 0;
    unsigned int mask = 0;
    int tmpX = 0;
    int tmpY = 0;
    XQueryPointer(_display, _root, &mouseRoot, &mouseWin, &tmpX, &tmpY, &winX, &winY, &mask);
    return std::make_tuple(tmpX, tmpY);
}

/* If this is the fullscreen client we don't take getBarHeight() into account
 * because the titlebar isn't being drawn on the window. */

void
Client::fixPosition() noexcept {

    if constexpr (debugActive()) {
        printToStderr("fix_position(): client was (", _x, ", ", _y, ")-(", _x + _width, ", ", _y + _height, ")");
    }
	
    auto& ct = ClientTracker::instance();
    auto& dm = DisplayManager::instance();
	int titlebarheight = (ct.getFullscreenClient().get() == this) ? 0 : getBarHeight();
    int xmax = dm.getWidth();
    int ymax = dm.getHeight();

    if (_width < getMinWinWidth()) {
        _width = getMinWinWidth();
	}
    if (_height < getMinWinHeight()) {
        _height = getMinWinHeight();
    }
	
	if (_width > xmax) {
        _width = xmax;
	}
	if (_height + (getBarHeight() + titlebarheight) > ymax) {
		_height = ymax - (getBarHeight() + titlebarheight);
	}

	if (_x < 0) {
        _x = 0;
	}
	if (_y < getBarHeight()) {
        _y = getBarHeight();
	}

	if (_x + _width + getBorderWidth() >= xmax) {
        _x = xmax - _width;
	}
	if (_y + _height + getBarHeight() >= ymax) {
        _y = (ymax - _height) - getBarHeight();
	}

    if constexpr (debugActive()) {
        printToStderr("fix_position(): client is (", _x, ", ", _y, ")-(", _x + _width, ", ", _y + _height, ")");
    }

    _x -= getBorderWidth();
    _y -= getBorderWidth();
}

void
Client::refixPosition(XConfigureRequestEvent *e) {
	Rect olddims { _x - getBorderWidth(),
                   _y - getBorderWidth(),
                   _width,
                   _height };

    fixPosition();
	if (olddims.getX() != _x) {
		e->value_mask |= CWX;
	}
	if (olddims.getY() != _y) {
		e->value_mask |= CWY;
	}
	if (olddims.getWidth() != _width ) {
		e->value_mask |= CWWidth;
	}
	if (olddims.getHeight() != _height) {
		e->value_mask |= CWHeight;
	}
}


/* Bleh, stupid macro names. I'm not feeling creative today. */

#define SHOW_EV(name, memb) \
	case name: \
		s = #name; \
		w = e.memb.window; \
		break;
#define SHOW(name) \
	case name: \
		return #name;

void showEvent(XEvent e) {
    if constexpr (debugActive()) {
        std::string s;
        Window w;
        ClientPointer c;

        switch (e.type)
        {
            SHOW_EV(ButtonPress, xbutton)
            SHOW_EV(ButtonRelease, xbutton)
            SHOW_EV(ClientMessage, xclient)
            SHOW_EV(ColormapNotify, xcolormap)
            SHOW_EV(ConfigureNotify, xconfigure)
            SHOW_EV(ConfigureRequest, xconfigurerequest)
            SHOW_EV(CreateNotify, xcreatewindow)
            SHOW_EV(DestroyNotify, xdestroywindow)
            SHOW_EV(EnterNotify, xcrossing)
            SHOW_EV(Expose, xexpose)
            SHOW_EV(MapNotify, xmap)
            SHOW_EV(MapRequest, xmaprequest)
            SHOW_EV(MappingNotify, xmapping)
            SHOW_EV(MotionNotify, xmotion)
            SHOW_EV(PropertyNotify, xproperty)
            SHOW_EV(ReparentNotify, xreparent)
            SHOW_EV(ResizeRequest, xresizerequest)
            SHOW_EV(UnmapNotify, xunmap)
            default:
                if (shape && e.type == shape_event) {
                    s = "ShapeNotify";
                    w = ((XShapeEvent *)&e)->window;
                } else {
                    s = "unknown event";
                    w = None;
                }
                break;
        }

        c = ClientTracker::instance().find(w, WINDOW);
        err(w, ": ", ((c && c->getName()) ? *(c->getName()) : "(none)"), ": ", s);
    }
}

static 
std::string 
showState(ClientPointer c) {
    if constexpr (debugActive()) {
        switch (c->getWMState()) {
            SHOW(WithdrawnState)
            SHOW(NormalState)
            SHOW(IconicState)
            default: return "unknown state";
        }
    }
}

static 
std::string 
showGravity(ClientPointer c) {
    if constexpr (debugActive()) {
	    if (!c->getSize() || !(c->getSize()->flags & PWinGravity)) {
	    	return "no grav (NW)";
	    }

	    switch (c->getSize()->win_gravity) {
	    	SHOW(UnmapGravity)
	    	SHOW(NorthWestGravity)
	    	SHOW(NorthGravity)
	    	SHOW(NorthEastGravity)
	    	SHOW(WestGravity)
	    	SHOW(CenterGravity)
	    	SHOW(EastGravity)
	    	SHOW(SouthWestGravity)
	    	SHOW(SouthGravity)
	    	SHOW(SouthEastGravity)
	    	SHOW(StaticGravity)
	    	default: return "unknown grav";
	    }
    }
}

void 
Client::dump() const noexcept {
    if constexpr (debugActive()) {
        err((_name ? *_name : ""), "\n\t", 
                showState(sharedReference()), ",", 
                showGravity(sharedReference()), 
                ", ignore ", _ignoreUnmap, 
                ", was_hidden ", _wasHidden, 
                "\n\tframe ", _frame, 
                ", win ", _window, 
                ", geom ", 
                _width, "x", _height, "+", _x, "+", _y);
    }
}

void
ClientTracker::dump() {
    if constexpr (debugActive()) {
        for (const auto& c : _clients) {
            if (c) {
                c->dump();
            }
        }
    }
}

/* We use XQueryTree here to preserve the window stacking order,
 * since the order in our linked list is different. */

static 
void quitNicely() {
	unsigned int nwins;
	Window dummyw1, dummyw2, *wins;
    Menu::instance().clear();
    auto& dm = DisplayManager::instance();
    auto& ct = ClientTracker::instance();
    dm.queryTree(&dummyw1, &dummyw2, &wins, &nwins);
	for (unsigned int i = 0; i < nwins; i++) {
		if (auto c = ct.find(wins[i], FRAME); c) {
            ct.remove(c, REMAP);
        }
	}
	XFree(wins);

    dm.free(font);
	if (xftfont) {
		XftFontClose(dm.getDisplay(), xftfont);
	}
    dm.free(resize_curs);
    dm.free(border_gc);
    dm.free(text_gc);

    dm.installColormap();
    dm.setInputFocus(PointerRoot);

	XCloseDisplay(dm.getDisplay());
	exit(0);
}

void 
drawString(XftDraw* d, XftColor* color, XftFont* font, int x, int y, const std::string& string) {
    auto ptr = string.c_str();
    XftDrawString8(d, color, font, x, y, (unsigned char*)ptr, string.length());
}

std::tuple<Status, std::optional<std::string>> 
fetchName(Display* disp, Window w) {
    char* temporaryStorage = nullptr;
    auto status = XFetchName(disp, w, &temporaryStorage);
    std::optional<std::string> returned;
    if (temporaryStorage) {
        // copy and then discard the temporary
        // learned this technique from CLIPS to prevent memory management issues
        returned = std::make_optional(temporaryStorage);
        XFree(temporaryStorage);
    }
    return std::make_tuple(status, returned);
}

void 
DisplayManager::grabKeysym(Window w, unsigned int mask, KeySym keysym) noexcept {
    XGrabKey(_display, XKeysymToKeycode(_display, keysym), mask, w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(_display, XKeysymToKeycode(_display, keysym), LockMask|mask, w, True, GrabModeAsync, GrabModeAsync); 
    if (_numLockMask) { 
        XGrabKey(_display, XKeysymToKeycode(_display, keysym), _numLockMask|mask, w, True, GrabModeAsync, GrabModeAsync); 
        XGrabKey(_display, XKeysymToKeycode(_display, keysym), _numLockMask|LockMask|mask, w, True, GrabModeAsync, GrabModeAsync); 
    }
}
void 
DisplayManager::grabKeysym(unsigned int mask, KeySym keysym) noexcept {
    grabKeysym(_root, mask, keysym);
}
