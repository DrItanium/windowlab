/* WindowLab - an X11 window manager
 * Copyright (c) 2001-2010 Nick Gravgaard
 * me at nickgravgaard.com
 * http://nickgravgaard.com/windowlab/
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
#include <cerrno>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <tuple>
#include <optional>
#include <string>
#include <algorithm>

// semaphor activated by SIGHUP
bool doMenuItems = false;

#ifdef XFT
XGlyphInfo extents;
#endif

const std::filesystem::path& getDefMenuRc() noexcept {
    static std::filesystem::path _menu(DEF_MENURC);
    return _menu;
}

const std::filesystem::path& getHomeDirectory() noexcept {
    static std::filesystem::path _home(getenv("HOME"));
    return _home;
}

MenuItemList& getMenuItems() noexcept {
    static MenuItemList _menuitems;
    return _menuitems;
}
std::optional<MenuItem> getMenuItem(std::size_t index) noexcept {
    if (index >= getMenuItemCount()) {
        return std::nullopt;
    } else {
        return std::make_optional(getMenuItems()[index]);
    }
}

void trimLeadingWs(std::string& line) {
    line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](auto ch) { return !std::isspace(ch); }));
}

std::optional<std::tuple<std::string, std::string>> parseLine(const std::string& line) noexcept {
    if (line.empty()) {
        return std::nullopt;
    }
    std::istringstream iss(line);
    std::string label;
    if (std::getline(iss, label, ':')) {
        //std::cout << "got label: " << label << std::endl;
        std::string cmd;
        if (std::getline(iss, cmd)) {
            //std::cout << "got cmd: " << cmd << std::endl;
            trimLeadingWs(cmd);
            if (!cmd.empty() && cmd.front() != '\r' && cmd.front() != '\n') {
                //std::cout << "tuple of " << label << " and " << cmd << std::endl;
                return { std::make_tuple(label, cmd) };
            }
        }
    }
    return std::nullopt;
}

void acquireMenuItems() noexcept {
    clearMenuItems();
    //std::cout << "Entering acquireMenuItems" << std::endl;
    std::filesystem::path menurcpath = getHomeDirectory() / ".windowlab/windowlab.menurc";
    std::ifstream menufile(menurcpath);
    if (!menufile.is_open()) {
        std::error_code ec;
        menurcpath = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (ec) {
            err("readlink() /proc/self/exe failed: %s\n", ec.message().c_str());
            menurcpath = ".";
        } 
        menurcpath = menurcpath.remove_filename();
        /// @todo make sure that this path only adds the ../.. if it makes sense (there are / in the string)
        menurcpath /= "../../etc/windowlab.menurc";
        menufile.close();
        menufile.open(menurcpath);
        if (!menufile.is_open()) {
            menufile.close();
            menufile.open(getDefMenuRc());
        }
    }
    if (menufile.is_open()) {
        std::string currentLine;
        while (std::getline(menufile, currentLine)) {
            if (!currentLine.empty()) {
                auto line = currentLine;
                trimLeadingWs(line);
                if (!line.empty() && (line.front() != '#')) {
                    if (auto parsed = parseLine(line); parsed) {
                        getMenuItems().emplace_back(std::get<0>(*parsed), std::get<1>(*parsed));
                    }
                }
            }
        }
    } else {
		err("can't find ~/.windowlab/windowlab.menurc, %s or %s\n", menurcpath.c_str(), getDefMenuRc().c_str());
        getMenuItems().emplace_back(NO_MENU_LABEL, NO_MENU_COMMAND);
    }
    menufile.close();
    unsigned int buttonStartX = 0;
    for (auto& menuItem : getMenuItems()) {
        menuItem.x = buttonStartX;
#ifdef XFT
		XftTextExtents8(dsply, xftfont, (unsigned char *)menuItem.label.data(), menuItem.label.size(), &extents);
        menuItem.width = extents.width + (SPACE * 4);
#else
		menuItem.width = XTextWidth(font, menuItem.label.data(), menuItem.label.size()) + (SPACE * 4);
#endif
        buttonStartX += menuItem.width + 1;
	}
	// menu items have been built
    doMenuItems = false;
    //std::cout << "Leaving acquireMenuItems" << std::endl;
}

void clearMenuItems() noexcept
{
    getMenuItems().clear();
}

bool shouldDoMenuItems() noexcept {
    return doMenuItems;
}
void requestMenuItems() noexcept {
    doMenuItems = true;
}

