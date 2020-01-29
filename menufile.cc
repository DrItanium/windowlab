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

XGlyphInfo extents;

const std::filesystem::path& getDefMenuRc() noexcept {
    static std::filesystem::path _menu(DEF_MENURC);
    return _menu;
}

const std::filesystem::path& getHomeDirectory() noexcept {
    static std::filesystem::path _home(getenv("HOME"));
    return _home;
}

Menu&
Menu::instance() noexcept {
    static Menu _menu;
    return _menu;
}

std::optional<std::reference_wrapper<MenuItem>>
Menu::at(std::size_t index) noexcept {
    if (index >= size()) {
        return std::nullopt;
    } else {
        return std::make_optional(std::reference_wrapper<MenuItem>(_menuItems[index]));
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
void
Menu::populate() noexcept {
    clear();
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
                        _menuItems.emplace_back(std::get<0>(*parsed), std::get<1>(*parsed));
                    }
                }
            }
        }
    } else {
		err("can't find ~/.windowlab/windowlab.menurc, %s or %s\n", menurcpath.c_str(), getDefMenuRc().c_str());
        _menuItems.emplace_back(NO_MENU_LABEL, NO_MENU_COMMAND);
    }
    menufile.close();
    unsigned int buttonStartX = 0;
    for (auto& menuItem : _menuItems) {
        menuItem.x = buttonStartX;
		XftTextExtents8(dsply, xftfont, (unsigned char *)menuItem.getLabel().data(), menuItem.getLabel().size(), &extents);
        menuItem.width = extents.width + (SPACE * 4);
        buttonStartX += menuItem.getWidth()+ 1;
	}
	// menu items have been built
    _updateMenuItems = false;
}

