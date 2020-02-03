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

std::vector<typename Client::Ptr> clients;

ClientPointer find_client(Window w, int mode) {
	if (mode == FRAME) {
        for (auto& client : clients) {
            if (client->frame == w) {
                return client;
            }
        }
	} else { // WINDOW
        for (auto& client : clients) {
            if (client->window == w) {
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

	XChangeProperty(dsply, window, wm_state, wm_state, 32, PropModeReplace, (unsigned char *)data, 2);
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

	if (XGetWindowProperty(dsply, window, wm_state, 0L, 2L, False, wm_state, &real_type, &real_format, &items_read, &items_left, &data) == Success && items_read) {
		state = *(long *)data;
		XFree(data);
	}
	return state;

}

void
Client::sendConfig() noexcept {
    XConfigureEvent ce;
    ce.type = ConfigureNotify;
    ce.event = window;
    ce.window = window;
    ce.x = x;
    ce.y = y;
    ce.width = width;
    ce.height = height;
    ce.border_width = 0;
    ce.above = None;
    ce.override_redirect = 0;

    XSendEvent(dsply, window, False, StructureNotifyMask, (XEvent*)&ce);
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
void remove_client(ClientPointer c, int mode)
{
	ClientPointer p;

	XGrabServer(dsply);
	XSetErrorHandler(ignore_xerror);

#ifdef DEBUG
    err("removing ", (c->name ? *c->name : ""), ", ", mode, ": ", XPending(dsply), " left");
#endif

	if (mode == WITHDRAW) {
        c->setWMState(WithdrawnState);
	} else { //REMAP
		XMapWindow(dsply, c->window);
	}
	gravitate(c, REMOVE_GRAVITY);
	XReparentWindow(dsply, c->window, root, c->x, c->y);
	XSetWindowBorderWidth(dsply, c->window, 1);
	XftDrawDestroy(c->xftdraw);
	XRemoveFromSaveSet(dsply, c->window);
	XDestroyWindow(dsply, c->frame);
    removeClientFromList(c);
	if (c->size)
	{
		XFree(c->size);
	}
	if (c == fullscreen_client)
	{
		fullscreen_client = nullptr;
	}
	if (c == focused_client)
	{
		focused_client = nullptr;
		check_focus(get_prev_focused());
	}

	XSync(dsply, False);
	XSetErrorHandler(handle_xerror);
	XUngrabServer(dsply);

    Taskbar::instance().redraw();
}

void redraw(ClientPointer c)
{
	if (c == fullscreen_client) {
		return;
	}
	XDrawLine(dsply, c->frame, border_gc, 0, BARHEIGHT() - DEF_BORDERWIDTH + DEF_BORDERWIDTH / 2, c->width, BARHEIGHT() - DEF_BORDERWIDTH + DEF_BORDERWIDTH / 2);
	// clear text part of bar
	if (c == focused_client) {
		XFillRectangle(dsply, c->frame,   active_gc, 0, 0, c->width - ((BARHEIGHT() - DEF_BORDERWIDTH) * 3), BARHEIGHT() - DEF_BORDERWIDTH);
	} else {
		XFillRectangle(dsply, c->frame, inactive_gc, 0, 0, c->width - ((BARHEIGHT() - DEF_BORDERWIDTH) * 3), BARHEIGHT() - DEF_BORDERWIDTH);
	}
	if (!c->trans && c->name) {
        drawString(c->xftdraw, &xft_detail, xftfont, SPACE, SPACE + xftfont->ascent, *(c->name));
	}
    auto background_gc = c == focused_client ? &active_gc : &inactive_gc;
    c->drawHideButton(&text_gc, background_gc);
    c->drawToggleDepthButton(&text_gc, background_gc);
    c->drawCloseButton(&text_gc, background_gc);
}

/* Window gravity is a mess to explain, but we don't need to do much
 * about it since we're using X borders. For NorthWest et al, the top
 * left corner of the window when there is no WM needs to match up
 * with the top left of our fram once we manage it, and likewise with
 * SouthWest and the bottom right (these are the only values I ever
 * use, but the others should be obvious). Our titlebar is on the top
 * so we only have to adjust in the first case. */

void gravitate(ClientPointer c, int multiplier)
{
	int dy = 0;
	int gravity = (c->size->flags & PWinGravity) ? c->size->win_gravity : NorthWestGravity;

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

	c->y += multiplier * dy;
}

/* Well, the man pages for the shape extension say nothing, but I was
 * able to find a shape.PS.Z on the x.org FTP site. What we want to do
 * here is make the window shape be a boolean OR (or union, if you
 * prefer) of the client's shape and our titlebar. The titlebar
 * requires both a bound and a clip because it has a border -- the X
 * server will paint the border in the region between the two. (I knew
 * that using X borders would get me eventually... ;-)) */

#ifdef SHAPE
void
Client::setShape() noexcept {
	int n, order;
	XRectangle temp;

	auto dummy = XShapeGetRectangles(dsply, window, ShapeBounding, &n, &order);
	if (n > 1) {
		XShapeCombineShape(dsply, frame, ShapeBounding, 0, BARHEIGHT(), window, ShapeBounding, ShapeSet);
		temp.x = -BORDERWIDTH(this);
		temp.y = -BORDERWIDTH(this);
		temp.width = width + (2 * BORDERWIDTH(this));
		temp.height = BARHEIGHT() + BORDERWIDTH(this);
		XShapeCombineRectangles(dsply, frame, ShapeBounding, 0, 0, &temp, 1, ShapeUnion, YXBanded);
        XRectangle temp2;
		temp2.x = 0;
		temp2.y = 0;
		temp2.width = width;
		temp2.height = BARHEIGHT() - BORDERWIDTH(this);
		XShapeCombineRectangles(dsply, frame, ShapeClip, 0, BARHEIGHT(), &temp2, 1, ShapeUnion, YXBanded);
		has_been_shaped = 1;
	} else {
		if (has_been_shaped) {
			// I can't find a 'remove all shaping' function...
			temp.x = -BORDERWIDTH(this);
			temp.y = -BORDERWIDTH(this);
			temp.width = width + (2 * BORDERWIDTH(this));
			temp.height = height + BARHEIGHT() + (2 * BORDERWIDTH(this));
			XShapeCombineRectangles(dsply, frame, ShapeBounding, 0, 0, &temp, 1, ShapeSet, YXBanded);
		}
	}
	XFree(dummy);
}
#endif

void check_focus(ClientPointer c)
{
	if (c) {
		XSetInputFocus(dsply, c->window, RevertToNone, CurrentTime);
		XInstallColormap(dsply, c->cmap);
	}
	if (c != focused_client) {
		ClientPointer old_focused = focused_client;
		focused_client = c;
		focus_count++;
		if (c) {
			c->focus_order = focus_count;
			redraw(c);
		}
		if (old_focused) {
			redraw(old_focused);
		}
        Taskbar::instance().redraw();
	}
}

ClientPointer get_prev_focused(void) {
	ClientPointer prev_focused;
	unsigned int highest = 0;

    for (auto& c : clients) {
		if (!c->hidden && c->focus_order > highest) {
			highest = c->focus_order;
			prev_focused = c;
		}
	}
	return prev_focused;
}
void
Client::drawHideButton(GC* detail, GC* background) noexcept {
	int x = width - ((BARHEIGHT() - DEF_BORDERWIDTH) * 3);
	int topleft_offset = (BARHEIGHT() / 2) - 5; // 5 being ~half of 9
	XFillRectangle(dsply, frame, *background, x, 0, BARHEIGHT() - DEF_BORDERWIDTH, BARHEIGHT() - DEF_BORDERWIDTH);

	XDrawLine(dsply, frame, *detail, x + topleft_offset + 4, topleft_offset + 2, x + topleft_offset + 4, topleft_offset + 0);
	XDrawLine(dsply, frame, *detail, x + topleft_offset + 6, topleft_offset + 2, x + topleft_offset + 7, topleft_offset + 1);
	XDrawLine(dsply, frame, *detail, x + topleft_offset + 6, topleft_offset + 4, x + topleft_offset + 8, topleft_offset + 4);
	XDrawLine(dsply, frame, *detail, x + topleft_offset + 6, topleft_offset + 6, x + topleft_offset + 7, topleft_offset + 7);
	XDrawLine(dsply, frame, *detail, x + topleft_offset + 4, topleft_offset + 6, x + topleft_offset + 4, topleft_offset + 8);
	XDrawLine(dsply, frame, *detail, x + topleft_offset + 2, topleft_offset + 6, x + topleft_offset + 1, topleft_offset + 7);
	XDrawLine(dsply, frame, *detail, x + topleft_offset + 2, topleft_offset + 4, x + topleft_offset + 0, topleft_offset + 4);
	XDrawLine(dsply, frame, *detail, x + topleft_offset + 2, topleft_offset + 2, x + topleft_offset + 1, topleft_offset + 1);
}


void
Client::drawToggleDepthButton(GC* detail, GC* background) noexcept {
	int x = width - ((BARHEIGHT() - DEF_BORDERWIDTH) * 2);
	int topleft_offset = (BARHEIGHT() / 2) - 6; // 6 being ~half of 11
	XFillRectangle(dsply, frame, *background, x, 0, BARHEIGHT() - DEF_BORDERWIDTH, BARHEIGHT() - DEF_BORDERWIDTH);

	XDrawRectangle(dsply, frame, *detail, x + topleft_offset, topleft_offset, 7, 7);
	XDrawRectangle(dsply, frame, *detail, x + topleft_offset + 3, topleft_offset + 3, 7, 7);
}


void
Client::drawCloseButton(GC* detail, GC* background) noexcept {
	int x = width - (BARHEIGHT() - DEF_BORDERWIDTH);
	int topleft_offset = (BARHEIGHT() / 2) - 5; // 5 being ~half of 9
	XFillRectangle(dsply, frame, *background, x, 0, BARHEIGHT() - DEF_BORDERWIDTH, BARHEIGHT() - DEF_BORDERWIDTH);

	XDrawLine(dsply, frame, *detail, x + topleft_offset + 1, topleft_offset, x + topleft_offset + 8, topleft_offset + 7);
	XDrawLine(dsply, frame, *detail, x + topleft_offset + 1, topleft_offset + 1, x + topleft_offset + 7, topleft_offset + 7);
	XDrawLine(dsply, frame, *detail, x + topleft_offset, topleft_offset + 1, x + topleft_offset + 7, topleft_offset + 8);

	XDrawLine(dsply, frame, *detail, x + topleft_offset, topleft_offset + 7, x + topleft_offset + 7, topleft_offset);
	XDrawLine(dsply, frame, *detail, x + topleft_offset + 1, topleft_offset + 7, x + topleft_offset + 7, topleft_offset + 1);
	XDrawLine(dsply, frame, *detail, x + topleft_offset + 1, topleft_offset + 8, x + topleft_offset + 8, topleft_offset + 1);
}

void
Client::raiseWindow() noexcept {
    // I agree with Nick Gravgaard, who is the moron who marked this X function as implicit int return...
    (void)XRaiseWindow(dsply, frame);
}

void 
Client::lowerWindow() noexcept {
    // I agree with Nick Gravgaard, who is the moron who marked this X function as implicit int return...
    (void)XLowerWindow(dsply, frame);
}

