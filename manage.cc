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

static void limit_size(ClientPointer , Rect *);
static bool get_incsize(ClientPointer , unsigned int *, unsigned int *, Rect *, int);

void 
Client::raiseLower() noexcept {
    auto& ct = ClientTracker::instance();
    if (auto self = sharedReference(); self == ct.getTopmostClient()) {
        lowerWindow();
        ct.setTopmostClient(nullptr); // lazy but amiwm does similar
    } else {
        raiseWindow();
        ct.setTopmostClient(self);
    }
}
void raise_lower(ClientPointer c) {
    if (c) {
        c->raiseLower();
    }
}

/* increment ignore_unmap here and decrement it in handle_unmap_event in events.c */

void
Client::hide() noexcept {
    if (!_hidden) {
        ++_ignoreUnmap;
        _hidden = true;
        auto& ct = ClientTracker::instance();
        auto& dm = DisplayManager::instance();
        if (sharedReference() == ct.getTopmostClient()) {
            ct.setTopmostClient(nullptr);
        }
        dm.unmapWindow(_frame);
        dm.unmapWindow(_window);
        setWMState(IconicState);
        ct.checkFocus(ClientTracker::instance().getPreviousFocused());
    }
}

void
Client::unhide() noexcept {
    if (_hidden) {
        _hidden = false;
        auto& ct = ClientTracker::instance();
        auto& dm = DisplayManager::instance();
        ct.setTopmostClient(sharedReference());
        dm.mapWindow(_window);
        dm.mapRaised(_frame);
        setWMState(NormalState);
    }
}


void toggle_fullscreen(ClientPointer c)
{
	int xoffset, yoffset, maxwinwidth, maxwinheight;
    auto& ctracker = ClientTracker::instance();
    auto& dm = DisplayManager::instance();
	if (c  && !c->getTrans()) {
        if (c == ctracker.getFullscreenClient()) { // reset to original size
            c->setDimensions(fs_prevdims);
            dm.moveResizeWindow(c->getFrame(), c->getX(), c->getY() - BARHEIGHT(), c->getWidth(), c->getHeight() + BARHEIGHT());
            dm.moveResizeWindow(c->getWidth(), 0, BARHEIGHT(), c->getWidth(), c->getHeight());
            c->sendConfig();
            ctracker.setFullscreenClient(nullptr);
			showing_taskbar = 1;
		} else { // make fullscreen
			xoffset = yoffset = 0;
            maxwinwidth = dm.getWidth();
            maxwinheight = dm.getHeight() - BARHEIGHT();
			if (ctracker.hasFullscreenClient()) { // reset existing fullscreen window to original size
                ctracker.getFullscreenClient()->setDimensions(fs_prevdims);
				dm.moveResizeWindow(ctracker.getFullscreenClient()->getFrame(), ctracker.getFullscreenClient()->getX(), ctracker.getFullscreenClient()->getY() - BARHEIGHT(), ctracker.getFullscreenClient()->getWidth(), ctracker.getFullscreenClient()->getHeight()+ BARHEIGHT());
				dm.moveResizeWindow(ctracker.getFullscreenClient()->getWindow(), 0, BARHEIGHT(), ctracker.getFullscreenClient()->getWidth(), ctracker.getFullscreenClient()->getHeight());
                ctracker.getFullscreenClient()->sendConfig();
			}
            fs_prevdims = c->getRect();
            c->setDimensions(0 - BORDERWIDTH(c), (BARHEIGHT() - BORDERWIDTH(c)), (maxwinwidth), maxwinheight);
			if (c->getSize()->flags & PMaxSize || c->getSize()->flags & PResizeInc) {
				if (c->getSize()->flags & PResizeInc) {
					Rect maxwinsize { xoffset, yoffset, maxwinwidth, maxwinheight };
					get_incsize(c, (unsigned int *)&c->getSize()->max_width, (unsigned int *)&c->getSize()->max_height, &maxwinsize, PIXELS);
				}
				if (c->getSize()->max_width < maxwinwidth) {
					c->setWidth( c->getSize()->max_width);
					xoffset = (maxwinwidth - c->getWidth()) / 2;
				}
				if (c->getSize()->max_height < maxwinheight) {
                    c->setHeight(c->getSize()->max_height);
					yoffset = (maxwinheight - c->getHeight()) / 2;
				}
			}
			dm.moveResizeWindow(c->getFrame(), c->getX(), c->getY(), maxwinwidth, maxwinheight);
			dm.moveResizeWindow(c->getWindow(), xoffset, yoffset, c->getWidth(), c->getHeight());
            c->sendConfig();
            ctracker.setFullscreenClient(c);
			showing_taskbar = in_taskbar;
		}
        Taskbar::instance().redraw();
	}
}

/* The name of this function is a bit misleading: if the client
 * doesn't listen to WM_DELETE then we just terminate it with extreme
 * prejudice. */
void
Client::sendWMDelete() noexcept {
	int n, found = 0;
    auto& dm = DisplayManager::instance();
	if (Atom* protocols = nullptr; XGetWMProtocols(dm.getDisplay(), _window, &protocols, &n)) {
		for (int i = 0; i < n; i++) {
			if (protocols[i] == wm_delete) {
				++found;
			}
		}
		XFree(protocols);
	}
	if (found) {
		send_xmessage(_window, wm_protos, wm_delete);
	} else {
		XKillClient(dm.getDisplay(), _window);
	}
}
void
Client::move() noexcept {
	XEvent ev;
	int old_cx = _x;
	int old_cy = _y;
	Rect bounddims;
	XSetWindowAttributes pattr;
    auto& dm = DisplayManager::instance();
    auto [dw, dh] = dm.getDimensions();
    auto [mousex, mousey] = getMousePosition();
	bounddims.setX((mousex - _x) - BORDERWIDTH(this));
	bounddims.setWidth((dw - bounddims.getX() - (getWidth() - bounddims.getX())) + 1);
	bounddims.setY(mousey - _y);
	bounddims.setHeight((dh - bounddims.getY() - (getHeight() - bounddims.getY())) + 1);
	bounddims.addToY((BARHEIGHT() * 2) - BORDERWIDTH(this));
	bounddims.addToHeight(getHeight() - ((BARHEIGHT() * 2) - DEF_BORDERWIDTH));

    auto constraint_win = dm.createWindow( bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, pattr);
#ifdef DEBUG
    std::cerr << "Client::move() : constraint_win is (" << bounddims.getX() << ", " << bounddims.getY() << ")-(" << (bounddims.getX() + bounddims.getWidth()) << ", " << (bounddims.getY() + bounddims.getHeight()) << ")" << std::endl;
#endif
    dm.mapWindow(constraint_win);

	if (!(XGrabPointer(dm.getDisplay(), dm.getRoot(), False, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, None, CurrentTime) == GrabSuccess)) {
		XDestroyWindow(dm.getDisplay(), constraint_win);
		return;
	}

	do
	{
		XMaskEvent(dm.getDisplay(), ExposureMask|MouseMask, &ev);
		switch (ev.type) {
			case Expose:
				if (ClientPointer exposed_c = ClientTracker::instance().find(ev.xexpose.window, FRAME); exposed_c) {
                    exposed_c->redraw();
				}
				break;
			case MotionNotify:
				_x = old_cx + (ev.xmotion.x - mousex);
				_y = old_cy + (ev.xmotion.y - mousey);
				XMoveWindow(dm.getDisplay(), _frame, _x, _y - BARHEIGHT());
                sendConfig();
				break;
		}
	} while (ev.type != ButtonRelease);

	ungrab();
	XDestroyWindow(dm.getDisplay(), constraint_win);
}

void 
Client::resize(int x, int y)
{
	XEvent ev;
	ClientPointer exposed_c;
	Window resize_win, resizebar_win;
	XSetWindowAttributes pattr, resize_pattr, resizebar_pattr;
    auto& dm = DisplayManager::instance();
    // inside the window, dragging outwards : TRUE
    // outside the window, dragging inwards : FALSE
    bool dragging_outwards = (x > _x + BORDERWIDTH(this)) && 
                             (x < (_x + _width) - BORDERWIDTH(this)) && 
                             (y > (_y - BARHEIGHT()) + BORDERWIDTH(this)) && 
                             (y < (_y + _height) - BORDERWIDTH(this));
    auto [dw, dh] = dm.getDimensions();

    Rect bounddims { 0, 0, static_cast<int>(dw), static_cast<int>(dh) };

	auto constraint_win = dm.createWindow(bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, pattr);
    dm.mapWindow(constraint_win);

	if (!(XGrabPointer(dm.getDisplay(), dm.getRoot(), False, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, resize_curs, CurrentTime) == GrabSuccess)) {
		XDestroyWindow(dm.getDisplay(), constraint_win);
		return;
	}
    Rect newdims { _x, _y - BARHEIGHT(), _width, _height + BARHEIGHT() };
    Rect recalceddims(newdims);

	// create and map resize window
	resize_pattr.override_redirect = True;
	resize_pattr.background_pixel = menu_col.pixel;
	resize_pattr.border_pixel = border_col.pixel;
	resize_pattr.event_mask = ChildMask|ButtonPressMask|ExposureMask|EnterWindowMask;
    resize_win = dm.createWindow(newdims, 
            DEF_BORDERWIDTH, 
            dm.getDefaultDepth(),
            CopyFromParent, 
            dm.getDefaultVisual(),
            CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWEventMask, resize_pattr);
    dm.mapRaised(resize_win);

	resizebar_pattr.override_redirect = True;
	resizebar_pattr.background_pixel = active_col.pixel;
	resizebar_pattr.border_pixel = border_col.pixel;
	resizebar_pattr.event_mask = ChildMask|ButtonPressMask|ExposureMask|EnterWindowMask;
	resizebar_win = XCreateWindow(dm.getDisplay(), resize_win, -DEF_BORDERWIDTH, -DEF_BORDERWIDTH, newdims.getWidth(), BARHEIGHT() - DEF_BORDERWIDTH, DEF_BORDERWIDTH, DefaultDepth(DisplayManager::instance().getDisplay(), DisplayManager::instance().getScreen()), CopyFromParent, DefaultVisual(DisplayManager::instance().getDisplay(), DisplayManager::instance().getScreen()), CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWEventMask, &resizebar_pattr);
    dm.mapRaised(resizebar_win);

	// temporarily swap drawables in order to draw on the resize window's XFT context
	XftDrawChange(_xftdraw, (Drawable) resizebar_win);

	// hide real window's frame
    dm.unmapWindow(_frame);

	do {
		XMaskEvent(dm.getDisplay(), ExposureMask|MouseMask, &ev);
		switch (ev.type) {
			case Expose:
				if (ev.xexpose.window == resizebar_win) {
                    writeTitleText(resizebar_win);
				} else {
					exposed_c = ClientTracker::instance().find(ev.xexpose.window, FRAME);
					if (exposed_c) {
                        exposed_c->redraw();
					}
				}
				break;
			case MotionNotify: {
					unsigned int in_taskbar = 1, leftedge_changed = 0, rightedge_changed = 0, topedge_changed = 0, bottomedge_changed = 0;
					int newwidth, newheight;
					// warping the pointer is wrong - wait until it leaves the taskbar
					if (ev.xmotion.y < BARHEIGHT()) {
						in_taskbar = 1;
					} else {
						if (in_taskbar == 1) { // first time outside taskbar
							in_taskbar = 0;
                            bounddims = { 0, BARHEIGHT(), static_cast<int>(dw), static_cast<int>(dh - BARHEIGHT()) };
							XMoveResizeWindow(dm.getDisplay(), constraint_win, bounddims.getX(), bounddims.getY(), bounddims.getWidth(), bounddims.getHeight());
							in_taskbar = 0;
						}
						// inside the window, dragging outwards
						if (dragging_outwards) {
							if (ev.xmotion.x < newdims.getX() + BORDERWIDTH(this)) {
								newdims.addToWidth(newdims.getX() + BORDERWIDTH(this) - ev.xmotion.x);
								newdims.setX(ev.xmotion.x - BORDERWIDTH(this));
								leftedge_changed = 1;
							} else if (ev.xmotion.x > newdims.getX() + newdims.getWidth() + BORDERWIDTH(this)) {
								newdims.setWidth((ev.xmotion.x - newdims.getX() - BORDERWIDTH(this)) + 1); // add 1 to allow window to be flush with edge of screen
								rightedge_changed = 1;
							}
							if (ev.xmotion.y < newdims.getY() + BORDERWIDTH(this)) {
								newdims.addToHeight(newdims.getY() + BORDERWIDTH(this) - ev.xmotion.y);
								newdims.setY(ev.xmotion.y - BORDERWIDTH(this));
								topedge_changed = 1;
							} else if (ev.xmotion.y > newdims.getY() + newdims.getHeight()+ BORDERWIDTH(this))
							{
								newdims.setHeight((ev.xmotion.y - newdims.getY() - BORDERWIDTH(this)) + 1); // add 1 to allow window to be flush with edge of screen
								bottomedge_changed = 1;
							}
						} else { // outside the window, dragging inwards
							unsigned int above_win = (ev.xmotion.y < newdims.getY() + BORDERWIDTH(this));
							unsigned int below_win = (ev.xmotion.y > newdims.getY() + newdims.getHeight()+ BORDERWIDTH(this));
							unsigned int leftof_win = (ev.xmotion.x < newdims.getX() + BORDERWIDTH(this));
							unsigned int rightof_win = (ev.xmotion.x > newdims.getX() + newdims.getWidth()+ BORDERWIDTH(this));

							unsigned int in_win = ((!above_win) && (!below_win) && (!leftof_win) && (!rightof_win));

							if (in_win) {
								unsigned int from_left = ev.xmotion.x - newdims.getX() - BORDERWIDTH(this);
								unsigned int from_right = newdims.getX() + newdims.getWidth() + BORDERWIDTH(this) - ev.xmotion.x;
								unsigned int from_top = ev.xmotion.y - newdims.getY() - BORDERWIDTH(this);
								unsigned int from_bottom = newdims.getY() + newdims.getHeight() + BORDERWIDTH(this) - ev.xmotion.y;
								if (from_left < from_right && from_left < from_top && from_left < from_bottom) {
                                    newdims.subtractFromWidth(ev.xmotion.x - newdims.getX() - BORDERWIDTH(this));
                                    newdims.setX(ev.xmotion.x - BORDERWIDTH(this));
									leftedge_changed = 1;
								} else if (from_right < from_top && from_right < from_bottom) {
                                    newdims.setWidth(ev.xmotion.x - newdims.getX() - BORDERWIDTH(this));
									rightedge_changed = 1;
								} else if (from_top < from_bottom) {
									newdims.subtractFromHeight(ev.xmotion.y - newdims.getY() - BORDERWIDTH(this));
									newdims.setY(ev.xmotion.y - BORDERWIDTH(this));
									topedge_changed = 1;
								} else {
                                    newdims.setHeight( ev.xmotion.y - newdims.getY() - BORDERWIDTH(this));
									bottomedge_changed = 1;
								}
							}
						}
						// coords have changed
						if (leftedge_changed || rightedge_changed || topedge_changed || bottomedge_changed) {
                            recalceddims = newdims;
							recalceddims.subtractFromHeight(BARHEIGHT());

							if (get_incsize(sharedReference(), (unsigned int *)&newwidth, (unsigned int *)&newheight, &recalceddims, PIXELS)) {
								if (leftedge_changed) {
									recalceddims.setX((recalceddims.getX() + recalceddims.getWidth()) - newwidth);
									recalceddims.setWidth(newwidth);
								} else if (rightedge_changed) {
                                    recalceddims.setWidth(newwidth);
								}

								if (topedge_changed) {
									recalceddims.setY((recalceddims.getY() + recalceddims.getHeight()) - newheight);
									recalceddims.setHeight(newheight);
								} else if (bottomedge_changed) {
                                    recalceddims.setHeight(newheight);
								}
							}

                            recalceddims.addToHeight(BARHEIGHT());
							limit_size(sharedReference(), &recalceddims);

							XMoveResizeWindow(dm.getDisplay(), resize_win, recalceddims.getX(), recalceddims.getY(), recalceddims.getWidth(), recalceddims.getHeight());
							XResizeWindow(dm.getDisplay(), resizebar_win, recalceddims.getWidth(), BARHEIGHT() - DEF_BORDERWIDTH);
						}
					}
				}
				break;
		}
	} while (ev.type != ButtonRelease);

    dm.ungrabServer();
	ungrab();
    setDimensions(recalceddims.getX(), recalceddims.getY() + BARHEIGHT(),
            recalceddims.getWidth(), recalceddims.getHeight() - BARHEIGHT());
	XMoveResizeWindow(dm.getDisplay(), _frame, _x, _y - BARHEIGHT(), _width, _height + BARHEIGHT());
	XResizeWindow(dm.getDisplay(), _window, _width, _height);

	// unhide real window's frame
	XMapWindow(dm.getDisplay(), _frame);
    
	XSetInputFocus(dm.getDisplay(), _window, RevertToNone, CurrentTime);

    sendConfig();
	XDestroyWindow(dm.getDisplay(), constraint_win);

	// reset the drawable
	XftDrawChange(_xftdraw, static_cast<Drawable>(_frame));
	
	XDestroyWindow(dm.getDisplay(), resizebar_win);
	XDestroyWindow(dm.getDisplay(), resize_win);
}

static void limit_size(ClientPointer c, Rect *newdims)
{
    auto [dw, dh] = DisplayManager::instance().getDimensions();

	if (c->getSize()->flags & PMinSize) {
        newdims->setWidth(c->getSize()->min_width, [compare = c->getSize()->min_width](int width) { return width < compare; });
        newdims->setHeight(c->getSize()->min_height, [compare = c->getSize()->min_height](int height) { return height < compare; });
	}

	if (c->getSize()->flags & PMaxSize) {
        newdims->setWidth(c->getSize()->max_width, [compare = c->getSize()->max_width](int width) { return width > compare; });
        newdims->setHeight(c->getSize()->max_height, [compare = c->getSize()->max_height](int height) { return height > compare; });
	}
    newdims->setWidth(MINWINWIDTH(), [](int width) { return width < MINWINWIDTH(); });
    newdims->setHeight(MINWINHEIGHT(), [](int height) { return height < MINWINHEIGHT(); });
    newdims->setWidth(dw, [dw](int width) { return width > dw; });
    newdims->setHeight((dh - BARHEIGHT()), [compare = (dh - BARHEIGHT())](int height) { return height > compare; });
}

/* If the window in question has a ResizeInc int, then it wants to be
 * resized in multiples of some (x,y). Here we set x_ret and y_ret to
 * the number of multiples (if mode == INCREMENTS) or the correct size
 * in pixels for said multiples (if mode == PIXELS). */

static bool get_incsize(ClientPointer c, unsigned int *x_ret, unsigned int *y_ret, Rect *newdims, int mode)
{
	if (c->getSize()->flags & PResizeInc) {
		auto basex = (c->getSize()->flags & PBaseSize) ? c->getSize()->base_width : (c->getSize()->flags & PMinSize) ? c->getSize()->min_width : 0;
		auto basey = (c->getSize()->flags & PBaseSize) ? c->getSize()->base_height : (c->getSize()->flags & PMinSize) ? c->getSize()->min_height : 0;
		// work around broken apps that set their resize increments to 0
		if (auto nWidth= newdims->getWidth(), nHeight = newdims->getHeight(); mode == PIXELS) {
			if (c->getSize()->width_inc != 0) {
				*x_ret = nWidth - ((nWidth - basex) % c->getSize()->width_inc);
			}
			if (c->getSize()->height_inc != 0) {
				*y_ret = nHeight - ((nHeight - basey) % c->getSize()->height_inc);
			}
		} else { // INCREMENTS
			if (c->getSize()->width_inc != 0) {
				*x_ret = (nWidth - basex) / c->getSize()->width_inc;
			}
			if (c->getSize()->height_inc != 0) {
				*y_ret = (nHeight - basey) / c->getSize()->height_inc;
			}
		}
		return true;
	}
	return false;
}

void 
Client::writeTitleText(Window /* barWin */) noexcept {
   if (!_trans && _name) {
       drawString(_xftdraw, &xft_detail, xftfont, SPACE, SPACE + xftfont->ascent, *_name);
   }
}
