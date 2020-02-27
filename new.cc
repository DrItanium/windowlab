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

#include "windowlab.h"

void
Client::setDimensions(const Rect& r) noexcept {
    setDimensions(r.getX(), r.getY(), r.getWidth(), r.getHeight());
}

void
Client::setDimensions(int x, int y, int width, int height) noexcept {
    _x = x;
    _y = y;
    _width = width;
    _height = height;
}

void
Client::setDimensions(XWindowAttributes& attr) noexcept {
    setDimensions(attr.x, attr.y, attr.width, attr.height);
    _cmap = attr.colormap;
}

/* Set up a client structure for the new (not-yet-mapped) window. The
 * confusing bit is that we have to ignore 2 unmap events if the
 * client was already mapped but has IconicState set (for instance,
 * when we are the second window manager in a session). That's
 * because there's one for the reparent (which happens on all viewable
 * windows) and then another for the unmapping itself. */

void
Client::makeNew(Window w) noexcept {
	XWindowAttributes attr;
    auto& clients = ClientTracker::instance();
    auto& dm = DisplayManager::instance();
    clients.add(ClientPointer(new Client(w)));
    auto c = clients.back();
    dm.grabServer();

    dm.getTransientForHint(w, c->_trans);
    auto [ status, opt ] = fetchName(dm.getDisplay(), w);
    c->setName(opt);
    dm.getWindowAttributes(w, attr);
    c->setDimensions(attr);
	c->_size = dm.allocSizeHints();
    c->_selfReference = c;
    dm.getWMNormalHints(c->_window, c->_size);

	// XReparentWindow seems to try an XUnmapWindow, regardless of whether the reparented window is mapped or not
	++c->_ignoreUnmap;
	
	if (attr.map_state != IsViewable) {
        c->initPosition();
        c->setWMState(NormalState);
		if (auto hints = dm.getWMHints(w); hints) {
			if (hints->flags & StateHint) {
                c->setWMState(hints->initial_state);
			}
			XFree(hints);
		}
	}

    c->fixPosition();
    c->gravitate(APPLY_GRAVITY);
    c->reparent();
    c->_xftdraw = XftDrawCreate(dm.getDisplay(), (Drawable) c->_frame, dm.getDefaultVisual(), dm.getDefaultColormap());


	if (c->getWMState() != IconicState) {
        dm.mapWindow(c->_window);
        dm.mapRaised(c->_frame);

        clients.setTopmostClient(c);
	} else {
        c->setHidden(true);
		if(attr.map_state == IsViewable) {
			++c->_ignoreUnmap;
            dm.unmapWindow(c->_window);
		}
	}

	// if no client has focus give focus to the new client
	if (!clients.hasFocusedClient()) {
        clients.checkFocus(c);
        clients.setFocusedClient(c);
	}

    dm.sync(False);
    dm.ungrabServer();

    Taskbar::performRedraw();
}

/* This one does *not* free the data coming back from Xlib; it just
 * sends back the pointer to what was allocated. */


/* Figure out where to map the window. c->x, c->y, c->width, and
 * c->height actually start out with values in them (whatever the
 * client passed to XCreateWindow).
 *
 * The ICCM says that there are no position/size fields anymore and
 * the SetWMNormalHints says that they are obsolete, so we use the values
 * we got from the window attributes
 * We honour both program and user preferences
 *
 * If we can't find a reasonable position hint, we make up a position
 * using the relative mouse co-ordinates and window size. To account
 * for window gravity while doing this, we add BARHEIGHT() into the
 * calculation and then degravitate. Don't think about it too hard, or
 * your head will explode. */

void
Client::initPosition() noexcept {
	// make sure it's big enough for the 3 buttons and a bit of bar
	if (_width < 4 * BARHEIGHT()) {
		_width = 4 * BARHEIGHT();
	}
	if (_height < BARHEIGHT()) {
		_height = BARHEIGHT();
	}

	if (_x == 0 && _y == 0) {
        auto [mousex, mousey] = DisplayManager::instance().getMousePosition();
		_x = mousex;
		_y = mousey + BARHEIGHT();
        gravitate(REMOVE_GRAVITY);
	}
}

void
Client::reparent() noexcept {
	XSetWindowAttributes pattr;
    auto& dm = DisplayManager::instance();

	pattr.override_redirect = True;
	pattr.background_pixel = empty_col.pixel;
	pattr.border_pixel = border_col.pixel;
	pattr.event_mask = ChildMask|ButtonPressMask|ExposureMask|EnterWindowMask;
    _frame = dm.createWindow(_x, _y - BARHEIGHT(), _width, _height + BARHEIGHT(), BORDERWIDTH(), dm.getDefaultDepth(), CopyFromParent, dm.getDefaultVisual(), CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWEventMask, pattr);

	if (shape) {
		XShapeSelectInput(dm.getDisplay(), _window, ShapeNotifyMask);
        setShape();
	}

    dm.addToSaveSet(_window);
	dm.selectInput(_window, ColormapChangeMask|PropertyChangeMask);
    dm.setWindowBorderWidth(_window, 0);
    dm.resizeWindow(_window, _width, _height);
    dm.reparentWindow(_window, _frame, 0, BARHEIGHT());

    sendConfig();
}
