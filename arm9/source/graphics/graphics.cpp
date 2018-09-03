/*-----------------------------------------------------------------
 Copyright (C) 2015
	Matthew Scholefield

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
#include <gl2d.h>
#include "bios_decompress_callback.h"
#include "FontGraphic.h"

// Graphic files
#include "_3ds_bottom.h"
#include "_3ds_bottom_bubble.h"

#include "_3ds_top.h"

#include "dialogbox.h"
#include "_3ds_bubble.h"
#include "_3ds_box_full.h"
#include "_3ds_box_empty.h"
#include "_3ds_cursor.h"
#include "_3ds_folder.h"

#include "queueControl.h"

#include "graphics.h"
#include "fontHandler.h"
#define CONSOLE_SCREEN_WIDTH 32
#define CONSOLE_SCREEN_HEIGHT 24

extern u16 usernameRendered[10];

extern bool whiteScreen;
extern bool fadeType;
extern bool fadeSpeed;
extern bool controlTopBright;
extern bool controlBottomBright;
int fadeDelay = 0;

extern bool showbubble;
extern bool showSTARTborder;

extern bool titleboxXmoveleft;
extern bool titleboxXmoveright;

extern bool applaunchprep;

int screenBrightness = 31;

int movetimer = 0;

int titleboxYmovepos = 0;

extern int spawnedtitleboxes;

extern bool startMenu;

extern bool flashcardUsed;

extern int folderNumber;
int titleboxXmovespeed[8] = {12, 10, 8, 8, 8, 8, 6, 4};
int titleboxXpos;
int titleboxYpos = 85;	// 85, when dropped down
int titlewindowXpos;

bool startBorderZoomOut = false;
int startBorderZoomAnimSeq[5] = {0, 1, 2, 1, 0};
int startBorderZoomAnimNum = 0;
int startBorderZoomAnimDelay = 0;

int launchDotX[12] = {0};
int launchDotY[12] = {0};

bool showdialogbox = false;
float dbox_movespeed = 22;
float dbox_Ypos = -192;

int bottomBg;

int bottomBgState = 0; // 0 = Uninitialized 1 = No Bubble 2 = bubble.

int subBgTexID, mainBgTexID, shoulderTexID, ndsimenutextTexID, bubbleTexID, progressTexID, dialogboxTexID, wirelessiconTexID;
int bipsTexID, scrollwindowTexID, buttonarrowTexID, launchdotTexID, startTexID, startbrdTexID, settingsTexID, braceTexID, boxfullTexID, boxemptyTexID, folderTexID;

glImage bubbleImage[1];
glImage dialogboxImage[(256 / 16) * (256 / 16)];
glImage startbrdImage[(32 / 32) * (256 / 80)];
glImage _3dsstartbrdImage[(32 / 32) * (192 / 64)];
glImage boxfullImage[(64 / 16) * (128 / 64)];
glImage boxemptyImage[(64 / 16) * (64 / 16)];
glImage folderImage[(64 / 16) * (64 / 16)];


int vblankRefreshCounter = 0;

int bubbleYpos = 80;
int bubbleXpos = 122;

void vramcpy_ui (void* dest, const void* src, int size) 
{
	u16* destination = (u16*)dest;
	u16* source = (u16*)src;
	while (size > 0) {
		*destination++ = *source++;
		size-=2;
	}
}

void ClearBrightness(void) {
	fadeType = true;
	screenBrightness = 0;
	swiWaitForVBlank();
	swiWaitForVBlank();
}

// Ported from PAlib (obsolete)
void SetBrightness(u8 screen, s8 bright) {
	u16 mode = 1 << 14;

	if (bright < 0) {
		mode = 2 << 14;
		bright = -bright;
	}
	if (bright > 31) bright = 31;
	*(u16*)(0x0400006C + (0x1000 * screen)) = bright + mode;
}

//-------------------------------------------------------
// set up a 2D layer construced of bitmap sprites
// this holds the image when rendering to the top screen
//-------------------------------------------------------

void initSubSprites(void)
{

	oamInit(&oamSub, SpriteMapping_Bmp_2D_256, false);
	int id = 0;

	//set up a 4x3 grid of 64x64 sprites to cover the screen
	for (int y = 0; y < 3; y++)
		for (int x = 0; x < 4; x++)
		{
			oamSub.oamMemory[id].attribute[0] = ATTR0_BMP | ATTR0_SQUARE | (64 * y);
			oamSub.oamMemory[id].attribute[1] = ATTR1_SIZE_64 | (64 * x);
			oamSub.oamMemory[id].attribute[2] = ATTR2_ALPHA(1) | (8 * 32 * y) | (8 * x);
			++id;
		}

	swiWaitForVBlank();

	oamUpdate(&oamSub);
}

void bottomBgLoad(bool drawBubble, bool init = false) {
	if (init || (!drawBubble && bottomBgState == 2)) {
		dmaCopy(_3ds_bottomTiles, bgGetGfxPtr(bottomBg), _3ds_bottomTilesLen);
		dmaCopy(_3ds_bottomPal, BG_PALETTE, _3ds_bottomPalLen);
		dmaCopy(_3ds_bottomMap, bgGetMapPtr(bottomBg), _3ds_bottomMapLen);
		// Set that we've not drawn the bubble.
		bottomBgState = 1;
	} else if (drawBubble && bottomBgState == 1){
		dmaCopy(_3ds_bottom_bubbleTiles, bgGetGfxPtr(bottomBg), _3ds_bottom_bubbleTilesLen);
		dmaCopy(_3ds_bottom_bubblePal, BG_PALETTE, _3ds_bottom_bubblePalLen);
		dmaCopy(_3ds_bottom_bubbleMap, bgGetMapPtr(bottomBg), _3ds_bottom_bubbleMapLen);
		// Set that we've drawn the bubble.
		bottomBgState = 2;
	}
}

// No longer used.
// void drawBG(glImage *images)
// {
// 	for (int y = 0; y < 256 / 16; y++)
// 	{
// 		for (int x = 0; x < 256 / 16; x++)
// 		{
// 			int i = y * 16 + x;
// 			glSprite(x * 16, y * 16, GL_FLIP_NONE, &images[i & 255]);
// 		}
// 	}
// }

void drawBubble(glImage *images)
{
	glSprite(bubbleXpos, bubbleYpos, GL_FLIP_NONE, &images[0]);
}

void drawDbox()
{
	for (int y = 0; y < 192 / 16; y++)
	{
		for (int x = 0; x < 256 / 16; x++)
		{
			int i = y * 16 + x;
			glSprite(x * 16, dbox_Ypos+y * 16, GL_FLIP_NONE, &dialogboxImage[i & 255]);
		}
	}
}


void reloadDboxPalette() {
	glBindTexture(0, dialogboxTexID);
	glColorSubTableEXT(0, 0, 4, 0, 0, (u16*) dialogboxPal);
}

void vBlankHandler()
{
	execQueue(); // Execute any actions queued during last vblank.

	glBegin2D();
	{
		if(fadeType == true) {
			if(!fadeDelay) {
				screenBrightness--;
				if (screenBrightness < 0) screenBrightness = 0;
			}
			if (!fadeSpeed) {
				fadeDelay++;
				if (fadeDelay == 3) fadeDelay = 0;
			} else {
				fadeDelay = 0;
			}
		} else {
			if(!fadeDelay) {
				screenBrightness++;
				if (screenBrightness > 31) screenBrightness = 31;
			}
			if (!fadeSpeed) {
				fadeDelay++;
				if (fadeDelay == 3) fadeDelay = 0;
			} else {
				fadeDelay = 0;
			}
		}
		if (controlBottomBright) SetBrightness(0, screenBrightness);
		if (controlTopBright) SetBrightness(1, screenBrightness);

		if (showdialogbox) {
			// Dialogbox moving up...
			if (dbox_movespeed <= 1) {
				if (dbox_Ypos >= 0) {
					// dbox stopped
					dbox_movespeed = 0;
					dbox_Ypos = 0;
				} else {
					// dbox moving up
					dbox_movespeed = 1;
				}
			} else {
				// Dbox decel
				dbox_movespeed -= 1.25;
			}
			dbox_Ypos += dbox_movespeed;
		} else {
			// Dialogbox moving down...
			if (dbox_Ypos <= -192 || dbox_Ypos >= 192) {
				dbox_movespeed = 22;
				dbox_Ypos = -192;
			} else {
				dbox_movespeed += 1;
				dbox_Ypos += dbox_movespeed;
			}
		}

		if (titleboxXmoveleft) {
			if (movetimer == 8) {
				startBorderZoomOut = true;
				titlewindowXpos -= 1;
				movetimer++;
			} else if (movetimer < 8) {
				titleboxXpos -= titleboxXmovespeed[movetimer];
				if(movetimer==0 || movetimer==2 || movetimer==4 || movetimer==6 ) titlewindowXpos -= 1;
				movetimer++;
			} else {
				titleboxXmoveleft = false;
				movetimer = 0;
			}
		} else if (titleboxXmoveright) {
			if (movetimer == 8) {
				startBorderZoomOut = true;
				titlewindowXpos += 1;
				movetimer++;
			} else if (movetimer < 8) {
				titleboxXpos += titleboxXmovespeed[movetimer];
				if(movetimer==0 || movetimer==2 || movetimer==4 || movetimer==6 ) titlewindowXpos += 1;
				movetimer++;
			} else {
				titleboxXmoveright = false;
				movetimer = 0;
			}
		}
		int spawnedboxXpos = 96;
		int iconXpos = 112;
		for(int i = 0; i < 40; i++) {
			if (i < spawnedtitleboxes) {
				glSprite(spawnedboxXpos-titleboxXpos, titleboxYpos, GL_FLIP_NONE, folderImage);
			} else {
				// Empty box
				glSprite(spawnedboxXpos-titleboxXpos, titleboxYpos, GL_FLIP_NONE, boxemptyImage);
			}
			spawnedboxXpos += 64;
			iconXpos += 64;
		}
		if (showSTARTborder) {
			glSprite(96, 92, GL_FLIP_NONE, &_3dsstartbrdImage[startBorderZoomAnimSeq[startBorderZoomAnimNum] & 63]);
			glSprite(96+32, 92, GL_FLIP_H, &_3dsstartbrdImage[startBorderZoomAnimSeq[startBorderZoomAnimNum] & 63]);
			glColor(RGB15(31, 31, 31));
		}

		// Refresh the background layer.
		bottomBgLoad(showbubble);
		if (showbubble) drawBubble(bubbleImage);
		if (dbox_Ypos != -192) {
			// Draw the dialog box.
			drawDbox();
		}

		if (vblankRefreshCounter >= REFRESH_EVERY_VBLANKS) {
			if (showdialogbox && dbox_Ypos == -192) {
				// Reload the dialog box palettes here...
				reloadDboxPalette();
			} else if (!showdialogbox) {
				reloadFontPalettes();
			}
			vblankRefreshCounter = 0;
		} else {
			vblankRefreshCounter++;
		}

		updateText(false);
		glColor(RGB15(31, 31, 31));
	}
	glEnd2D();
	GFX_FLUSH = 0;

	startBorderZoomAnimDelay++;
	if (startBorderZoomAnimDelay == 8) {
		startBorderZoomAnimNum++;
		if(startBorderZoomAnimSeq[startBorderZoomAnimNum] == 0) {
			startBorderZoomAnimNum = 0;
		}
		startBorderZoomAnimDelay = 0;
	}
}

void topBgLoad() {
	swiDecompressLZSSVram ((void*)_3ds_topTiles, (void*)CHAR_BASE_BLOCK_SUB(2), 0, &decompressBiosCallback);
	vramcpy_ui (&BG_PALETTE_SUB[0], _3ds_topPal, _3ds_topPalLen);
}

void graphicsInit()
{
	titleboxXpos = folderNumber*64;
	
	*(u16*)(0x0400006C) |= BIT(14);
	*(u16*)(0x0400006C) &= BIT(15);
	SetBrightness(0, 31);
	SetBrightness(1, 31);

	////////////////////////////////////////////////////////////
	videoSetMode(MODE_5_3D | DISPLAY_BG2_ACTIVE);
	videoSetModeSub(MODE_3_2D | DISPLAY_BG0_ACTIVE);

	// Initialize gl2d
	glScreen2D();
	// Make gl2d render on transparent stage.
	glClearColor(31,31,31,0);
	glDisable(GL_CLEAR_BMP);

	// Clear the GL texture state
	glResetTextures();

	vramSetBankC(VRAM_C_SUB_BG_0x06200000);

	REG_BG0CNT_SUB = BG_MAP_BASE(0) | BG_COLOR_256 | BG_TILE_BASE(2);
	BG_PALETTE[0]=0;
	BG_PALETTE[255]=0xffff;
	u16* bgMapSub = (u16*)SCREEN_BASE_BLOCK_SUB(0);
	for (int i = 0; i < CONSOLE_SCREEN_WIDTH*CONSOLE_SCREEN_HEIGHT; i++) {
		bgMapSub[i] = (u16)i;
	}

	lcdMainOnBottom();

	topBgLoad();

	// Set up enough texture memory for our textures
	// Bank A is just 128kb and we are using 194 kb of
	// sprites
	vramSetBankA(VRAM_A_TEXTURE);
	vramSetBankB(VRAM_B_TEXTURE);
	vramSetBankD(VRAM_D_MAIN_BG_0x06000000);
	vramSetBankE(VRAM_E_TEX_PALETTE);
	vramSetBankF(VRAM_F_TEX_PALETTE_SLOT4);
	vramSetBankG(VRAM_G_TEX_PALETTE_SLOT5); // 16Kb of palette ram, and font textures take up 8*16 bytes.
	vramSetBankH(VRAM_H_SUB_BG_EXT_PALETTE);
	vramSetBankI(VRAM_I_SUB_SPRITE_EXT_PALETTE);

	// Initialize the bottom background
	bottomBg = bgInit(2, BgType_ExRotation, BgSize_ER_256x256, 0,1);
	
	bottomBgLoad(false, true);
	swiWaitForVBlank();

	dialogboxTexID = glLoadTileSet(dialogboxImage, // pointer to glImage array
							16, // sprite width
							16, // sprite height
							256, // bitmap width
							192, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_256, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_256, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							4, // Length of the palette to use (16 colors)
							(u16*) dialogboxPal, // Load our 16 color tiles palette
							(u8*) dialogboxBitmap // image data generated by GRIT
							);
	
	titleboxYpos = 96;
	bubbleYpos += 18;
	bubbleXpos += 3;
	bubbleTexID = glLoadTileSet(bubbleImage, // pointer to glImage array
							7, // sprite width
							7, // sprite height
							8, // bitmap width
							8, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_8, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_8, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							8, // Length of the palette to use (16 colors)
							(u16*) _3ds_bubblePal, // Load our 16 color tiles palette
							(u8*) _3ds_bubbleBitmap // image data generated by GRIT
							);

	startbrdTexID = glLoadTileSet(_3dsstartbrdImage, // pointer to glImage array
							32, // sprite width
							64, // sprite height
							32, // bitmap width
							192, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_32, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_256, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							6, // Length of the palette to use (6 colors)
							(u16*) _3ds_cursorPal, // Load our 16 color tiles palette
							(u8*) _3ds_cursorBitmap // image data generated by GRIT
							);

	boxfullTexID = glLoadTileSet(boxfullImage, // pointer to glImage array
							64, // sprite width
							64, // sprite height
							64, // bitmap width
							64, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							16, // Length of the palette to use (16 colors)
							(u16*) _3ds_box_fullPal, // Load our 16 color tiles palette
							(u8*) _3ds_box_fullBitmap // image data generated by GRIT
							);

	boxemptyTexID = glLoadTileSet(boxemptyImage, // pointer to glImage array
							64, // sprite width
							64, // sprite height
							64, // bitmap width
							64, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							16, // Length of the palette to use (16 colors)
							(u16*) _3ds_box_emptyPal, // Load our 16 color tiles palette
							(u8*) _3ds_box_emptyBitmap // image data generated by GRIT
							);

	folderTexID = glLoadTileSet(folderImage, // pointer to glImage array
							64, // sprite width
							64, // sprite height
							64, // bitmap width
							64, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							16, // Length of the palette to use (16 colors)
							(u16*) _3ds_folderPal, // Load our 16 color tiles palette
							(u8*) _3ds_folderBitmap // image data generated by GRIT
							);

	irqSet(IRQ_VBLANK, vBlankHandler);
	irqEnable(IRQ_VBLANK);
	//consoleDemoInit();


}
