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


ClientPointer
ClientTracker::find(Window w, int mode) {
    if (mode == FRAME) {
        for (auto& client : _clients) {
            if (client->getFrame() == w) {
                return client;
            }
        }
    } else {
        for (auto& client : _clients) {
            if (client->getWindow() == w) {
                return client;
            }
        }
    }
    return nullptr;
}

void 
Client::setWMState(int state) noexcept
{
    /* Attempt to follow the ICCCM by explicitly specifying 32 bits for
     * this property. Does this goof up on 64 bit systems? */
	CARD32 data[2];

	data[0] = state;
	data[1] = None; //Icon? We don't need no steenking icon.
    DisplayManager::instance().changeProperty(_window, wm_state, wm_state, 32, PropModeReplace, (unsigned char*)data, 2);
}

long 
Client::getWMState() const noexcept
{
/* If we can't find a WM_STATE we're going to have to assume
 * Withdrawn. This is not exactly optimal, since we can't really
 * distinguish between the case where no WM has run yet and when the
 * state was explicitly removed (Clients are allowed to either set the
 * atom to Withdrawn or just remove it... yuck.) */
	Atom real_type;
	int real_format;
	long state = WithdrawnState;
	unsigned long items_read, items_left;
	unsigned char *data;

    if (DisplayManager::instance().getWindowProperty(_window, wm_state, 0L, 2L, False, wm_state, &real_type, &real_format, &items_read, &items_left, &data) == Success && items_read) {
		state = *((long *)data);
		XFree(data);
	}
	return state;

}

void
Client::sendConfig() noexcept {
    XConfigureEvent ce;
    ce.type = ConfigureNotify;
    ce.event = _window;
    ce.window = _window;
    ce.x = _x;
    ce.y = _y;
    ce.width = _width;
    ce.height = _height;
    ce.border_width = 0;
    ce.above = None;
    ce.override_redirect = 0;
    DisplayManager::instance().sendEvent(_window, False, StructureNotifyMask, ce);
}

Client::~Client() {
	XftDrawDestroy(_xftdraw);
    if (_size) {
        XFree(_size);
        _size = nullptr;
    }
}

void
Client::removeFromView() noexcept { 
    auto& dm = DisplayManager::instance();
    gravitate(REMOVE_GRAVITY);
    dm.reparentWindow(_window, dm.getRoot(), _x, _y);
    dm.setWindowBorderWidth(_window, 1);
    dm.removeFromSaveSet(_window);
    dm.destroyWindow(_frame);
}


/* After pulling my hair out trying to find some way to tell if a
 * window is still valid, I've decided to instead carefully ignore any
 * errors raised by this function. We know that the X calls are, and
 * we know the only reason why they could fail -- a window has removed
 * itself completely before the Unmap and Destroy events get through
 * the queue to us. It's not absolutely perfect, but it works.
 *
 * The 'withdrawing' argument specifies if the client is actually
 * (destroying itself||being destroyed by us) or if we are merely
 * cleaning up its data structures when we exit mid-session. */
void
ClientTracker::remove(ClientPointer c, int mode) {
    auto& dm = DisplayManager::instance();
    dm.grabServer();

    // temporarily disable error handling
    dm.setErrorHandler([](Display*, XErrorEvent*) { return 0; });

    if constexpr (debugActive()) {
        err("removing ", (c->getName() ? *c->getName(): ""), ", ", mode, ": ", DisplayManager::instance().getPending(), " left");
    }

	if (mode == WITHDRAW) {
        c->setWMState(WithdrawnState);
	} else { //REMAP
        dm.mapWindow(c->getWindow());
	}
    c->removeFromView();
    remove(c);
    if (c == _fullscreenClient) {
        _fullscreenClient.reset();
	}
	if (c == _focusedClient) {
        _focusedClient.reset();
        checkFocus(getPreviousFocused());
	}

    dm.sync(False);
    // okay phew, reactivate it
    dm.setErrorHandler(handleXError);
    dm.ungrabServer();

    Taskbar::performRedraw();
}

void
Client::redraw() noexcept {
    auto self = sharedReference();
    auto& tracker = ClientTracker::instance();
    auto& dm = DisplayManager::instance();
    if (self == tracker.getFullscreenClient()) {
        return;
    }
    drawLine(border_gc, 0, BARHEIGHT() - DEF_BORDERWIDTH + DEF_BORDERWIDTH / 2, _width, BARHEIGHT() - DEF_BORDERWIDTH + DEF_BORDERWIDTH / 2);
	// clear text part of bar
	if (self == tracker.getFocusedClient()) {
        dm.fillRectangle(_frame, active_gc, 0, 0, _width - ((BARHEIGHT() - DEF_BORDERWIDTH) * 3), BARHEIGHT() - DEF_BORDERWIDTH);
	} else {
        dm.fillRectangle(_frame, inactive_gc, 0, 0, _width - ((BARHEIGHT() - DEF_BORDERWIDTH) * 3), BARHEIGHT() - DEF_BORDERWIDTH);
	}
	if (!_trans && _name) {
        drawString(_xftdraw, &xft_detail, xftfont, SPACE, SPACE + xftfont->ascent, *(_name));
	}
    auto background_gc = self == tracker.getFocusedClient() ? &active_gc : &inactive_gc;
    drawHideButton(&text_gc, background_gc);
    drawToggleDepthButton(&text_gc, background_gc);
    drawCloseButton(&text_gc, background_gc);

}

void 
Client::gravitate(int multiplier) noexcept {
/* Window gravity is a mess to explain, but we don't need to do much
 * about it since we're using X borders. For NorthWest et al, the top
 * left corner of the window when there is no WM needs to match up
 * with the top left of our fram once we manage it, and likewise with
 * SouthWest and the bottom right (these are the only values I ever
 * use, but the others should be obvious). Our titlebar is on the top
 * so we only have to adjust in the first case. */
	int dy = 0;
	int gravity = (_size->flags & PWinGravity) ? _size->win_gravity : NorthWestGravity;

	switch (gravity) {
		case NorthWestGravity:
		case NorthEastGravity:
		case NorthGravity:
			dy = BARHEIGHT();
			break;
		case CenterGravity:
			dy = BARHEIGHT()/2;
			break;
	}

	_y += multiplier * dy;
}



/* Well, the man pages for the shape extension say nothing, but I was
 * able to find a shape.PS.Z on the x.org FTP site. What we want to do
 * here is make the window shape be a boolean OR (or union, if you
 * prefer) of the client's shape and our titlebar. The titlebar
 * requires both a bound and a clip because it has a border -- the X
 * server will paint the border in the region between the two. (I knew
 * that using X borders would get me eventually... ;-)) */

void
Client::setShape() noexcept {
	int n, order;
	XRectangle temp;
    auto& dm = DisplayManager::instance();

	auto dummy = XShapeGetRectangles(dm.getDisplay(), _window, ShapeBounding, &n, &order);
	if (n > 1) {
		XShapeCombineShape(dm.getDisplay(), _frame, ShapeBounding, 0, BARHEIGHT(), _window, ShapeBounding, ShapeSet);
		temp.x = -getBorderWidth();
		temp.y = -getBorderWidth();
		temp.width = _width + (2 * getBorderWidth());
		temp.height = BARHEIGHT() + getBorderWidth();
		XShapeCombineRectangles(dm.getDisplay(), _frame, ShapeBounding, 0, 0, &temp, 1, ShapeUnion, YXBanded);
        XRectangle temp2;
		temp2.x = 0;
		temp2.y = 0;
		temp2.width = _width;
		temp2.height = BARHEIGHT() - getBorderWidth();
		XShapeCombineRectangles(dm.getDisplay(), _frame, ShapeClip, 0, BARHEIGHT(), &temp2, 1, ShapeUnion, YXBanded);
		_hasBeenShaped = 1;
	} else {
		if (_hasBeenShaped) {
			// I can't find a 'remove all shaping' function...
			temp.x = -getBorderWidth();
			temp.y = -getBorderWidth();
			temp.width = _width + (2 * getBorderWidth());
			temp.height = _height + BARHEIGHT() + (2 * getBorderWidth());
			XShapeCombineRectangles(dm.getDisplay(), _frame, ShapeBounding, 0, 0, &temp, 1, ShapeSet, YXBanded);
		}
	}
	XFree(dummy);
}

void
ClientTracker::checkFocus(ClientPointer c) {
	if (c) {
        auto& dm = DisplayManager::instance();
        dm.setInputFocus(c->getWindow());
        dm.installColormap(c->getColormap());
	}
	if (c != _focusedClient) {
		ClientPointer old_focused = _focusedClient;
		_focusedClient = c;
        ++_focusCount;
		if (c) {
            c->setFocusOrder(_focusCount);
            c->redraw();
		}
		if (old_focused) {
            old_focused->redraw();
		}
        Taskbar::performRedraw();
	}
}

ClientPointer
ClientTracker::getPreviousFocused() {
	ClientPointer prevFocused;
	unsigned int highest = 0;

    for (auto& c : _clients) {
		if (!c->isHidden() && c->getFocusOrder() > highest) {
			highest = c->getFocusOrder();
			prevFocused = c;
		}
	}
	return prevFocused;
}
void
Client::drawLine(GC gc, int x1, int y1, int x2, int y2) noexcept {
    DisplayManager::instance().drawLine(_frame, gc, x1, y1, x2, y2);
}
void 
Client::drawRectangle(GC gc, int x, int y, unsigned int width, unsigned int height) noexcept {
    DisplayManager::instance().drawRectangle(_frame, gc, x, y, width, height);
}
void 
Client::fillRectangle(GC gc, int x, int y, unsigned int width, unsigned int height) noexcept {
    DisplayManager::instance().fillRectangle(_frame, gc, x, y, width, height);
}
void
Client::drawHideButton(GC* detail, GC* background) noexcept {
	int x = _width - ((BARHEIGHT() - DEF_BORDERWIDTH) * 3);
	int topleft_offset = (BARHEIGHT() / 2) - 5; // 5 being ~half of 9
    fillRectangle(*background, x, 0, BARHEIGHT() - DEF_BORDERWIDTH, BARHEIGHT() - DEF_BORDERWIDTH);


	drawLine(detail, x + topleft_offset + 4, topleft_offset + 2, x + topleft_offset + 4, topleft_offset + 0);
	drawLine(detail, x + topleft_offset + 6, topleft_offset + 2, x + topleft_offset + 7, topleft_offset + 1);
	drawLine(detail, x + topleft_offset + 6, topleft_offset + 4, x + topleft_offset + 8, topleft_offset + 4);
	drawLine(detail, x + topleft_offset + 6, topleft_offset + 6, x + topleft_offset + 7, topleft_offset + 7);
	drawLine(detail, x + topleft_offset + 4, topleft_offset + 6, x + topleft_offset + 4, topleft_offset + 8);
	drawLine(detail, x + topleft_offset + 2, topleft_offset + 6, x + topleft_offset + 1, topleft_offset + 7);
	drawLine(detail, x + topleft_offset + 2, topleft_offset + 4, x + topleft_offset + 0, topleft_offset + 4);
	drawLine(detail, x + topleft_offset + 2, topleft_offset + 2, x + topleft_offset + 1, topleft_offset + 1);
}


void
Client::drawToggleDepthButton(GC* detail, GC* background) noexcept {
	int x = _width - ((BARHEIGHT() - DEF_BORDERWIDTH) * 2);
	int topleftOffset = (BARHEIGHT() / 2) - 6; // 6 being ~half of 11
    fillRectangle(*background, x, 0, BARHEIGHT() - DEF_BORDERWIDTH, BARHEIGHT() - DEF_BORDERWIDTH);

	drawRectangle(*detail, x + topleftOffset, topleftOffset, 7, 7);
	drawRectangle(*detail, x + topleftOffset + 3, topleftOffset + 3, 7, 7);
}


void
Client::drawCloseButton(GC* detail, GC* background) noexcept {
	int x = _width - (BARHEIGHT() - DEF_BORDERWIDTH);
	int topleftOffset = (BARHEIGHT() / 2) - 5; // 5 being ~half of 9
	fillRectangle(*background, x, 0, BARHEIGHT() - DEF_BORDERWIDTH, BARHEIGHT() - DEF_BORDERWIDTH);

	drawLine(detail, x + topleftOffset + 1, topleftOffset,     x + topleftOffset + 8, topleftOffset + 7);
	drawLine(detail, x + topleftOffset + 1, topleftOffset + 1, x + topleftOffset + 7, topleftOffset + 7);
	drawLine(detail, x + topleftOffset,     topleftOffset + 1, x + topleftOffset + 7, topleftOffset + 8);

	drawLine(detail, x + topleftOffset,     topleftOffset + 7, x + topleftOffset + 7, topleftOffset);
	drawLine(detail, x + topleftOffset + 1, topleftOffset + 7, x + topleftOffset + 7, topleftOffset + 1);
	drawLine(detail, x + topleftOffset + 1, topleftOffset + 8, x + topleftOffset + 8, topleftOffset + 1);
}

void
Client::raiseWindow() noexcept {
    DisplayManager::instance().raiseWindow(_frame);
}

void 
Client::lowerWindow() noexcept {
    DisplayManager::instance().lowerWindow(_frame);
}

Rect
Client::getRect() const noexcept {
    return { _x, _y, _width, _height };
}

