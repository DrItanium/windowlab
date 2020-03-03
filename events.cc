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

#include <X11/Xatom.h>
#include "windowlab.h"

static void handle_key_press(XKeyEvent *);
static void handle_button_press(XButtonEvent *);
static void handle_windowbar_click(XButtonEvent *, ClientPointer );
static void handle_configure_request(XConfigureRequestEvent *);
static void handle_map_request(XMapRequestEvent *);
static void handle_unmap_event(XUnmapEvent *);
static void handle_destroy_event(XDestroyWindowEvent *);
static void handle_client_message(XClientMessageEvent *);
static void handle_property_change(XPropertyEvent *);
static void handle_enter_event(XCrossingEvent *);
static void handle_colormap_change(XColormapEvent *);
static void handle_expose_event(XExposeEvent *);
static void handle_shape_change(XShapeEvent *);

static int interruptible_XNextEvent(XEvent *event);

/* We may want to put in some sort of check for unknown events at some
 * point. TWM has an interesting and different way of doing this... */

void doEventLoop()
{
	XEvent ev;

	for (;;) {
		interruptible_XNextEvent(&ev);
        if constexpr (debugActive()) {
            showEvent(ev);
        }
		/* check to see if menu rebuild has been requested */
        if (Menu::instance().shouldRepopulate()) {
            Menu::instance().populate();
        }
		switch (ev.type) {
			case KeyPress:
				handle_key_press(&ev.xkey);
				break;
			case ButtonPress:
				handle_button_press(&ev.xbutton);
				break;
			case ConfigureRequest:
				handle_configure_request(&ev.xconfigurerequest);
				break;
			case MapRequest:
				handle_map_request(&ev.xmaprequest);
				break;
			case UnmapNotify:
				handle_unmap_event(&ev.xunmap);
				break;
			case DestroyNotify:
				handle_destroy_event(&ev.xdestroywindow);
				break;
			case ClientMessage:
				handle_client_message(&ev.xclient);
				break;
			case ColormapNotify:
				handle_colormap_change(&ev.xcolormap);
				break;
			case PropertyNotify:
				handle_property_change(&ev.xproperty);
				break;
			case EnterNotify:
				handle_enter_event(&ev.xcrossing);
				break;
			case Expose:
				handle_expose_event(&ev.xexpose);
				break;
			default:
				if (shape && ev.type == shape_event) {
					handle_shape_change((XShapeEvent *)&ev);
				}
		}
	}
}

static void handle_key_press(XKeyEvent *e)
{
    auto& taskbar = Taskbar::instance();
    auto& clients = ClientTracker::instance();
    auto& dm = DisplayManager::instance();
    auto key = dm.keycodeToKeysym(e->keycode);
	switch (key) {
		case KEY_CYCLEPREV:
            taskbar.cyclePrevious();
			break;
		case KEY_CYCLENEXT:
            taskbar.cycleNext();
			break;
		case KEY_FULLSCREEN:
			toggle_fullscreen(clients.getFocusedClient());
			break;
		case KEY_TOGGLEZ:
            clients.getFocusedClient()->raiseLower();
			break;
	}
}

/* Someone clicked a button. If it was on the root, we get the click
 * by default. If it's on a window frame, we get it as well. If it's
 * on a client window, it may still fall through to us if the client
 * doesn't select for mouse-click events. */

static void handle_button_press(XButtonEvent *e)
{
    auto& taskbar = Taskbar::instance();
    auto& clients = ClientTracker::instance();
    auto& dm = DisplayManager::instance();
	if (e->state & MODIFIER) {
		if (clients.hasFocusedClient() && clients.getFocusedClient() != clients.getFullscreenClient()) {
            clients.getFocusedClient()->resize(e->x_root, e->y_root);
		} else {
			// pass event on
            dm.allowEvents(ReplayPointer, CurrentTime);
		}
	} else if (e->window == dm.getRoot()) {
        if constexpr (debugActive()) {
            clients.dump();
        }
		if (e->button == Button3) {
            taskbar.rightClickRoot();
		}
	} else if (e->window == taskbar.getWindow()) {
		switch (e->button) {
			case Button1: // left mouse button
                taskbar.leftClick(e->x);
				break;
			case Button3: // right mouse button
                taskbar.rightClick(e->x);
				break;
			case Button4: // mouse wheel up
                taskbar.cyclePrevious();
				break;
			case Button5: // mouse wheel down
                taskbar.cycleNext();
				break;
		}
	} else {
		// pass event on
        dm.allowEvents(ReplayPointer, CurrentTime);
		if (e->button == Button1) {
			ClientPointer c = clients.find(e->window, FRAME);
			if (c) {
				// click-to-focus
                clients.checkFocus(c);
                if (e->y < BARHEIGHT() && c != clients.getFullscreenClient()) {
                    handle_windowbar_click(e, c);
                }
			}
		} else if (e->button == Button3) {
            taskbar.rightClickRoot();
		}
	}
}

static void handle_windowbar_click(XButtonEvent *e, ClientPointer c)
{
	static ClientPointer  first_click_c;
	static Time first_click_time;
	XEvent ev;
    auto& dm = DisplayManager::instance();

    if (unsigned int in_box_down = c->boxClicked(e->x); in_box_down <= 2) {
		if (!dm.grab(MouseMask, None)) {
			return;
		}

        dm.grabServer();

		unsigned int in_box = 1;
        unsigned int in_box_up = 0;

		c->drawButton(&text_gc, &depressed_gc, in_box_down);
		do
		{
            dm.maskEvent(MouseMask, ev);
            in_box_up = c->boxClicked(ev.xbutton.x - (c->getX() + DEF_BORDERWIDTH));
			int win_ypos = (ev.xbutton.y - c->getY()) + BARHEIGHT();
			if (ev.type == MotionNotify) {
				if ((win_ypos <= BARHEIGHT()) && (win_ypos >= DEF_BORDERWIDTH) && (in_box_up == in_box_down)) {
					in_box = 1;
					c->drawButton(&text_gc, &depressed_gc, in_box_down);
				} else {
					in_box = 0;
					c->drawButton(&text_gc, &active_gc, in_box_down);
				}
			}
		} while (ev.type != ButtonRelease);
        c->drawButton(&text_gc, &active_gc, in_box_down);

        dm.ungrabServer();
        dm.ungrab();
		if (in_box) {
            if (c) {
                switch (in_box_up) {
                    case 0:
                        c->sendWMDelete();
                        break;
                    case 1:
                        c->raiseLower();
                        break;
                    case 2:
                        c->hide();
                        break;
                }
            }
		}
	} else if (in_box_down != UINT_MAX) {
		if (first_click_c == c && (e->time - first_click_time) < DEF_DBLCLKTIME) {
            if (c) {
                c->raiseLower();
            }
			first_click_c = nullptr; // prevent 3rd clicks counting as double clicks
		} else {
			first_click_c = c;
		}
		first_click_time = e->time;
        c->move();
	}
}

unsigned int
Client::boxClicked(int x) const noexcept {
    if (int pixFromRight = _width - x; pixFromRight < 0) {
        return std::numeric_limits<unsigned int>::max(); // outside window
    } else {
        return (pixFromRight / (BARHEIGHT() - DEF_BORDERWIDTH));
    }
}
void
Client::drawButton(GC *detail_gc, GC *background_gc, unsigned int which_box) noexcept {
	switch (which_box) {
		case 0:
            drawCloseButton(detail_gc, background_gc);
			break;
		case 1:
            drawToggleDepthButton(detail_gc, background_gc);
			break;
		case 2:
            drawHideButton(detail_gc, background_gc);
			break;
	}
}

/* Because we are redirecting the root window, we get ConfigureRequest
 * events from both clients we're handling and ones that we aren't.
 * For clients we manage, we need to fiddle with the frame and the
 * client window, and for unmanaged windows we have to pass along
 * everything unchanged. Thankfully, we can reuse (a) the
 * XWindowChanges struct and (b) the code to configure the client
 * window in both cases.
 *
 * Most of the assignments here are going to be garbage, but only the
 * ones that are masked in by e->value_mask will be looked at by the X
 * server.
 *
 * We ignore managed clients that want their z-order changed and
 * managed fullscreen clients that want their size and/or position
 * changed (except to update their size and/or position for when
 * fullscreen mode is toggled off). From what I can remember, clients
 * are supposed to have been written so that they are aware that their
 * requirements may not be met by the window manager. */

static void handle_configure_request(XConfigureRequestEvent *e) {
    auto& ctracker = ClientTracker::instance();
    auto& dm = DisplayManager::instance();
	ClientPointer c = ctracker.find(e->window, WINDOW);
	XWindowChanges wc;

	if (ctracker.hasFullscreenClient() && c == ctracker.getFullscreenClient()) {
        fs_prevdims.setX(e->x, [e](int) { return e->value_mask & CWX; });
        fs_prevdims.setY(e->y, [e](int) { return e->value_mask & CWY; });
        fs_prevdims.setWidth(e->width, [e](int) { return e->value_mask & CWWidth; });
        fs_prevdims.setHeight(e->height, [e](int) { return e->value_mask & CWHeight; });
		return;
	}

	if (c) {
        c->gravitate(REMOVE_GRAVITY);
		if (e->value_mask & CWX) {
            c->setX(e->x);
		}
		if (e->value_mask & CWY) {
            c->setY(e->y);
		}
		if (e->value_mask & CWWidth) {
            c->setWidth(e->width);
		}
		if (e->value_mask & CWHeight) {
            c->setHeight(e->height);
		}
        c->refixPosition(e);
        c->gravitate(APPLY_GRAVITY);
		// configure the frame
		wc.x = c->getX();
		wc.y = c->getY() - BARHEIGHT();
		wc.width = c->getWidth();
		wc.height = c->getHeight()+ BARHEIGHT();
		wc.border_width = DEF_BORDERWIDTH;
		//wc.sibling = e->above;
		//wc.stack_mode = e->detail;
        dm.configureWindow(c->getFrame(), e->value_mask, wc);
		if (e->value_mask & (CWWidth|CWHeight)) {
            c->setShape();
		}
		c->sendConfig();
		// start setting up the next call
		wc.x = 0;
		wc.y = BARHEIGHT();
	} else {
		wc.x = e->x;
		wc.y = e->y;
	}

	wc.width = e->width;
	wc.height = e->height;
	//wc.sibling = e->above;
	//wc.stack_mode = e->detail;
    dm.configureWindow(e->window, e->value_mask, wc);
}

/* Two possibilities if a client is asking to be mapped. One is that
 * it's a new window, so we handle that if it isn't in our clients
 * list anywhere. The other is that it already exists and wants to
 * de-iconify, which is simple to take care of. */

static void handle_map_request(XMapRequestEvent *e) {
	ClientPointer c = ClientTracker::instance().find(e->window, WINDOW);
	if (c) {
        c->unhide();
	} else {
        Client::makeNew(e->window);
	}
}

/* See windowlab.h for the intro to this one. If this is a window we
 * unmapped ourselves, decrement c->ignore_unmap and casually go on as
 * if nothing had happened. If the window unmapped itself from under
 * our feet, however, get rid of it.
 *
 * If you spend a lot of time with -DDEBUG on, you'll realize that
 * because most clients unmap and destroy themselves at once, they're
 * gone before we even get the Unmap event, never mind the Destroy
 * one. This will necessitate some extra caution in remove_client.
 *
 * Personally, I think that if Map events are intercepted, Unmap
 * events should be intercepted too. No use arguing with a standard
 * that's almost as old as I am though. :-( */

static void handle_unmap_event(XUnmapEvent *e) {
	if (ClientPointer c = ClientTracker::instance().find(e->window, WINDOW); c) {
        if (c->getIgnoreUnmap()) {
            c->decrementIgnoreUnmap();
		} else {
            ClientTracker::instance().remove(c, WITHDRAW);
		}
	}
}

/* This happens when a window is iconified and destroys itself. An
 * Unmap event wouldn't happen in that case because the window is
 * already unmapped. */

static void handle_destroy_event(XDestroyWindowEvent *e) {
    auto& ct = ClientTracker::instance();
	if (auto c = ct.find(e->window, WINDOW); c) {
        ct.remove(c, WITHDRAW);
	}
}

/* If a client wants to iconify itself (boo! hiss!) it must send a
 * special kind of ClientMessage. We might set up other handlers here
 * but there's nothing else required by the ICCCM. */

static void handle_client_message(XClientMessageEvent *e) {
	if (auto c = ClientTracker::instance().find(e->window, WINDOW); c && e->message_type == wm_change_state && e->format == 32 && e->data.l[0] == IconicState) {
        c->hide();
	}
}

/* All that we have cached is the name and the size hints, so we only
 * have to check for those here. A change in the name means we have to
 * immediately wipe out the old name and redraw; size hints only get
 * used when we need them. */

static void handle_property_change(XPropertyEvent *e) {
	if (ClientPointer c = ClientTracker::instance().find(e->window, WINDOW); c) {
        auto& dm = DisplayManager::instance();
		switch (e->atom) {
            case XA_WM_NAME: {
                                 auto [ status, opt ] = fetchName(dm.getDisplay(), c->getWindow());
                                 (void)status; // status isn't actually used but is returned in the tuple
                                 c->setName(opt);
                                 c->redraw();
                                 Taskbar::performRedraw();
                                 break;
                             }
            case XA_WM_NORMAL_HINTS: {
                                         long dummy = 0;
                                         XGetWMNormalHints(dm.getDisplay(), c->getWindow(), c->getSize(), &dummy);
                                         break;
                                     }
		}
	}
}

/* X's default focus policy is follows-mouse, but we have to set it
 * anyway because some sloppily written clients assume that (a) they
 * can set the focus whenever they want or (b) that they don't have
 * the focus unless the keyboard is grabbed to them. OTOH it does
 * allow us to keep the previous focus when pointing at the root,
 * which is nice.
 *
 * We also implement a colormap-follows-mouse policy here. That, on
 * the third hand, is *not* X's default. */

static void handle_enter_event(XCrossingEvent *e) {
	if (auto& taskbar = Taskbar::instance(); e->window == taskbar.getWindow()) {
        taskbar.setInsideTaskbar(true);
		if (!taskbar.showingTaskbar()) {
            taskbar.setShowingTaskbar(true);
            taskbar.redraw();
		}
	} else {
        auto& ctracker = ClientTracker::instance();
        taskbar.setInsideTaskbar(false);
        if (ctracker.hasFullscreenClient()) {
            if (taskbar.showingTaskbar()) {
                taskbar.setShowingTaskbar(false);
                taskbar.redraw();
			}
		} else { // no fullscreen client
			if (!taskbar.showingTaskbar()) {
                taskbar.setShowingTaskbar(true);
                taskbar.redraw();
			}
		}

		if (ClientPointer c = ctracker.find(e->window, FRAME); c) {
            auto& dm = DisplayManager::instance();
			XGrabButton(dm.getDisplay(), AnyButton, AnyModifier, c->getFrame(), False, ButtonMask, GrabModeSync, GrabModeSync, None, None);
		}
	}
}

/* Here's part 2 of our colormap policy: when a client installs a new
 * colormap on itself, set the display's colormap to that. Arguably,
 * this is bad, because we should only set the colormap if that client
 * has the focus. However, clients don't usually set colormaps at
 * random when you're not interacting with them, so I think we're
 * safe. If you have an 8-bit display and this doesn't work for you,
 * by all means yell at me, but very few people have 8-bit displays
 * these days. */

static void handle_colormap_change(XColormapEvent *e) {
	if (ClientPointer c = ClientTracker::instance().find(e->window, WINDOW); c  && e->c_new) { // use c_new for c++
        auto& dm = DisplayManager::instance();
        c->setColormap(e->colormap);
        dm.installColormap(c->getColormap());
	}
}

/* If we were covered by multiple windows, we will usually get
 * multiple expose events, so ignore them unless e->count (the number
 * of outstanding exposes) is zero. */

static void handle_expose_event(XExposeEvent *e) {
	if (auto& taskbar = Taskbar::instance(); e->window == taskbar.getWindow()) {
		if (e->count == 0) {
            taskbar.redraw();
		}
	} else {
		if (ClientPointer c = ClientTracker::instance().find(e->window, FRAME); c  && e->count == 0) {
            c->redraw();
		}
	}
}

static void handle_shape_change(XShapeEvent *e) {
	if (ClientPointer c = ClientTracker::instance().find(e->window, WINDOW); c) {
        c->setShape();
	}
}

/* interruptibleXNextEvent() was originally taken from Blender's source code
 * and came with the following copyright notice: */

/* Copyright (c) Mark J. Kilgard, 1994, 1995, 1996. */

/* This program is freely distributable without licensing fees
 * and is provided without guarantee or warrantee expressed or
 * implied. This program is -not- in the public domain. */

/* Unlike XNextEvent, if a signal arrives, interruptibleXNextEvent will
 * return zero. */

static int interruptible_XNextEvent(XEvent *event) {
    auto& dm = DisplayManager::instance();
    for (int dsply_fd = dm.connectionNumber();;) {
        if (dm.pending()) {
            dm.nextEvent(event);
			return 1;
		}
        fd_set fds;
		FD_ZERO(&fds);
		FD_SET(dsply_fd, &fds);
		if (int rc = select(dsply_fd + 1, &fds, nullptr, nullptr, nullptr); rc < 0) {
			if (errno == EINTR) {
				return 0;
			}
			return 1;
		}
	}
}
