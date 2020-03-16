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
        ct.checkFocus(ct.getPreviousFocused());
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


void 
ClientTracker::toggleFullscreen() noexcept {
    auto c = getFocusedClient();
    auto& dm = DisplayManager::instance();
    auto& tbar = Taskbar::instance();
	if (c  && !c->getTrans()) {
        if (c == getFullscreenClient()) { // reset to original size
            c->setDimensions(getFullscreenPreviousDimensions());
            dm.moveResizeWindow(c->getFrame(), c->getX(), c->getY() - BARHEIGHT(), c->getWidth(), c->getHeight() + BARHEIGHT());
            dm.moveResizeWindow(c->getWidth(), 0, BARHEIGHT(), c->getWidth(), c->getHeight());
            c->sendConfig();
            setFullscreenClient(nullptr);
            tbar.setShowingTaskbar(true);
		} else { // make fullscreen
			int xoffset = 0;
            int yoffset = 0;
            int maxwinwidth = dm.getWidth();
            int maxwinheight = dm.getHeight() - BARHEIGHT();
			if (hasFullscreenClient()) { // reset existing fullscreen window to original size
                getFullscreenClient()->setDimensions(getFullscreenPreviousDimensions());
				dm.moveResizeWindow(getFullscreenClient()->getFrame(), getFullscreenClient()->getX(), getFullscreenClient()->getY() - BARHEIGHT(), getFullscreenClient()->getWidth(), getFullscreenClient()->getHeight()+ BARHEIGHT());
				dm.moveResizeWindow(getFullscreenClient()->getWindow(), 0, BARHEIGHT(), getFullscreenClient()->getWidth(), getFullscreenClient()->getHeight());
                getFullscreenClient()->sendConfig();
			}

            setFullscreenPreviousDimensions(c->getRect());
            c->setDimensions(0 - getBorderWidth(), (BARHEIGHT() - getBorderWidth()), (maxwinwidth), maxwinheight);
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
            setFullscreenClient(c);
            tbar.setShowingTaskbar(tbar.insideTaskbar());
		}
        Taskbar::performRedraw();
	}
}

/* The name of this function is a bit misleading: if the client
 * doesn't listen to WM_DELETE then we just terminate it with extreme
 * prejudice. */
void
Client::sendWMDelete() noexcept {
	int n = 0;
    int found = 0;
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
        sendXMessage(_window, wm_protos, wm_delete);
	} else {
        dm.killClient(_window);
	}
}
void
Client::move() noexcept {
	XEvent ev;
	int old_cx = _x;
	int old_cy = _y;
	XSetWindowAttributes pattr;
    auto& dm = DisplayManager::instance();
    auto& ct = ClientTracker::instance();
    auto [dw, dh] = dm.getDimensions();
    auto [mousex, mousey] = dm.getMousePosition();
    auto bdx = (mousex - _x) - getBorderWidth();
    auto bdy = (mousey - _y) + ((BARHEIGHT() * 2) - getBorderWidth());
    auto bdw = (dw - bdx - (getWidth() - bdx)) + 1;
    auto bdh = ((dh - bdy - (getHeight() - bdy)) + 1) + (getHeight() - ((BARHEIGHT() * 2) - DEF_BORDERWIDTH));
    Rect bounddims(bdx, bdy, bdw, bdh);
    auto constraint_win = dm.createWindow(bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, pattr);
    if constexpr (debugActive()) {
        std::cerr << "Client::move() : constraint_win is (" << bounddims.getX() << ", " << bounddims.getY() << ")-(" << (bounddims.getX() + bounddims.getWidth()) << ", " << (bounddims.getY() + bounddims.getHeight()) << ")" << std::endl;
    }
    dm.mapWindow(constraint_win);

	if (!(dm.grabPointer(false, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, None, CurrentTime) == GrabSuccess)) {
        dm.destroyWindow(constraint_win);
		return;
	}

	do {
		dm.maskEvent(ExposureMask|MouseMask, ev);
		switch (ev.type) {
			case Expose:
				if (ClientPointer exposed_c = ct.find(ev.xexpose.window, FRAME); exposed_c) {
                    exposed_c->redraw();
				}
				break;
			case MotionNotify:
				_x = old_cx + (ev.xmotion.x - mousex);
				_y = old_cy + (ev.xmotion.y - mousey);
                dm.moveWindow(_frame, _x, _y - BARHEIGHT());
                sendConfig();
				break;
		}
	} while (ev.type != ButtonRelease);

    dm.ungrab();
    dm.destroyWindow(constraint_win);
}

void 
Client::resize(int x, int y) {
	XEvent ev;
	ClientPointer exposed_c;
	Window resize_win, resizebar_win;
	XSetWindowAttributes pattr, resize_pattr, resizebar_pattr;
    auto& ct = ClientTracker::instance();
    auto& dm = DisplayManager::instance();
    // inside the window, dragging outwards : TRUE
    // outside the window, dragging inwards : FALSE
    bool dragging_outwards = (x > _x + getBorderWidth()) && 
                             (x < (_x + _width) - getBorderWidth()) && 
                             (y > (_y - BARHEIGHT()) + getBorderWidth()) && 
                             (y < (_y + _height) - getBorderWidth());
    auto [dw, dh] = dm.getDimensions();

    Rect bounddims { 0, 0, static_cast<int>(dw), static_cast<int>(dh) };

	auto constraint_win = dm.createWindow(bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, pattr);
    dm.mapWindow(constraint_win);

	if (!(dm.grabPointer(false, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, resize_curs, CurrentTime) == GrabSuccess)) {
        dm.destroyWindow(constraint_win);
		return;
	}
    Rect newdims { _x, _y - BARHEIGHT(), _width, _height + BARHEIGHT() };
    Rect recalceddims(newdims);

	// create and map resize window
	resize_pattr.override_redirect = True;
	resize_pattr.background_pixel = menu_col.pixel;
	resize_pattr.border_pixel = border_col.pixel;
	resize_pattr.event_mask = ChildMask|ButtonPressMask|ExposureMask|EnterWindowMask;
    resize_win = dm.createWindow(newdims, DEF_BORDERWIDTH, dm.getDefaultDepth(), CopyFromParent, dm.getDefaultVisual(), CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWEventMask, resize_pattr);
    dm.mapRaised(resize_win);

	resizebar_pattr.override_redirect = True;
	resizebar_pattr.background_pixel = active_col.pixel;
	resizebar_pattr.border_pixel = border_col.pixel;
	resizebar_pattr.event_mask = ChildMask|ButtonPressMask|ExposureMask|EnterWindowMask;
    resizebar_win = dm.createWindow(resize_win, -DEF_BORDERWIDTH, -DEF_BORDERWIDTH, newdims.getWidth(), BARHEIGHT() - DEF_BORDERWIDTH, DEF_BORDERWIDTH, DefaultDepth(DisplayManager::instance().getDisplay(), DisplayManager::instance().getScreen()), CopyFromParent, DefaultVisual(DisplayManager::instance().getDisplay(), DisplayManager::instance().getScreen()), CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWEventMask, resizebar_pattr);
    dm.mapRaised(resizebar_win);

	// temporarily swap drawables in order to draw on the resize window's XFT context
	XftDrawChange(_xftdraw, (Drawable) resizebar_win);

	// hide real window's frame
    dm.unmapWindow(_frame);

	do {
        dm.maskEvent(ExposureMask|MouseMask, ev);
		switch (ev.type) {
			case Expose:
				if (ev.xexpose.window == resizebar_win) {
                    writeTitleText(resizebar_win);
				} else {
					exposed_c = ct.find(ev.xexpose.window, FRAME);
					if (exposed_c) {
                        exposed_c->redraw();
					}
				}
				break;
			case MotionNotify: {
                    bool in_taskbar = true;
					unsigned int leftedge_changed = 0; 
                    unsigned int rightedge_changed = 0; 
                    unsigned int topedge_changed = 0; 
                    unsigned int bottomedge_changed = 0;
					int newwidth = 0;
                    int newheight = 0;
					// warping the pointer is wrong - wait until it leaves the taskbar
					if (ev.xmotion.y < BARHEIGHT()) {
						in_taskbar = true;
					} else {
						if (in_taskbar) { // first time outside taskbar
							in_taskbar = false;
                            bounddims = { 0, BARHEIGHT(), static_cast<int>(dw), static_cast<int>(dh - BARHEIGHT()) };
                            dm.moveResizeWindow(constraint_win, bounddims);
							in_taskbar = false;
						}
						// inside the window, dragging outwards
						if (dragging_outwards) {
							if (ev.xmotion.x < newdims.getX() + getBorderWidth()) {
								newdims.addToWidth(newdims.getX() + getBorderWidth() - ev.xmotion.x);
								newdims.setX(ev.xmotion.x - getBorderWidth());
								leftedge_changed = 1;
							} else if (ev.xmotion.x > newdims.getX() + newdims.getWidth() + getBorderWidth()) {
								newdims.setWidth((ev.xmotion.x - newdims.getX() - getBorderWidth()) + 1); // add 1 to allow window to be flush with edge of screen
								rightedge_changed = 1;
							}
							if (ev.xmotion.y < newdims.getY() + getBorderWidth()) {
								newdims.addToHeight(newdims.getY() + getBorderWidth() - ev.xmotion.y);
								newdims.setY(ev.xmotion.y - getBorderWidth());
								topedge_changed = 1;
							} else if (ev.xmotion.y > newdims.getY() + newdims.getHeight()+ getBorderWidth())
							{
								newdims.setHeight((ev.xmotion.y - newdims.getY() - getBorderWidth()) + 1); // add 1 to allow window to be flush with edge of screen
								bottomedge_changed = 1;
							}
						} else { // outside the window, dragging inwards
							unsigned int above_win = (ev.xmotion.y < newdims.getY() + getBorderWidth());
							unsigned int below_win = (ev.xmotion.y > newdims.getY() + newdims.getHeight()+ getBorderWidth());
							unsigned int leftof_win = (ev.xmotion.x < newdims.getX() + getBorderWidth());
							unsigned int rightof_win = (ev.xmotion.x > newdims.getX() + newdims.getWidth()+ getBorderWidth());

							unsigned int in_win = ((!above_win) && (!below_win) && (!leftof_win) && (!rightof_win));

							if (in_win) {
								unsigned int from_left = ev.xmotion.x - newdims.getX() - getBorderWidth();
								unsigned int from_right = newdims.getX() + newdims.getWidth() + getBorderWidth() - ev.xmotion.x;
								unsigned int from_top = ev.xmotion.y - newdims.getY() - getBorderWidth();
								unsigned int from_bottom = newdims.getY() + newdims.getHeight() + getBorderWidth() - ev.xmotion.y;
								if (from_left < from_right && from_left < from_top && from_left < from_bottom) {
                                    newdims.subtractFromWidth(ev.xmotion.x - newdims.getX() - getBorderWidth());
                                    newdims.setX(ev.xmotion.x - getBorderWidth());
									leftedge_changed = 1;
								} else if (from_right < from_top && from_right < from_bottom) {
                                    newdims.setWidth(ev.xmotion.x - newdims.getX() - getBorderWidth());
									rightedge_changed = 1;
								} else if (from_top < from_bottom) {
									newdims.subtractFromHeight(ev.xmotion.y - newdims.getY() - getBorderWidth());
									newdims.setY(ev.xmotion.y - getBorderWidth());
									topedge_changed = 1;
								} else {
                                    newdims.setHeight( ev.xmotion.y - newdims.getY() - getBorderWidth());
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

                            dm.moveResizeWindow(resize_win, recalceddims);
                            dm.resizeWindow(resizebar_win, recalceddims.getWidth(), BARHEIGHT() - DEF_BORDERWIDTH);
						}
					}
				}
				break;
		}
	} while (ev.type != ButtonRelease);

    dm.ungrabServer();
	dm.ungrab();
    setDimensions(recalceddims.getX(), recalceddims.getY() + BARHEIGHT(),
            recalceddims.getWidth(), recalceddims.getHeight() - BARHEIGHT());
    dm.moveResizeWindow(_frame, _x, _y - BARHEIGHT(), _width, _height + BARHEIGHT());
    dm.resizeWindow(_window, _width, _height);

	// unhide real window's frame
    dm.mapWindow(_frame);
    dm.setInputFocus(_window); 

    sendConfig();
    dm.destroyWindow(constraint_win);

	// reset the drawable
	XftDrawChange(_xftdraw, static_cast<Drawable>(_frame));
	
    dm.destroyWindow(resizebar_win);
    dm.destroyWindow(resize_win);
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
