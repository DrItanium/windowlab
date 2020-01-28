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
#include <filesystem>
#include <tuple>
#include <optional>

// semaphor activated by SIGHUP
bool doMenuItems = false;

static int parseline(char *, char *, char *);

#ifdef XFT
XGlyphInfo extents;
#endif

const std::filesystem::path& getDefMenuRc() noexcept {
    static std::filesystem::path _menu(DEF_MENURC);
    return _menu;
}

MenuItemList& getMenuItems() noexcept {
    static MenuItemList _menuitems;
    return _menuitems;
}

void trimLeadingWs(std::string& line) {
    line.erase(line.begin, std::find_if(line.begin(), line.end(), [](int ch) { return !std::isspace(ch); }));
}

std::optional<std::tuple<std::string, std::string>> parseLine(const std::string& line) noexcept {
    if (line.empty()) {
        return std::nullopt;
    }
    std::istringstream iss(line);
    std::string label;
    if (std::getline(iss, label, ':')) {
        std::string cmd;
        if (std::getline(iss, cmd)) {
            trimLeadingWs(cmd);
            if (!cmd.empty() && cmd.front() != '\r' && cmd.front() != '\n') {
                return { std::make_tuple(label, cmd) };
            }
        }
    }
    return std::nullopt;
}

void acquireMenuItems() noexcept {
    getMenuItems().clear();
    std::filesystem::path menurcpath = getenv("HOME") / ".windowlab/windowlab.menurc";
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
        menurcpath /= ".." / ".." / "etc/windowlab.menurc";
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
                        getMenuItems().emplace_back(std::get<0>(*parsed), std::get<1>(*parsed), 0, 0);
                    }
                }
            }
        }
    } else {
		err("can't find ~/.windowlab/windowlab.menurc, %s or %s\n", menurcpath.c_str(), getDefMenuRc().c_str());
        getMenuItems().emplace_back({NO_MENU_LABEL, NO_MENU_COMMAND});
#if 0
		// one menu item - xterm
		err("can't find ~/.windowlab/windowlab.menurc, %s or %s\n", menurcpath, DEF_MENURC);
		menuitems[0].command = (char *)malloc(strlen(NO_MENU_COMMAND) + 1);
		strcpy(menuitems[0].command, NO_MENU_COMMAND);
		menuitems[0].label = (char *)malloc(strlen(NO_MENU_LABEL) + 1);
		strcpy(menuitems[0].label, NO_MENU_LABEL);
		num_menuitems = 1;
#endif
    }
    menufile.close();
    unsigned int buttonStartX = 0;
    for (const auto& menuItem : getMenuItems()) {
        menuItem.x = button_startx;
    }
	for (i = 0; i < num_menuitems; i++)
	{
		menuitems[i].x = buttonStartX;
#ifdef XFT
		XftTextExtents8(dsply, xftfont, (unsigned char *)menuitems[i].label.data(), menuitems[i].label.size(), &extents);
        menuItem.width = extents.width + (SPACE * 4);
#else
		menuItem.width = XTextWidth(font, menuitems[i].label.data(), menuitems[i].label.size()) + (SPACE * 4);
#endif
        buttonStartX += menuItem.width + 1;
	}
	// menu items have been built
    doMenuItems = false;
}
#if 0
void get_menuitems(void)
{
	unsigned int i, button_startx = 0;
	FILE *menufile = nullptr;
	char menurcpath[PATH_MAX], *c;

	menuitems = (MenuItem *)malloc(MAX_MENUITEMS_SIZE);
	if (!menuitems)
	{
		err("Unable to allocate menu items array.");
		return;
	}
	memset(menuitems, 0, MAX_MENUITEMS_SIZE);

	snprintf(menurcpath, sizeof(menurcpath), "%s/.windowlab/windowlab.menurc", getenv("HOME"));
#ifdef DEBUG
	printf("trying to open: %s\n", menurcpath);
#endif
	if (!(menufile = fopen(menurcpath, "r")))
	{
		ssize_t len;
		// get location of the executable
		if ((len = readlink("/proc/self/exe", menurcpath, PATH_MAX - 1)) == -1)
		{
			err("readlink() /proc/self/exe failed: %s\n", strerror(errno));
			menurcpath[0] = '.';
			menurcpath[1] = '\0';
		}
		else
		{
			// insert null to end the file path properly
			menurcpath[len] = '\0';
		}
		if ((c = strrchr(menurcpath, '/')))
		{
			*c = '\0';
		}
		if ((c = strrchr(menurcpath, '/')))
		{
			*c = '\0';
		}
		strncat(menurcpath, "/etc/windowlab.menurc", PATH_MAX - strlen(menurcpath) - 1);
#ifdef DEBUG
		printf("trying to open: %s\n", menurcpath);
#endif
		if (!(menufile = fopen(menurcpath, "r")))
		{
#ifdef DEBUG
			printf("trying to open: %s\n", DEF_MENURC);
#endif
			menufile = fopen(DEF_MENURC, "r");
		}
	}
	if (menufile)
	{
		num_menuitems = 0;
		while ((!feof(menufile)) && (!ferror(menufile)) && (num_menuitems < MAX_MENUITEMS))
		{
			char menustr[STR_SIZE] = "";
			fgets(menustr, STR_SIZE, menufile);
			if (strlen(menustr) != 0)
			{
				char *pmenustr = menustr;
				while (pmenustr[0] == ' ' || pmenustr[0] == '\t')
				{
					pmenustr++;
				}
				if (pmenustr[0] != '#')
				{
					char labelstr[STR_SIZE] = "", commandstr[STR_SIZE] = "";
					if (parseline(pmenustr, labelstr, commandstr))
					{
						menuitems[num_menuitems].label = (char *)malloc(strlen(labelstr) + 1);
						menuitems[num_menuitems].command = (char *)malloc(strlen(commandstr) + 1);
						strcpy(menuitems[num_menuitems].label, labelstr);
						strcpy(menuitems[num_menuitems].command, commandstr);
						num_menuitems++;
					}
				}
			}
		}
		fclose(menufile);
	}
	else
	{
		// one menu item - xterm
		err("can't find ~/.windowlab/windowlab.menurc, %s or %s\n", menurcpath, DEF_MENURC);
		menuitems[0].command = (char *)malloc(strlen(NO_MENU_COMMAND) + 1);
		strcpy(menuitems[0].command, NO_MENU_COMMAND);
		menuitems[0].label = (char *)malloc(strlen(NO_MENU_LABEL) + 1);
		strcpy(menuitems[0].label, NO_MENU_LABEL);
		num_menuitems = 1;
	}

	for (i = 0; i < num_menuitems; i++)
	{
		menuitems[i].x = button_startx;
#ifdef XFT
		XftTextExtents8(dsply, xftfont, (unsigned char *)menuitems[i].label, strlen(menuitems[i].label), &extents);
		menuitems[i].width = extents.width + (SPACE * 4);
#else
		menuitems[i].width = XTextWidth(font, menuitems[i].label, strlen(menuitems[i].label)) + (SPACE * 4);
#endif
		button_startx += menuitems[i].width + 1;
	}
	// menu items have been built
	do_menuitems = 0;
}
#endif

int parseline(char *menustr, char *labelstr, char *commandstr)
{
	int success = 0;
	int menustrlen = strlen(menustr);
	char *ptemp = nullptr;
	char *menustrcpy = (char *)malloc(menustrlen + 1);

	if (!menustrcpy)
	{
		return 0;
	}

	strcpy(menustrcpy, menustr);
	ptemp = strtok(menustrcpy, ":");

	if (ptemp)
	{
		strcpy(labelstr, ptemp);
		ptemp = strtok(nullptr, "\n");
		if (ptemp) // right of ':' is not empty
		{
			while (*ptemp == ' ' || *ptemp == '\t')
			{
				ptemp++;
			}
			if (*ptemp != '\0' && *ptemp != '\r' && *ptemp != '\n')
			{
				strcpy(commandstr, ptemp);
				success = 1;
			}
		}
	}
	if (menustrcpy)
	{
		free(menustrcpy);
	}
	return success;
}

void clearMenuItems() noexcept
{
    getMenu
}
void free_menuitems(void)
{
	unsigned int i;
	if (menuitems)
	{
		for (i = 0; i < num_menuitems; i++)
		{
            menuitems[i].clear();
		}
		free(menuitems);
		menuitems = nullptr;
	}
}
