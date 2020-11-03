/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <maxmod9.h>

#include <vector>
#include <algorithm>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>

#include <string.h>
#include <unistd.h>
#include <gl2d.h>

#include "graphics/graphics.h"

#include "graphics/fontHandler.h"

#include "inifile.h"

#include "soundbank.h"
#include "soundbank_bin.h"

bool fadeType = false;		// false = out, true = in
bool fadeSpeed = true;		// false = slow (for DSi launch effect), true = fast
bool controlTopBright = true;
bool controlBottomBright = true;

extern void ClearBrightness();

const char* settingsinipath = "sd:/3DSBank/3DSBank.ini";

bool arm7SCFGLocked = false;
int consoleModel = 0;
/*	0 = Nintendo DSi (Retail)
	1 = Nintendo DSi (Dev/Panda)
	2 = Nintendo 3DS
	3 = New Nintendo 3DS	*/
bool isRegularDS = true;

int folderNumber = 0;
std::string folderAliases[40];
static int cursorPosition = 0;

/**
 * Remove trailing slashes from a pathname, if present.
 * @param path Pathname to modify.
 */
static void RemoveTrailingSlashes(std::string& path)
{
	while (!path.empty() && path[path.size()-1] == '/') {
		path.resize(path.size()-1);
	}
}

/**
 * Remove trailing spaces from a cheat code line, if present.
 * @param path Code line to modify.
 */
static void RemoveTrailingSpaces(std::string& code)
{
	while (!code.empty() && code[code.size()-1] == ' ') {
		code.resize(code.size()-1);
	}
}

using namespace std;

bool dropDown = false;
bool redoDropDown = false;
bool showbubble = false;
bool showSTARTborder = false;

bool titleboxXmoveleft = false;
bool titleboxXmoveright = false;

bool applaunchprep = false;

int spawnedtitleboxes = 0;

touchPosition touch;

void LoadSettings(void) {
	CIniFile settingsini( settingsinipath );

	folderNumber = settingsini.GetInt("3DSBANK", "FOLDER_SLOT", 0);
	for (int i = 0; i < 40; i++)
	{
		folderAliases[i] = settingsini.GetString("3DSBANK", "SLOT_NAME_" + to_string(i), "Slot " + to_string(i));
	}
}

void SaveSettings(bool newSlot) {
	CIniFile settingsini( settingsinipath );

	settingsini.SetInt("3DSBANK", "FOLDER_SLOT", (newSlot ? spawnedtitleboxes : cursorPosition));
	settingsini.SaveIniFile(settingsinipath);
}

void storeFolder(void) {
	char folderPath[32];
	snprintf(folderPath, sizeof(folderPath), "sd:/3DSBank/Slot%i", folderNumber);

	rename("sd:/Nintendo 3DS", folderPath);				// Store folder into bank
}

void selectFolder(int selectedFolderNumber) {
	char selectedFolderPath[32];
	snprintf(selectedFolderPath, sizeof(selectedFolderPath), "sd:/3DSBank/Slot%i", selectedFolderNumber);

	rename(selectedFolderPath, "sd:/Nintendo 3DS");		// Have 3DS use selected folder
}

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

char filePath[PATH_MAX];

//---------------------------------------------------------------------------------
void doPause() {
//---------------------------------------------------------------------------------
	// iprintf("Press start...\n");
	// printSmall(false, x, y, "Press start...");
	while(1) {
		scanKeys();
		if(keysDown() & KEY_START)
			break;
		swiWaitForVBlank();
	}
	scanKeys();
}

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

mm_sound_effect snd_launch;
mm_sound_effect snd_select;
mm_sound_effect snd_stop;
mm_sound_effect snd_wrong;
mm_sound_effect snd_back;
mm_sound_effect snd_switch;

void InitSound() {
	mmInitDefaultMem((mm_addr)soundbank_bin);
	
	mmLoadEffect( SFX_LAUNCH );
	mmLoadEffect( SFX_SELECT );

	snd_launch = {
		{ SFX_LAUNCH } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_select = {
		{ SFX_SELECT } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
}

struct DirEntry
{
	string name;
	string visibleName;
	bool isDirectory;
};

bool dirEntryPredicate(const DirEntry& lhs, const DirEntry& rhs)
{

	if (!lhs.isDirectory && rhs.isDirectory)
	{
		return false;
	}
	if (lhs.isDirectory && !rhs.isDirectory)
	{
		return true;
	}
	return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
}

void exitApp(void) {
	mmEffectEx(&snd_launch);
	fadeType = false;
	for (int i = 0; i < 60; i++) swiWaitForVBlank();
	fifoSendValue32(FIFO_USER_02, 1);
}

void getDirectoryContents(vector<DirEntry>& dirContents)
{

	dirContents.clear();

	spawnedtitleboxes = 0;
	
	struct stat st;
	DIR *pdir = opendir(".");

	if (pdir == NULL)
	{
		// iprintf("Unable to open the directory.\n");
		printSmall (false, 4, 4, "Unable to open the directory.");
	}
	else
	{

		while (true)
		{
			DirEntry dirEntry;

			struct dirent* pent = readdir(pdir);
			if (pent == NULL) break;

			stat(pent->d_name, &st);
			dirEntry.name = pent->d_name;
			dirEntry.isDirectory = (st.st_mode & S_IFDIR) ? true : false;

			if (dirEntry.isDirectory)
				dirEntry.visibleName = "[" + dirEntry.name + "]";
			else
				dirEntry.visibleName = dirEntry.name;

			if (dirEntry.name.compare(".") != 0 && dirEntry.name.compare("..") != 0 && dirEntry.isDirectory) {
				dirContents.push_back(dirEntry);
				spawnedtitleboxes++;
			}

		}

		closedir(pdir);
	}

	sort(dirContents.begin(), dirContents.end(), dirEntryPredicate);
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// overwrite reboot stub identifier
	extern u64 *fake_heap_end;
	*fake_heap_end = 0;

	defaultExceptionHandler();

	if (!fatInitDefault()) {
		consoleDemoInit();
		printf("fatInitDefault failed!");
		stop();
	}

	mkdir("sd:/3DSBank", 0777);
	chdir("sd:/3DSBank");

	LoadSettings();

	char currentFolderText[256];
	//snprintf(currentFolderText, sizeof(currentFolderText), "Folder previously used: Slot %i", folderNumber);
	snprintf(currentFolderText, sizeof(currentFolderText), "Current slot: %s", folderAliases[folderNumber].c_str());
	cursorPosition = folderNumber;

	graphicsInit(cursorPosition * 64); // (96 + 32) / 2);

	fontInit();

	keysSetRepeat(25, 5);

	InitSound();

	storeFolder();

	vector<DirEntry> dirContents;
	getDirectoryContents(dirContents);

	SaveSettings(true);

	int pressed = 0;

	fadeType = true;
	for (int i = 0; i < 30; i++) swiWaitForVBlank();
	
	printLargeCentered(false, 8, currentFolderText);

	while(1) {
		clearText();
		if (cursorPosition >= spawnedtitleboxes)
		{
			showbubble = false;
			showSTARTborder = false;
		} else {
			showbubble = true;
			showSTARTborder = true;
			//printLargeCentered(false, 37+17, dirContents.at(cursorPosition).name.c_str());
			printLargeCentered(false, 37+17, folderAliases[cursorPosition].c_str());
		}
		printLargeCentered(false, 8, currentFolderText);

		do
		{
			scanKeys();
			pressed = keysDownRepeat();
			touchRead(&touch);
			swiWaitForVBlank();
		} while (!pressed);

		if (((pressed & KEY_LEFT) && !titleboxXmoveleft && !titleboxXmoveright)
		|| ((pressed & KEY_TOUCH) && touch.py > 88 && touch.py < 144 && touch.px < 96 && !titleboxXmoveleft && !titleboxXmoveright)) {
			cursorPosition -= 1;
			if (cursorPosition >= 0) {
				titleboxXmoveleft = true;
				mmEffectEx(&snd_select);
			}
		} else if (((pressed & KEY_RIGHT) && !titleboxXmoveleft && !titleboxXmoveright)
				|| ((pressed & KEY_TOUCH) && touch.py > 88 && touch.py < 144 && touch.px > 160 && !titleboxXmoveleft && !titleboxXmoveright)) {
			cursorPosition += 1;
			if (cursorPosition <= 39) {
				titleboxXmoveright = true;
				mmEffectEx(&snd_select);
			}
		}
		if (cursorPosition < 0)
		{
			cursorPosition = 0;
		}
		else if (cursorPosition > 39)
		{
			cursorPosition = 39;
		}

		// Select folder
		if (((pressed & KEY_A) && !titleboxXmoveleft && !titleboxXmoveright && showSTARTborder)
		|| ((pressed & KEY_TOUCH) && touch.py > 88 && touch.py < 144 && touch.px > 96 && touch.px < 160 && !titleboxXmoveleft && !titleboxXmoveright && showSTARTborder)
		|| ((pressed & KEY_TOUCH) && touch.py > 170 && !titleboxXmoveleft && !titleboxXmoveright && showSTARTborder)) {
			SaveSettings(false);
			selectFolder(cursorPosition);
			exitApp();
		}
	}

	return 0;
}
