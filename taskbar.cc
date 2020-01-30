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

	pattr.override_redirect = True;
	pattr.background_pixel = empty_col.pixel;
	pattr.border_pixel = border_col.pixel;
	pattr.event_mask = ChildMask|ButtonPressMask|ExposureMask|EnterWindowMask;
	_taskbar = XCreateWindow(dsply, root, 0 - DEF_BORDERWIDTH, 0 - DEF_BORDERWIDTH, DisplayWidth(dsply, screen), BARHEIGHT() - DEF_BORDERWIDTH, DEF_BORDERWIDTH, DefaultDepth(dsply, screen), CopyFromParent, DefaultVisual(dsply, screen), CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWEventMask, &pattr);

	XMapWindow(dsply, _taskbar);

#ifdef XFT
	_tbxftdraw = XftDrawCreate(dsply, (Drawable) _taskbar, DefaultVisual(dsply, DefaultScreen(dsply)), DefaultColormap(dsply, DefaultScreen(dsply)));
#endif
    _made = true;
}

void remember_hidden(void)
{
	ClientPointer c;
	for (c = head_client; c; c = c->next)
	{
		c->was_hidden = c->hidden;
	}
}

void forget_hidden(void)
{
	ClientPointer c;
	for (c = head_client; c; c = c->next)
	{
		if (c == focused_client)
		{
			c->was_hidden = c->hidden;
		}
		else
		{
			c->was_hidden = 0;
		}
	}
}

void lclick_taskbutton(ClientPointer old_c, ClientPointer c)
{
	if (old_c) {
		if (old_c->was_hidden) {
			hide(old_c);
		}
	}

	if (c->hidden) {
		unhide(c);
	} else {
        if (c->was_hidden) {
			hide(c);
		} else {
			raise_lower(c);
		}
	}
	check_focus(c);
}

void
Taskbar::leftClick(int x) 
{
	XEvent ev;
	Window constraint_win;
	XSetWindowAttributes pattr;

	unsigned int button_clicked, old_button_clicked, i;
	ClientPointer c, exposed_c, old_c;
	if (head_client) {
		remember_hidden();

        // unused?
        auto [mousex, mousey] = getMousePosition();
        Rect bounddims {0, 0, DisplayWidth(dsply, screen), BARHEIGHT()};

		constraint_win = createWindow(dsply, root, bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, &pattr);
		XMapWindow(dsply, constraint_win);

		if (!(XGrabPointer(dsply, root, False, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, None, CurrentTime) == GrabSuccess)) {
			XDestroyWindow(dsply, constraint_win);
			return;
		}

        auto buttonWidth = getButtonWidth();

		button_clicked = (unsigned int)(x / buttonWidth);
		for (i = 0, c = head_client; i < button_clicked; i++) {
			c = c->next;
		}

		lclick_taskbutton(nullptr, c);

		do
		{
			XMaskEvent(dsply, ExposureMask|MouseMask|KeyMask, &ev);
			switch (ev.type) {
				case Expose:
					exposed_c = find_client(ev.xexpose.window, FRAME);
					if (exposed_c) {
						::redraw(exposed_c);
					}
					break;
				case MotionNotify:
					old_button_clicked = button_clicked;
					button_clicked = (unsigned int)(ev.xmotion.x / buttonWidth);
					if (button_clicked != old_button_clicked) {
						old_c = c;
						for (i = 0, c = head_client; i < button_clicked; i++) {
							c = c->next;
						}
						lclick_taskbutton(old_c, c);
					}
					break;
				case KeyPress:
					XPutBackEvent(dsply, &ev);
					break;
			}
		}
		while (ev.type != ButtonPress && ev.type != ButtonRelease && ev.type != KeyPress);

		XUnmapWindow(dsply, constraint_win);
		XDestroyWindow(dsply, constraint_win);
		ungrab();

		forget_hidden();
	}
}

void
Taskbar::rightClick(int x)
{
	XEvent ev;
	unsigned int current_item = UINT_MAX;
	XSetWindowAttributes pattr;

	auto [mousex, mousey] = getMousePosition();
	Rect bounddims { 0, 0, DisplayWidth(dsply, screen), BARHEIGHT() };

	auto constraint_win = createWindow(dsply, root, bounddims, 0, CopyFromParent, InputOnly, CopyFromParent, 0, &pattr);
	XMapWindow(dsply, constraint_win);

	if (!(XGrabPointer(dsply, root, False, MouseMask, GrabModeAsync, GrabModeAsync, constraint_win, None, CurrentTime) == GrabSuccess)) {
		XDestroyWindow(dsply, constraint_win);
		return;
	}
    drawMenubar();
    updateMenuItem(INT_MAX); // force initial highlight
	current_item = updateMenuItem(x);
	do
	{
		XMaskEvent(dsply, MouseMask|KeyMask, &ev);
		switch (ev.type)
		{
			case MotionNotify:
				current_item = updateMenuItem(ev.xmotion.x);
				break;
			case ButtonRelease:
				if (current_item != UINT_MAX) {
                    if (auto item = Menu::instance().at(current_item); item) {
                        forkExec(*item);
                    }
				}
				break;
			case KeyPress:
				XPutBackEvent(dsply, &ev);
				break;
		}
	}
	while (ev.type != ButtonPress && ev.type != ButtonRelease && ev.type != KeyPress);

    Taskbar::instance().redraw();
	XUnmapWindow(dsply, constraint_win);
	XDestroyWindow(dsply, constraint_win);
	ungrab();
}

void
Taskbar::rightClickRoot()
{
	XEvent ev;
	if (!grab(root, MouseMask, None))
	{
		return;
	}
	drawMenubar();
	do
	{
		XMaskEvent(dsply, MouseMask|KeyMask, &ev);
		switch (ev.type)
		{
			case MotionNotify:
				if (ev.xmotion.y < BARHEIGHT())
				{
					ungrab();
                    rightClick(ev.xmotion.x);
					return;
				}
				break;
			case KeyPress:
				XPutBackEvent(dsply, &ev);
				break;
		}
	}
	while (ev.type != ButtonRelease && ev.type != KeyPress);

    redraw();
	ungrab();
}
void
Taskbar::redraw() 
{
	unsigned int i;
	ClientPointer c;

	auto buttonWidth = getButtonWidth();
	XClearWindow(dsply, _taskbar);

	if (showing_taskbar == 0)
	{
		return;
	}

	for (c = head_client, i = 0; c ; c = c->next, i++)
	{
		auto button_startx = static_cast<int>(i * buttonWidth);
		auto button_iwidth = static_cast<unsigned int>(((i + 1) * buttonWidth) - button_startx);
		if (button_startx != 0)
		{
			XDrawLine(dsply, _taskbar, border_gc, button_startx - 1, 0, button_startx - 1, BARHEIGHT() - DEF_BORDERWIDTH);
		}
		if (c == focused_client)
		{
			XFillRectangle(dsply, _taskbar, active_gc, button_startx, 0, button_iwidth, BARHEIGHT() - DEF_BORDERWIDTH);
		}
		else
		{
			XFillRectangle(dsply, _taskbar, inactive_gc, button_startx, 0, button_iwidth, BARHEIGHT() - DEF_BORDERWIDTH);
		}
		if (!c->trans && c->name)
		{
#ifdef XFT
			XftDrawString8(_tbxftdraw, &xft_detail, xftfont, button_startx + SPACE, SPACE + xftfont->ascent, (unsigned char *)c->name, strlen(c->name));
#else
			XDrawString(dsply, _taskbar, text_gc, button_startx + SPACE, SPACE + font->ascent, c->name, strlen(c->name));
#endif
		}
	}

}

void 
Taskbar::drawMenubar()
{
	XFillRectangle(dsply, _taskbar, menu_gc, 0, 0, DisplayWidth(dsply, screen), BARHEIGHT() - DEF_BORDERWIDTH);

    for (auto& menuItem : Menu::instance()) {
		if (!menuItem->label.empty() && !menuItem->command.empty())
		{
#ifdef XFT
			XftDrawString8(_tbxftdraw, &xft_detail, xftfont, menuItem->x + (SPACE * 2), xftfont->ascent + SPACE, (unsigned char *)menuItem->label.data(), menuItem->label.size());
#else
			XDrawString(dsply, taskbar, text_gc, menuItem->x + (SPACE * 2), font->ascent + SPACE, menuItem->label, menuItem->label.size());
#endif
		}
	}
}

unsigned int 
Taskbar::updateMenuItem (int mousex)
{
    //std::cout << "enter update_menuitem" << std::endl;
	static unsigned int last_item = UINT_MAX; // retain value from last call
	unsigned int i = 0;
	if (mousex == INT_MAX) { // entered function to set last_item
        last_item = Menu::instance().size();
		return UINT_MAX;
	}
    for (const auto& menuItem : Menu::instance()) {
		if ((mousex >= menuItem->x) && (mousex <= (menuItem->x + menuItem->width))) {
			break;
		}
        ++i;
	}

	if (i != last_item) /* don't redraw if same */ {
		if (last_item != Menu::instance().size()) {
			drawMenuItem(last_item, 0);
		}
		if (i != Menu::instance().size()) {
			drawMenuItem(i, 1);
		}
		last_item = i; // set to new menu item
	}

	if (i != Menu::instance().size()) {
		return i;
	} else /* no item selected */ {
		return UINT_MAX;
	}
}

void
Taskbar::drawMenuItem(unsigned int index, unsigned int active)
{
    auto menuItem = Menu::instance().at(index);
    if (!menuItem) {
        return;
    }
	if (active) {
		XFillRectangle(dsply, _taskbar, selected_gc, menuItem->getX(), 0, menuItem->getWidth(), BARHEIGHT() - DEF_BORDERWIDTH);
	} else {
		XFillRectangle(dsply, _taskbar, menu_gc, menuItem->getX(), 0, menuItem->getWidth(), BARHEIGHT() - DEF_BORDERWIDTH);
	}
#ifdef XFT
	XftDrawString8(_tbxftdraw, &xft_detail, xftfont, menuItem->getX() + (SPACE * 2), xftfont->ascent + SPACE, (unsigned char *)menuItem->getLabel().data(), menuItem->getLabel().size());
#else
	XDrawString(dsply, _taskbar, text_gc, menuItem->getX() + (SPACE * 2), font->ascent + SPACE, menuItem->getLabel().data(), menuItem->getLabel().size());
#endif
}

float
Taskbar::getButtonWidth()
{
    return ((float)(DisplayWidth(dsply, screen) + DEF_BORDERWIDTH)) / clients.size();
}
void 
Taskbar::cyclePrevious() {
    if (clients.size() >= 2) { // at least 2 windows exist
        ClientPointer c = focused_client;
        ClientPointer original_c = c;
        auto pos = findClient(c);
        if (pos == clients.end() || pos == clients.begin()) {
            // we default to the front of the list and then go back one so it really is just list.back() if
            // the focused_client is not in the list. We also do the same thing if we are looking at
            // the front of the collection as well.
            c = clients.back();
        } else {
            // otherwise we must go to the previous element
            c = *(--pos);
        }
        lclick_taskbutton(nullptr, c);
    }
}
void cycle_previous(void)
{
    Taskbar::instance().cyclePrevious();
}

void
Taskbar::cycleNext()
{
    if (clients.size() >= 2) {
	    ClientPointer c = focused_client;
        auto pos = findClient(c);
        if (pos != clients.end()) {
            if (auto next = ++pos; next == clients.end()) {
                // we jump to the front if we encounter the end iterator
                c = clients.front();
            } else {
                c = *next;
            }
        } else {
            // we are looking at the end of the list so just default to the front as the "next"
            c = clients.front();
        }
        lclick_taskbutton(nullptr, c);
	}
}
void cycle_next(void)
{
    Taskbar::instance().cycleNext();
}
