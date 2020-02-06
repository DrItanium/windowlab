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
    if (auto self = sharedReference(); self == topmost_client) {
        lowerWindow();
        topmost_client = nullptr; // lazy but amiwm does similar
    } else {
        raiseWindow();
        topmost_client = self;
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
    if (!hidden) {
        ignore_unmap++;
        hidden = true;
        if (sharedReference() == topmost_client) {
            topmost_client = nullptr;
        }
        XUnmapWindow(dsply, _frame);
        XUnmapWindow(dsply, _window);
        setWMState(IconicState);
        check_focus(get_prev_focused());
    }
}

void
Client::unhide() noexcept {
    if (hidden) {
        hidden = false;
        topmost_client = sharedReference();
        XMapWindow(dsply, _window);
        XMapRaised(dsply, _frame);
        setWMState(NormalState);
    }
}

void hide(ClientPointer c) {
	if (c ) {
        c->hide();
	}
}

void unhide(ClientPointer c) {
	if (c ) {
        c->unhide();
	}
}

void toggle_fullscreen(ClientPointer c)
{
	int xoffset, yoffset, maxwinwidth, maxwinheight;
	if (c  && !c->getTrans()) {
		if (c == fullscreen_client) { // reset to original size
			c->x = fs_prevdims.getX();
			c->y = fs_prevdims.getY();
			c->width = fs_prevdims.getWidth();
			c->height = fs_prevdims.getHeight();
			XMoveResizeWindow(dsply, c->getFrame(), c->x, c->y - BARHEIGHT(), c->width, c->height + BARHEIGHT());
			XMoveResizeWindow(dsply, c->getWindow(), 0, BARHEIGHT(), c->width, c->height);
            c->sendConfig();
			fullscreen_client = nullptr;
			showing_taskbar = 1;
		} else { // make fullscreen
			xoffset = yoffset = 0;
			maxwinwidth = DisplayWidth(dsply, screen);
			maxwinheight = DisplayHeight(dsply, screen) - BARHEIGHT();
			if (fullscreen_client ) { // reset existing fullscreen window to original size
				fullscreen_client->x = fs_prevdims.getX();
				fullscreen_client->y = fs_prevdims.getY();
				fullscreen_client->width = fs_prevdims.getWidth();
				fullscreen_client->height = fs_prevdims.getHeight();
				XMoveResizeWindow(dsply, fullscreen_client->getFrame(), fullscreen_client->x, fullscreen_client->y - BARHEIGHT(), fullscreen_client->width, fullscreen_client->height + BARHEIGHT());
				XMoveResizeWindow(dsply, fullscreen_client->getWindow(), 0, BARHEIGHT(), fullscreen_client->width, fullscreen_client->height);
                fullscreen_client->sendConfig();
			}
            fs_prevdims.become(c->x, c->y, c->width, c->height);
			c->x = 0 - BORDERWIDTH(c);
			c->y = BARHEIGHT() - BORDERWIDTH(c);
			c->width = maxwinwidth;
			c->height = maxwinheight;
			if (c->getSize()->flags & PMaxSize || c->getSize()->flags & PResizeInc) {
				if (c->getSize()->flags & PResizeInc) {
					Rect maxwinsize { xoffset, yoffset, maxwinwidth, maxwinheight };
					get_incsize(c, (unsigned int *)&c->getSize()->max_width, (unsigned int *)&c->getSize()->max_height, &maxwinsize, PIXELS);
				}
				if (c->getSize()->max_width < maxwinwidth) {
					c->width = c->getSize()->max_width;
					xoffset = (maxwinwidth - c->width) / 2;
				}
				if (c->getSize()->max_height < maxwinheight) {
					c->height = c->getSize()->max_height;
					yoffset = (maxwinheight - c->height) / 2;
				}
			}
			XMoveResizeWindow(dsply, c->getFrame(), c->x, c->y, maxwinwidth, maxwinheight);
			XMoveResizeWindow(dsply, c->getWindow(), xoffset, yoffset, c->width, c->height);
            c->sendConfig();
			fullscreen_client = c;
			showing_taskbar = in_taskbar;
		}
        Taskbar::instance().redraw();
	}
}

/* The name of this function is a bit misleading: if the client
 * doesn't listen to WM_DELETE then we just terminate it with extreme
 * prejudice. */

void send_wm_delete(ClientPointer c)
{
	int i, n, found = 0;
	Atom *protocols;

	if (XGetWMProtocols(dsply, c->getWindow(), &protocols, &n)) {
		for (i = 0; i < n; i++) {
			if (protocols[i] == wm_delete) {
				found++;
			}
		}
		XFree(protocols);
	}
	if (found) {
		send_xmessage(c->getWindow(), wm_protos, wm_delete);
	} else {
		XKillClient(dsply, c->getWindow());
	}
}
void
Client::move() noexcept {
	XEvent ev;
	int old_cx = x;
	int old_cy = y;
	Rect bounddims;
	XSetWindowAttributes pattr;

	int dw = DisplayWidth(dsply, screen);
	int dh = DisplayHeight(dsply, screen);
    auto [mousex, mousey] = getMousePosition();
	bounddims.setX((mousex - x) - BORDERWIDTH(this));
	bounddims.setWidth((dw - bounddims.getX() - (width - bounddims.getX())) + 1);
	bounddims.setY(mousey - y);
	bounddims.setHeight((dh - bounddims.getY() - (height - bounddims.getY())) + 1);
	bounddims.addToY((BARHEIGHT() * 2) - BORDERWIDTH(this));
	bounddims.addToHeight(height - ((BARHEIGHT() * 2) - DEF_BORDERWIDTH));

    auto constraint_win = createWindow(dsply, root, bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, &pattr);
#ifdef DEBUG
    std::cerr << "Client::move() : constraint_win is (" << bounddims.getX() << ", " << bounddims.getY() << ")-(" << (bounddims.getX() + bounddims.getWidth()) << ", " << (bounddims.getY() + bounddims.getHeight()) << ")" << std::endl;
#endif
	XMapWindow(dsply, constraint_win);

	if (!(XGrabPointer(dsply, root, False, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, None, CurrentTime) == GrabSuccess)) {
		XDestroyWindow(dsply, constraint_win);
		return;
	}

	do
	{
		XMaskEvent(dsply, ExposureMask|MouseMask, &ev);
		switch (ev.type) {
			case Expose:
				if (ClientPointer exposed_c = find_client(ev.xexpose.window, FRAME); exposed_c) {
                    exposed_c->redraw();
				}
				break;
			case MotionNotify:
				x = old_cx + (ev.xmotion.x - mousex);
				y = old_cy + (ev.xmotion.y - mousey);
				XMoveWindow(dsply, _frame, x, y - BARHEIGHT());
                sendConfig();
				break;
		}
	} while (ev.type != ButtonRelease);

	ungrab();
	XDestroyWindow(dsply, constraint_win);
}

void resize(ClientPointer c, int x, int y)
{
	XEvent ev;
	ClientPointer exposed_c;
	Window resize_win, resizebar_win;
	XSetWindowAttributes pattr, resize_pattr, resizebar_pattr;

    // inside the window, dragging outwards : TRUE
    // outside the window, dragging inwards : FALSE
    bool dragging_outwards = x > c->x + BORDERWIDTH(c) && x < (c->x + c->width) - BORDERWIDTH(c) && y > (c->y - BARHEIGHT()) + BORDERWIDTH(c) && y < (c->y + c->height) - BORDERWIDTH(c);

	unsigned int dw = DisplayWidth(dsply, screen);
	unsigned int dh = DisplayHeight(dsply, screen);

    Rect bounddims { 0, 0, static_cast<int>(dw), static_cast<int>(dh) };

	auto constraint_win = createWindow(dsply, root, bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, &pattr);
	XMapWindow(dsply, constraint_win);

	if (!(XGrabPointer(dsply, root, False, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, resize_curs, CurrentTime) == GrabSuccess)) {
		XDestroyWindow(dsply, constraint_win);
		return;
	}
    Rect newdims { c->x, c->y - BARHEIGHT(), c->width, c->height + BARHEIGHT() };
    Rect recalceddims(newdims);

	// create and map resize window
	resize_pattr.override_redirect = True;
	resize_pattr.background_pixel = menu_col.pixel;
	resize_pattr.border_pixel = border_col.pixel;
	resize_pattr.event_mask = ChildMask|ButtonPressMask|ExposureMask|EnterWindowMask;
    resize_win = createWindow(dsply, root, newdims, DEF_BORDERWIDTH, DefaultDepth(dsply, screen), CopyFromParent, DefaultVisual(dsply, screen), CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWEventMask, &resize_pattr);
	XMapRaised(dsply, resize_win);

	resizebar_pattr.override_redirect = True;
	resizebar_pattr.background_pixel = active_col.pixel;
	resizebar_pattr.border_pixel = border_col.pixel;
	resizebar_pattr.event_mask = ChildMask|ButtonPressMask|ExposureMask|EnterWindowMask;
	resizebar_win = XCreateWindow(dsply, resize_win, -DEF_BORDERWIDTH, -DEF_BORDERWIDTH, newdims.getWidth(), BARHEIGHT() - DEF_BORDERWIDTH, DEF_BORDERWIDTH, DefaultDepth(dsply, screen), CopyFromParent, DefaultVisual(dsply, screen), CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWEventMask, &resizebar_pattr);
	XMapRaised(dsply, resizebar_win);

	// temporarily swap drawables in order to draw on the resize window's XFT context
	XftDrawChange(c->getXftDraw(), (Drawable) resizebar_win);

	// hide real window's frame
	XUnmapWindow(dsply, c->getFrame());

	do {
		XMaskEvent(dsply, ExposureMask|MouseMask, &ev);
		switch (ev.type) {
			case Expose:
				if (ev.xexpose.window == resizebar_win) {
                    c->writeTitleText(resizebar_win);
				} else {
					exposed_c = find_client(ev.xexpose.window, FRAME);
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
							XMoveResizeWindow(dsply, constraint_win, bounddims.getX(), bounddims.getY(), bounddims.getWidth(), bounddims.getHeight());
							in_taskbar = 0;
						}
						// inside the window, dragging outwards
						if (dragging_outwards) {
							if (ev.xmotion.x < newdims.getX() + BORDERWIDTH(c)) {
								newdims.addToWidth(newdims.getX() + BORDERWIDTH(c) - ev.xmotion.x);
								newdims.setX(ev.xmotion.x - BORDERWIDTH(c));
								leftedge_changed = 1;
							} else if (ev.xmotion.x > newdims.getX() + newdims.getWidth() + BORDERWIDTH(c)) {
								newdims.setWidth((ev.xmotion.x - newdims.getX() - BORDERWIDTH(c)) + 1); // add 1 to allow window to be flush with edge of screen
								rightedge_changed = 1;
							}
							if (ev.xmotion.y < newdims.getY() + BORDERWIDTH(c)) {
								newdims.addToHeight(newdims.getY() + BORDERWIDTH(c) - ev.xmotion.y);
								newdims.setY(ev.xmotion.y - BORDERWIDTH(c));
								topedge_changed = 1;
							} else if (ev.xmotion.y > newdims.getY() + newdims.getHeight()+ BORDERWIDTH(c))
							{
								newdims.setHeight((ev.xmotion.y - newdims.getY() - BORDERWIDTH(c)) + 1); // add 1 to allow window to be flush with edge of screen
								bottomedge_changed = 1;
							}
						} else { // outside the window, dragging inwards
							unsigned int above_win = (ev.xmotion.y < newdims.getY() + BORDERWIDTH(c));
							unsigned int below_win = (ev.xmotion.y > newdims.getY() + newdims.getHeight()+ BORDERWIDTH(c));
							unsigned int leftof_win = (ev.xmotion.x < newdims.getX() + BORDERWIDTH(c));
							unsigned int rightof_win = (ev.xmotion.x > newdims.getX() + newdims.getWidth()+ BORDERWIDTH(c));

							unsigned int in_win = ((!above_win) && (!below_win) && (!leftof_win) && (!rightof_win));

							if (in_win) {
								unsigned int from_left = ev.xmotion.x - newdims.getX() - BORDERWIDTH(c);
								unsigned int from_right = newdims.getX() + newdims.getWidth() + BORDERWIDTH(c) - ev.xmotion.x;
								unsigned int from_top = ev.xmotion.y - newdims.getY() - BORDERWIDTH(c);
								unsigned int from_bottom = newdims.getY() + newdims.getHeight() + BORDERWIDTH(c) - ev.xmotion.y;
								if (from_left < from_right && from_left < from_top && from_left < from_bottom) {
                                    newdims.subtractFromWidth(ev.xmotion.x - newdims.getX() - BORDERWIDTH(c));
                                    newdims.setX(ev.xmotion.x - BORDERWIDTH(c));
									leftedge_changed = 1;
								} else if (from_right < from_top && from_right < from_bottom) {
                                    newdims.setWidth(ev.xmotion.x - newdims.getX() - BORDERWIDTH(c));
									rightedge_changed = 1;
								} else if (from_top < from_bottom) {
									newdims.subtractFromHeight(ev.xmotion.y - newdims.getY() - BORDERWIDTH(c));
									newdims.setY(ev.xmotion.y - BORDERWIDTH(c));
									topedge_changed = 1;
								} else {
                                    newdims.setHeight( ev.xmotion.y - newdims.getY() - BORDERWIDTH(c));
									bottomedge_changed = 1;
								}
							}
						}
						// coords have changed
						if (leftedge_changed || rightedge_changed || topedge_changed || bottomedge_changed) {
                            recalceddims = newdims;
							recalceddims.subtractFromHeight(BARHEIGHT());

							if (get_incsize(c, (unsigned int *)&newwidth, (unsigned int *)&newheight, &recalceddims, PIXELS)) {
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
							limit_size(c, &recalceddims);

							XMoveResizeWindow(dsply, resize_win, recalceddims.getX(), recalceddims.getY(), recalceddims.getWidth(), recalceddims.getHeight());
							XResizeWindow(dsply, resizebar_win, recalceddims.getWidth(), BARHEIGHT() - DEF_BORDERWIDTH);
						}
					}
				}
				break;
		}
	} while (ev.type != ButtonRelease);

	XUngrabServer(dsply);
	ungrab();
	c->x = recalceddims.getX();
	c->y = recalceddims.getY() + BARHEIGHT();
	c->width = recalceddims.getWidth();
	c->height = recalceddims.getHeight() - BARHEIGHT();

	XMoveResizeWindow(dsply, c->getFrame(), c->x, c->y - BARHEIGHT(), c->width, c->height + BARHEIGHT());
	XResizeWindow(dsply, c->getWindow(), c->width, c->height);

	// unhide real window's frame
	XMapWindow(dsply, c->getFrame());

	XSetInputFocus(dsply, c->getWindow(), RevertToNone, CurrentTime);

    c->sendConfig();
	XDestroyWindow(dsply, constraint_win);

	// reset the drawable
	XftDrawChange(c->getXftDraw(), (Drawable) c->getFrame());
	
	XDestroyWindow(dsply, resizebar_win);
	XDestroyWindow(dsply, resize_win);
}

static void limit_size(ClientPointer c, Rect *newdims)
{
	auto dw = DisplayWidth(dsply, screen);
	auto dh = DisplayHeight(dsply, screen);

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
