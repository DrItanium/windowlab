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
#include <iostream>



Taskbar&
Taskbar::instance() noexcept {
    static Taskbar _bar;
    if (!_bar._made) {
        _bar.make();
    }
    return _bar;
}
void
Taskbar::make() noexcept {
    if (_made) {
        return;
    }
	XSetWindowAttributes pattr;
    auto& dm = DisplayManager::instance();
	pattr.override_redirect = True;
	pattr.background_pixel = empty_col.pixel;
	pattr.border_pixel = border_col.pixel;
	pattr.event_mask = ChildMask|ButtonPressMask|ExposureMask|EnterWindowMask;
	_taskbar = XCreateWindow(dm.getDisplay(), dm.getRoot(), 0 - DEF_BORDERWIDTH, 0 - DEF_BORDERWIDTH, DisplayWidth(DisplayManager::instance().getDisplay(), DisplayManager::instance().getScreen()), BARHEIGHT() - DEF_BORDERWIDTH, DEF_BORDERWIDTH, DefaultDepth(DisplayManager::instance().getDisplay(), DisplayManager::instance().getScreen()), CopyFromParent, DefaultVisual(DisplayManager::instance().getDisplay(), DisplayManager::instance().getScreen()), CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWEventMask, &pattr);

	XMapWindow(dm.getDisplay(), _taskbar);

	_tbxftdraw = XftDrawCreate(dm.getDisplay(), 
            (Drawable) _taskbar, 
            DefaultVisual(dm.getDisplay(), DefaultScreen(dm.getDisplay())), 
            DefaultColormap(dm.getDisplay(), DefaultScreen(dm.getDisplay())));
    _made = true;
}

bool
ClientTracker::accept(std::function<bool(ClientPointer)> fn) {
    auto stop = false;
    for (auto & c : _clients) {
        stop = fn(c);
        if (stop) {
            break;
        }
    }
    return stop;
}

void
Client::rememberHidden() noexcept {
    _wasHidden = _hidden;
}


void
Client::forgetHidden() noexcept {
    _wasHidden = (sharedReference() == ClientTracker::instance().getFocusedClient()) ? _hidden : false;
}

void 
lclick_taskbutton(ClientPointer old_c, ClientPointer c) {
	if (old_c) {
		if (old_c->wasHidden()) {
            old_c->hide();
		}
	}
    if (c) {
        if (c->isHidden()) {
            c->unhide();
        } else {
            if (c->wasHidden()) {
                c->hide();
            } else {
                c->raiseLower();
            }
        }
        ClientTracker::instance().checkFocus(c);
    }
}

void
Taskbar::leftClick(int x) {

    auto& ctracker = ClientTracker::instance();
    auto& dm = DisplayManager::instance();
	if (!ctracker.empty()) {
        XSetWindowAttributes pattr;
        ctracker.accept([](ClientPointer p) { p->rememberHidden(); return false; });

        // unused?
        //auto [mousex, mousey] = getMousePosition();
        Rect bounddims {0, 0, dm.getWidth(), BARHEIGHT() };

		auto constraint_win = dm.createWindow(bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, pattr);
		XMapWindow(dm.getDisplay(), constraint_win);

		if (!(XGrabPointer(dm.getDisplay(), dm.getRoot(), False, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, None, CurrentTime) == GrabSuccess)) {
			XDestroyWindow(dm.getDisplay(), constraint_win);
			return;
		}

        auto buttonWidth = getButtonWidth();

		auto button_clicked = (unsigned int)(x / buttonWidth);
        auto c = ctracker.at(button_clicked);

		lclick_taskbutton(nullptr, c);
        XEvent ev;
		do {
			XMaskEvent(dm.getDisplay(), ExposureMask|MouseMask|KeyMask, &ev);
			switch (ev.type) {
                case Expose: {
                                 if (auto exposed_c = ctracker.find(ev.xexpose.window, FRAME); exposed_c) {
                                     exposed_c->redraw();
                                 }
                                 break;
                             }
				case MotionNotify: {
                                       auto old_button_clicked = button_clicked;
                                       button_clicked = (unsigned int)(ev.xmotion.x / buttonWidth);
                                       if (button_clicked != old_button_clicked) {
                                           auto old_c = c;
                                           c = ctracker.at(button_clicked);
                                           lclick_taskbutton(old_c, c);
                                       }
                                       break;
                                   }
				case KeyPress:
                    dm.putbackEvent(ev);
					break;
			}
		} while (ev.type != ButtonPress && ev.type != ButtonRelease && ev.type != KeyPress);

		XUnmapWindow(dm.getDisplay(), constraint_win);
		XDestroyWindow(dm.getDisplay(), constraint_win);
		ungrab();

        ctracker.accept([](ClientPointer p) { p->forgetHidden(); return false; });
	}
}

void
Taskbar::rightClick(int x) {
	XEvent ev;
	unsigned int current_item = UINT_MAX;
	XSetWindowAttributes pattr;
    auto& dm = DisplayManager::instance();

	//auto [mousex, mousey] = getMousePosition();
	Rect bounddims { 0, 0, dm.getWidth(), BARHEIGHT() };

	auto constraint_win = dm.createWindow(bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, pattr);
    dm.mapWindow(constraint_win);

	if (!(XGrabPointer(dm.getDisplay(), dm.getRoot(), False, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, None, CurrentTime) == GrabSuccess)) {
		XDestroyWindow(dm.getDisplay(), constraint_win);
		return;
	}
    drawMenubar();
    updateMenuItem(INT_MAX); // force initial highlight
	current_item = updateMenuItem(x);
	do {
		XMaskEvent(dm.getDisplay(), MouseMask|KeyMask, &ev);
		switch (ev.type)
		{
			case MotionNotify:
				current_item = updateMenuItem(ev.xmotion.x);
				break;
			case ButtonRelease:
				if (current_item != UINT_MAX) {
                    if (auto item = Menu::instance().at(current_item); item) {
                        item->forkExec();
                    }
				}
				break;
			case KeyPress:
                dm.putbackEvent(ev);
				break;
		}
	} while (ev.type != ButtonPress && ev.type != ButtonRelease && ev.type != KeyPress);

    Taskbar::instance().redraw();
	XUnmapWindow(dm.getDisplay(), constraint_win);
	XDestroyWindow(dm.getDisplay(), constraint_win);
	ungrab();
}

void
Taskbar::rightClickRoot() {
    auto& dm = DisplayManager::instance();
	if (!grab(dm.getRoot(), MouseMask, None)) {
		return;
	}
	drawMenubar();
	XEvent ev;
	do {
		XMaskEvent(dm.getDisplay(), MouseMask|KeyMask, &ev);
		switch (ev.type) {
			case MotionNotify:
				if (ev.xmotion.y < BARHEIGHT()) {
					ungrab();
                    rightClick(ev.xmotion.x);
					return;
				}
				break;
			case KeyPress:
                dm.putbackEvent(ev);
				break;
		}
	} while (ev.type != ButtonRelease && ev.type != KeyPress);

    redraw();
	ungrab();
}

void
Taskbar::redraw() {
	auto buttonWidth = getButtonWidth();
    auto& dm = DisplayManager::instance();
	XClearWindow(dm.getDisplay(), _taskbar);

	if (showing_taskbar == 0) {
		return;
	}

	unsigned int i = 0;
    ClientTracker::instance().accept([this, &i, buttonWidth](ClientPointer c) {
                auto& dm = DisplayManager::instance();
                auto& ct = ClientTracker::instance();
		        auto button_startx = static_cast<int>(i * buttonWidth);
		        auto button_iwidth = static_cast<unsigned int>(((i + 1) * buttonWidth) - button_startx);
		        if (button_startx != 0) {
		        	XDrawLine(dm.getDisplay(), _taskbar, border_gc, button_startx - 1, 0, button_startx - 1, BARHEIGHT() - DEF_BORDERWIDTH);
		        }
		        if (c == ct.getFocusedClient()) {
		        	XFillRectangle(dm.getDisplay(), _taskbar, active_gc, button_startx, 0, button_iwidth, BARHEIGHT() - DEF_BORDERWIDTH);
		        } else {
		        	XFillRectangle(dm.getDisplay(), _taskbar, inactive_gc, button_startx, 0, button_iwidth, BARHEIGHT() - DEF_BORDERWIDTH);
		        }
		        if (!c->getTrans() && c->getName()) {
                    drawString(_tbxftdraw, &xft_detail, xftfont, button_startx + SPACE, SPACE + xftfont->ascent, *(c->getName()));
		        }
                ++i;
                return false;
            });
}

void 
Taskbar::drawMenubar()
{
	XFillRectangle(DisplayManager::instance().getDisplay(), _taskbar, menu_gc, 0, 0, DisplayWidth(DisplayManager::instance().getDisplay(), DisplayManager::instance().getScreen()), BARHEIGHT() - DEF_BORDERWIDTH);

    for (auto& menuItem : Menu::instance()) {
        if (!menuItem->isEmpty()) {
            drawString(_tbxftdraw, &xft_detail, xftfont, menuItem->getX() + (SPACE * 2), xftfont->ascent + SPACE, menuItem->getLabel());
		}
	}
}

unsigned int 
Taskbar::updateMenuItem (int mousex) {
    //std::cout << "enter update_menuitem" << std::endl;
	static unsigned int last_item = UINT_MAX; // retain value from last call
	unsigned int i = 0;
    auto& menu = Menu::instance();
	if (mousex == INT_MAX) { // entered function to set last_item
        last_item = menu.size();
		return UINT_MAX;
	}
    for (const auto& menuItem : menu) {
		if ((mousex >= menuItem->getX()) && (mousex <= (menuItem->getX() + menuItem->getWidth()))) {
			break;
		}
        ++i;
	}

	if (i != last_item) /* don't redraw if same */ {
		if (last_item != menu.size()) {
			drawMenuItem(last_item, 0);
		}
		if (i != menu.size()) {
			drawMenuItem(i, 1);
		}
		last_item = i; // set to new menu item
	}

	if (i != menu.size()) {
		return i;
	} else /* no item selected */ {
		return UINT_MAX;
	}
}

void
Taskbar::drawMenuItem(unsigned int index, bool active) {
    if (auto menuItem = Menu::instance().at(index); !menuItem) {
        return;
    } else {
        if (active) {
            XFillRectangle(DisplayManager::instance().getDisplay(), _taskbar, selected_gc, menuItem->getX(), 0, menuItem->getWidth(), BARHEIGHT() - DEF_BORDERWIDTH);
        } else {
            XFillRectangle(DisplayManager::instance().getDisplay(), _taskbar, menu_gc, menuItem->getX(), 0, menuItem->getWidth(), BARHEIGHT() - DEF_BORDERWIDTH);
        }
        drawString(_tbxftdraw, &xft_detail, xftfont, menuItem->getX() + (SPACE * 2), xftfont->ascent + SPACE, menuItem->getLabel());
    }
}

float
Taskbar::getButtonWidth() {
    auto& dm = DisplayManager::instance();
    return ((float)(dm.getWidth() + DEF_BORDERWIDTH)) / ClientTracker::instance().size();
}
void 
Taskbar::cyclePrevious() {
    auto& ctracker = ClientTracker::instance();
    if (ctracker.size() >= 2) { // at least 2 windows exist
        ClientPointer c = ctracker.getFocusedClient();
        auto pos = ctracker.find(c);
        if (pos == ctracker.end() || pos == ctracker.begin()) {
            // we default to the front of the list and then go back one so it really is just list.back() if
            // the focused_client is not in the list. We also do the same thing if we are looking at
            // the front of the collection as well.
            c = ctracker.back();
        } else {
            // otherwise we must go to the previous element
            c = *(--pos);
        }
        lclick_taskbutton(nullptr, c);
    }
}

void
Taskbar::cycleNext() {
    if (auto& ctracker = ClientTracker::instance(); ctracker.size() >= 2) {
        ClientPointer c = ctracker.getFocusedClient();
        auto pos = ctracker.find(c);
        if (pos != ctracker.end()) {
            if (auto next = ++pos; next == ctracker.end()) {
                // we jump to the front if we encounter the end iterator
                c = ctracker.front();
            } else {
                c = *next;
            }
        } else {
            // we are looking at the end of the list so just default to the front as the "next"
            c = ctracker.front();
        }
        lclick_taskbutton(nullptr, c);
	}
}
