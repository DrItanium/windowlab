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
    _taskbar = dm.createWindow(0 - DEF_BORDERWIDTH, 0 - DEF_BORDERWIDTH, dm.getWidth(), getBarHeight() - DEF_BORDERWIDTH, DEF_BORDERWIDTH, dm.getDefaultDepth(), CopyFromParent, dm.getDefaultVisual(), CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWEventMask, pattr);
    dm.mapWindow(_taskbar);

	_tbxftdraw = XftDrawCreate(dm.getDisplay(), (Drawable) _taskbar, dm.getDefaultVisual(), dm.getDefaultColormap());
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
        Rect bounddims {0, 0, dm.getWidth(), getBarHeight() };

		auto constraint_win = dm.createWindow(bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, pattr);
        dm.mapWindow(constraint_win);

        if (!(dm.grabPointer(false, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, None, CurrentTime) == GrabSuccess)) {
            dm.destroyWindow(constraint_win);
			return;
		}

        auto buttonWidth = getButtonWidth();

		auto button_clicked = (unsigned int)(x / buttonWidth);
        auto c = ctracker.at(button_clicked);

		lclick_taskbutton(nullptr, c);
        XEvent ev;
		do {
            dm.maskEvent(ExposureMask|MouseMask|KeyMask, ev);
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
        dm.unmapWindow(constraint_win);
        dm.destroyWindow(constraint_win);
		dm.ungrab();

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
	Rect bounddims { 0, 0, dm.getWidth(), getBarHeight() };

	auto constraint_win = dm.createWindow(bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, pattr);
    dm.mapWindow(constraint_win);

	if (!(dm.grabPointer(false, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, None, CurrentTime) == GrabSuccess)) {
        dm.destroyWindow(constraint_win);
		return;
	}
    drawMenubar();
    updateMenuItem(INT_MAX); // force initial highlight
	current_item = updateMenuItem(x);
	do {
        dm.maskEvent(MouseMask|KeyMask, ev);
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
    dm.unmapWindow(constraint_win);
    dm.destroyWindow(constraint_win);
	dm.ungrab();
}

void
Taskbar::rightClickRoot() {
    auto& dm = DisplayManager::instance();
	if (!dm.grab(MouseMask, None)) {
		return;
	}
	drawMenubar();
	XEvent ev;
	do {
		dm.maskEvent(MouseMask|KeyMask, ev);
		switch (ev.type) {
			case MotionNotify:
				if (ev.xmotion.y < getBarHeight()) {
					dm.ungrab();
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
	dm.ungrab();
}

void
Taskbar::redraw() {
	auto buttonWidth = getButtonWidth();
    auto& dm = DisplayManager::instance();
    dm.clearWindow(_taskbar);

    if (!_showing) {
		return;
	}

	unsigned int i = 0;
    ClientTracker::instance().accept([this, &i, buttonWidth](ClientPointer c) {
                auto& dm = DisplayManager::instance();
                auto& ct = ClientTracker::instance();
		        auto button_startx = static_cast<int>(i * buttonWidth);
		        auto button_iwidth = static_cast<unsigned int>(((i + 1) * buttonWidth) - button_startx);
		        if (button_startx != 0) {
		        	dm.drawLine(_taskbar, border_gc, button_startx - 1, 0, button_startx - 1, getBarHeight() - DEF_BORDERWIDTH);
		        }
		        if (c == ct.getFocusedClient()) {
                    dm.fillRectangle(_taskbar, active_gc, button_startx, 0, button_iwidth, getBarHeight() - DEF_BORDERWIDTH);
		        } else {
                    dm.fillRectangle(_taskbar, inactive_gc, button_startx, 0, button_iwidth, getBarHeight() - DEF_BORDERWIDTH);
		        }
		        if (!c->getTrans() && c->getName()) {
                    drawString(_tbxftdraw, &xft_detail, xftfont, button_startx + SPACE, SPACE + xftfont->ascent, *(c->getName()));
		        }
                ++i;
                return false;
            });
}

void 
Taskbar::drawMenubar() {
    auto& dm = DisplayManager::instance();
    dm.fillRectangle(_taskbar, menu_gc, 0, 0, dm.getWidth(), getBarHeight() - DEF_BORDERWIDTH);

    for (auto& menuItem : Menu::instance()) {
        if (!menuItem->isEmpty()) {
            drawString(_tbxftdraw, &xft_detail, xftfont, menuItem->getX() + (SPACE * 2), xftfont->ascent + SPACE, menuItem->getLabel());
		}
	}
}

unsigned int 
Taskbar::updateMenuItem (int mousex) {
	static unsigned int last_item = UINT_MAX; // retain value from last call
    auto& menu = Menu::instance();
	if (mousex == INT_MAX) { // entered function to set last_item
        last_item = menu.size();
		return UINT_MAX;
	}
	unsigned int i = 0;
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
        auto x = menuItem->getX();
        auto width = menuItem->getWidth();
        if (auto& dm = DisplayManager::instance(); active) {
            dm.fillRectangle(_taskbar, selected_gc, x, 0, width, getBarHeight() - DEF_BORDERWIDTH);
        } else {
            dm.fillRectangle(_taskbar, menu_gc, x, 0, width, getBarHeight() - DEF_BORDERWIDTH);
        }
        drawString(_tbxftdraw, &xft_detail, xftfont,x + (SPACE * 2), xftfont->ascent + SPACE, menuItem->getLabel());
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
        if (auto pos = ctracker.find(c); pos == ctracker.end() || pos == ctracker.begin()) {
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
        if (auto pos = ctracker.find(c); pos != ctracker.end()) {
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
