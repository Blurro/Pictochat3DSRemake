#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <netdb.h>
#include <poll.h>
#include <tex3ds.h>
#include <3ds/console.h>
#include <3ds/font.h>
#include <3ds/gfx.h>
#include <stdlib.h>
#include <extras.c>

#define SCREEN_WIDTH_TOP  400
#define SCREEN_WIDTH_BOTTOM  320
#define SCREEN_HEIGHT 240
#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define MENU 0
#define CHATROOM 1
u8 stateScreen = CHATROOM;

C2D_TextBuf g_staticBuf;
C2D_Text g_staticText;
C2D_Font font;
C2D_SpriteSheet spriteSheet;
C2D_SpriteSheet fontSheet;
C2D_SpriteSheet canvSheetTemp;
C2D_ImageTint tintMenuBorders;
C2D_ImageTint tintTextBlack;
C2D_ImageTint tintTextSpecial;
C2D_ImageTint tintTextHover;
u16 typedGlyphs[1024];
u16 typedGlyphsIndex = 0;
u16 beginTypeX = 95;
u16 beginTypeY = 22;
u8 keyboardBegin = 0;

void AppendGlyph(u16 glyph) {
	if (typedGlyphsIndex < 1024) {
		typedGlyphs[typedGlyphsIndex++] = glyph;
	}
}

void NormalKeyAction(u16 keyID) {
	AppendGlyph(keyID);
}

void SpecialKeyAction(u16 keyID, const char* keyLabel) {
	if (strcmp(keyLabel, "backsp") == 0) {
		if (typedGlyphsIndex > 0) typedGlyphsIndex--;
	}
}

void DrawTypedGlyphs() {
	u8 startX = 38;
	u16 endX = 310;
	u8 addY = 17;
	u16 nextTypeX = beginTypeX;
	u16 nextTypeY = beginTypeY;
	u8 lineCount = 5;
	for (int i = 0; i < typedGlyphsIndex; i++) {
		// spacebar case
		if (typedGlyphs[i] - keyboardBegin == 47) {
			u8 spaceWidth = 3; // 3 seems cool
			nextTypeX += spaceWidth + 1;
			if (nextTypeX > endX) {
				if (nextTypeY + addY > beginTypeY + lineCount * addY) {
					typedGlyphsIndex--;
					break; // already went 6 lines, stop drawing
				}
				else {
					nextTypeX = startX;
					nextTypeY += addY; // move down
				}
			}
			continue;
		}
		// enter case
		if (typedGlyphs[i] - keyboardBegin == 33) {
			if (nextTypeY + addY > beginTypeY + lineCount * addY) {
				typedGlyphsIndex--;
				continue; // already went 6 lines, stay on current
			}
			else {
				nextTypeX = startX;
				nextTypeY += addY; // move down
			}
			continue;
		}
		// any other key case
		C2D_Image img = C2D_SpriteSheetGetImage(fontSheet, typedGlyphs[i] - keyboardBegin);
		if (nextTypeX + img.subtex->width > endX) {
			if (nextTypeY + addY > beginTypeY + lineCount * addY) {
				typedGlyphsIndex--;
				break; // already went 6 lines, stop drawing
			}
			else {
				nextTypeX = startX; // reset x
				nextTypeY += addY; // move down
			}
		}
		C2D_DrawImageAt(img, nextTypeX, nextTypeY, 0.0f, &tintTextBlack, 1.0f, 1.0f);
		nextTypeX += img.subtex->width + 1;
	}
}

typedef struct {
	int x;
	int y;
	int width;
	int height;
	const char* label;
	u8 type;
	u16 id;
} KeyButton;

#define KEY_COUNT 100
KeyButton button_hitboxes[KEY_COUNT];

const char* keyLabels[] = { // order of appearance
	"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "min", "eq",
	"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "backsp",
	"caps", "a", "s", "d", "f", "g", "h", "j", "k", "l", "enter",
	"shift", "z", "x", "c", "v", "b", "n", "m", "comma", "dot", "forslash",
	"semicol", "acute", "space", "leftsqu", "rightsqu"
};

const char* layoutLabels[] = {
	"canvas", "send", "pull", "clr"
};

u8 drawID = 0;

#define NORMALKEY 1
#define SPECIALKEY 2
#define LAYOUTBUTTON 3
#define CANVAS 4
static void initKeys() {
	memset(button_hitboxes, 0, KEY_COUNT * sizeof(KeyButton));
	u8 key = 0;
	// button 1 and 2 are reserved for 'off' (not clicking) and 'none' (clicking but on no button)
	u8 button = 2; // button refers to position in hitboxes, doesnt match key which is keyLabels pos

	// get id 0 to all 0s
	memset(&button_hitboxes[0], 0, sizeof(button_hitboxes[0]));
	button_hitboxes[1].id = 1;
	// canvas 'button'
	button_hitboxes[button].x = 34;
	button_hitboxes[button].y = 20;
	button_hitboxes[button].width = 282;
	button_hitboxes[button].height = 101;
	button_hitboxes[button].label = layoutLabels[0];
	button_hitboxes[button].type = CANVAS;
	button_hitboxes[button].id = button;
	drawID = button;
	button++;
	// send button

	button++;
	// pull button

	button++;
	// clear button

	button++;
	// keyboard 1 keys
	keyboardBegin = button;
	for (int i = 0; i < 12; i++) {
		button_hitboxes[button].x = 37 + (i * 18); // 17px + 1px gap
		button_hitboxes[button].y = 129;
		button_hitboxes[button].width = 17;
		button_hitboxes[button].height = 17;
		button_hitboxes[button].label = keyLabels[key];
		button_hitboxes[button].type = NORMALKEY;
		button_hitboxes[button].id = button;
		key++;
		button++;
	}
	for (int i = 0; i < 11; i++) {
		button_hitboxes[button].x = 47 + (i * 18);
		button_hitboxes[button].y = 147;
		button_hitboxes[button].width = 17;
		button_hitboxes[button].type = NORMALKEY;
		if (i == 10) { // backspace button
			button_hitboxes[button].width = 29;
			button_hitboxes[button].type = SPECIALKEY;
		}
		button_hitboxes[button].height = 17;
		button_hitboxes[button].label = keyLabels[key];
		button_hitboxes[button].id = button;
		key++;
		button++;
	}
	u8 add = 0;
	for (int i = 0; i < 11; i++) {
		button_hitboxes[button].x = 34 + (i * 18) + add;
		button_hitboxes[button].y = 165;
		button_hitboxes[button].width = 17;
		button_hitboxes[button].type = NORMALKEY;
		if (i == 0 || i == 10) { // caps and enter
			button_hitboxes[button].width = 39;
			if (i == 0) {
				button_hitboxes[button].width = 20;
				add = 3;
				button_hitboxes[button].type = SPECIALKEY; // enter stays normalkey, handled by draw
			}
		}
		button_hitboxes[button].height = 17;
		button_hitboxes[button].label = keyLabels[key];
		button_hitboxes[button].id = button;
		key++;
		button++;
	}
	add = 0;
	for (int i = 0; i < 11; i++) {
		button_hitboxes[button].x = 34 + (i * 18) + add;
		button_hitboxes[button].y = 183;
		button_hitboxes[button].width = 17;
		button_hitboxes[button].type = NORMALKEY;
		if (i == 0) { // shift
			button_hitboxes[button].width = 29;
			add = 12;
			button_hitboxes[button].type = SPECIALKEY;
		}
		button_hitboxes[button].height = 17;
		button_hitboxes[button].label = keyLabels[key];
		button_hitboxes[button].id = button;
		key++;
		button++;
	}
	add = 0;
	for (int i = 0; i < 5; i++) {
		button_hitboxes[button].x = 73 + (i * 18) + add;
		button_hitboxes[button].y = 201;
		button_hitboxes[button].width = 17;
		button_hitboxes[button].type = NORMALKEY;
		if (i == 2) { // space
			button_hitboxes[button].width = 89;
			add = 72;
			button_hitboxes[button].type = NORMALKEY; // still normal, the draw func will handle when it sees the spacebar id
		}
		button_hitboxes[button].height = 17;
		button_hitboxes[button].label = keyLabels[key];
		button_hitboxes[button].id = button;
		key++;
		button++;
	}
}

typedef struct {
	C2D_Image image;
	int width;
	int height;
	char* name;
} NameEntry;

NameEntry usernames[2]; // our two usernames

void drawUsernameAt(NameEntry* entry, float x, float y) {
	C2D_DrawImageAt(entry->image, x, y, 0.0f, NULL, 1.0f, 1.0f);
}

//BUTTON TEST FUNCTION
bool triangleOn = false;
void drawTriangleIfOn() {
	if (triangleOn) {
		C2D_DrawTriangle(50 / 2, SCREEN_HEIGHT - 50, C2D_Color32(255, 0, 0, 255),
			0, SCREEN_HEIGHT, C2D_Color32(0, 255, 0, 255),
			50, SCREEN_HEIGHT, C2D_Color32(0, 0, 255, 255), 0);
	}
}

u16 lastTouchX = 65535;
u16 lastTouchY = 65535;
u16 lastClick = 0;
u16 stateClick = 0; // 0 means "off"
u16 stateClickAndTouch = 0; // 0 means "off" - this one is used for tinting text and counting hold time
u8 buttonTimer = 0;
KeyButton* keyLast = &button_hitboxes[0];
void buttonCode(u16 c, u16 px, u16 py, bool mousedown, KeyButton* key) {
	if (stateClick == 0 || stateClick == c) {
		bool touching = (px >= key->x && px < key->x + key->width && py >= key->y && py < key->y + key->height);
		//snprintf(printText + strlen(printText), sizeof(printText) - strlen(printText), "Button %d clicked at (%d, %d) - key position: (%f, %f) size: (%f, %f)\n", c, px, py, key->x, key->y, key->width, key->height);
		if ((mousedown && touching) || (stateClick == c)) {
			if (mousedown || (stateClick == c)) {
				stateClick = c;
				keyLast = key;
				bool wasTouching = (lastTouchX != 65535 && lastTouchY != 65535 &&
					lastTouchX >= key->x && lastTouchX < key->x + key->width &&
					lastTouchY >= key->y && lastTouchY < key->y + key->height);
				if (!mousedown && wasTouching) {
					lastTouchX = 65535;
					lastTouchY = 65535;
					buttonTimer = 0;
					// this where the button click action happens

					// make img = the character image then draw it
					//C2D_Image img = C2D_SpriteSheetGetImage(fontSheet, getGlyphIndex(key->label));
					if (key->type == NORMALKEY) {
						NormalKeyAction(key->id);
					}
					else if (key->type == SPECIALKEY) {
						SpecialKeyAction(key->id, key->label);
					}
				}
				else if (touching) {
					// click and holding within button bounds
					stateClickAndTouch = c;
					if (key->type == NORMALKEY || key->type == SPECIALKEY) {
						C2D_DrawRectSolid(key->x, key->y, 0, key->width, 17, C2D_Color32(154, 154, 178, 255));
					}
					//else if (key->type == LAYOUTBUTTON) {
						// whatever holding down graphic this uses }
					if (buttonTimer < 34 && key->type != CANVAS) {
						buttonTimer += 1;
					}
					if (buttonTimer == 34) {
						if (key->type == NORMALKEY) {
							NormalKeyAction(key->id);
						}
						else if (key->type == SPECIALKEY) {
							SpecialKeyAction(key->id, key->label);
						}
						buttonTimer -= 5;
					}
				}
				else
				{
					if (!mousedown) {
						stateClick = 0; // Reset stateClick to "off"
						stateClickAndTouch = 0;
						buttonTimer = 0;
					}
					else {
						if (key->type == NORMALKEY || key->type == SPECIALKEY) {
							C2D_DrawRectSolid(key->x, key->y, 0, key->width, 17, C2D_Color32(154, 154, 178, 255)); // drag off button outer square colour
							if (key->type == SPECIALKEY) {
								C2D_DrawRectSolid(key->x + 1, key->y + 1, 0, key->width - 2, 15, C2D_Color32(203, 203, 203, 255));
							}
							else {
								C2D_DrawRectSolid(key->x + 1, key->y + 1, 0, key->width - 2, 15, C2D_Color32(235, 235, 235, 255));
							}
						}
						//else if (key->type == LAYOUTBUTTON) {
							// whatever drag off graphic }
						buttonTimer = 0;
						stateClickAndTouch = 0; // Reset to "off"
					}
				}
			}
		}
	}
}

//---------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
//---------------------------------------------------------------------------------
	romfsInit();
	cfguInit();
	// Init libs
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);

	// Create screens
	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	touchPosition touch;

	C2D_PlainImageTint(&tintTextBlack, C2D_Color32(0, 0, 0, 255), 1.0f);
	C2D_PlainImageTint(&tintTextSpecial, C2D_Color32(81, 81, 81, 255), 1.0f);
	C2D_PlainImageTint(&tintTextHover, C2D_Color32(40, 81, 138, 255), 1.0f);




	usernames[0].name = "joe";


	// make fonts and texts
	g_staticBuf = C2D_TextBufNew(4096);
	font = C2D_FontLoad("romfs:/mojangles.bcfnt");
	C2D_TextFontParse(&g_staticText, font, g_staticBuf, "i am attempting to make a 3ds app");
	C2D_TextOptimize(&g_staticText); // necessary to add after every parse it seems

	spriteSheet = C2D_SpriteSheetLoad("romfs:/sprites.t3x");
	fontSheet = C2D_SpriteSheetLoad("romfs:/font.t3x");
	canvSheetTemp = C2D_SpriteSheetLoad("romfs:/canvload.t3x");
	initKeys();

	static float fpsAvg = 60;
	static u64 lastTime = 0;
	u32 bgcol = C2D_Color32(170, 170, 170, 255);
	u32 bgcol2 = C2D_Color32(59, 61, 80, 255);
	u32 whitecol = C2D_Color32(255, 255, 255, 255);

	C2D_Image img; // this img i reuse a bunch in loop

	MemoryImage imgCanvasCopy;

	// define canvas drawing things
	PixelArray drawPixelsFrame;
	(&drawPixelsFrame)->data = (PixelData*)linearAlloc(3200 * sizeof(PixelData)); // user couldnt possibly draw more than 320 pixels in one frame
	memset((&drawPixelsFrame)->data, 0, 3200 * sizeof(PixelData));
	(&drawPixelsFrame)->count = 0;
	MemoryImage drawSpaceImg;
	u32* rawCanvas;
	u32* rawMask;
	u32* rawcanv6;

	//each of the below lines can be placed inside loop without mem leaks. above would require frees before end of loop or definition
	C2D_Image imgCanvmask = C2D_SpriteSheetGetImage(spriteSheet, 3);
	C2D_Image imgCanvas = C2D_SpriteSheetGetImage(spriteSheet, 2);
	C2D_Image imgToPixels = C2D_SpriteSheetGetImage(fontSheet, 24);
	C2D_Image imgToPixels2 = C2D_SpriteSheetGetImage(fontSheet, 25);



	MemoryImage canv6burn;
	C2D_Image imgCanv6 = C2D_SpriteSheetGetImage(canvSheetTemp, 5);
	AllocDataFromImage(&rawcanv6, &imgCanv6);
	C2D_SpriteSheetFree(canvSheetTemp);

	for (int y = 0; y < 105; ++y) {
		uint32_t* row = rawcanv6 + y * 286;
		uint32_t last4[4] = { row[282], row[283], row[284], row[285] };

		// shift pixels at 282–285 to 128–131
		row[128] = last4[0];
		row[129] = last4[1];
		row[130] = last4[2];
		row[131] = last4[3];

		// zero out the original end of row (optional)
		row[282] = 0;
		row[283] = 0;
		row[284] = 0;
		row[285] = 0;

		// optional: clear x=132 to 285
		for (int x = 132; x < 286; ++x) row[x] = 0;
	}
	AllocImageFromData(&canv6burn, rawcanv6, 286, 105, true);





	AllocDataFromImage(&rawCanvas, &imgCanvas); // this one is NOT a pointer! (this and imagefromdata will free before allocating automatically anyway)
	AllocImageFromData(&imgCanvasCopy, rawCanvas, (&imgCanvas)->subtex->width, (&imgCanvas)->subtex->height, true);


	PixelArray charsAsPixels;
	PixelArray charsAsPixels2;
	ImgPos images[2] = {
	{ &imgToPixels, 0, 0 },
	{ &imgToPixels2, 10, 4 }
	};
	memset(&charsAsPixels, 0, sizeof(PixelArray));
	memset(&charsAsPixels2, 0, sizeof(PixelArray));
	AllocPixelsFromImages(&charsAsPixels, images, 2, 0x000000FF);
	UpdateImage(&imgCanvasCopy, &charsAsPixels);

	// gets canvas mask as data which we use for checking drawing within bounds
	AllocDataFromImage(&rawMask, &imgCanvmask); // rawmask is used in loop for paint out of bounds checking
	AllocImageFromData(&drawSpaceImg, rawMask, (&imgCanvmask)->subtex->width, (&imgCanvmask)->subtex->height, true); // just copies our mask size to a blank image (handles the uv calc stuff too so even tho the swizzle math is unnecessary)
	memset(drawSpaceImg.image.tex->data, 0, drawSpaceImg.image.tex->width * drawSpaceImg.image.tex->height * sizeof(u32)); // makes blank


	//UpdateImage(&imgCanvasCopy, &drawPixelsFrame);
	// free the allocs that arent used in loop
	linearFree(&rawCanvas);
	linearFree(&charsAsPixels);

	// Main loop
	while (aptMainLoop())
	{
		memset(printText, 0, sizeof(printText));
		u64 currentTime = osGetTime();
		float deltaTime = (currentTime - lastTime) / 1000.0f;
		float fps = (deltaTime > 0) ? (1.0f / deltaTime) : 0;
		lastTime = currentTime;
		fpsAvg = fpsAvg * 0.95f + fps * 0.05f;

		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu
		
		// Render the scene
		C2D_Prepare();
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

		// ========================
		//   TOP SCREEN  \/\/\/\/\/
		// ========================

		C2D_TargetClear(top, bgcol2);
		C2D_SceneBegin(top);

		//img = C2D_SpriteSheetGetImage(spriteSheet, 3);
		//C2D_DrawImageAt(img, 60, 0, 0, NULL, 299, 1);
		C2D_DrawRectSolid(60, 0, 0, 299, SCREEN_HEIGHT, C2D_Color32(170, 170, 170, 255));
		for (int y = 2; y < SCREEN_HEIGHT; y += 5) { // to my surprise this is faster than drawing the bg image
			C2D_DrawRectSolid(60, y, 0, 299, 1, C2D_Color32(186, 186, 186, 255)); }
		// sidebar
		for (float y = 0; y < 240.0f; y += 3) {
			for (int i = 0; i < 6; i++) { // 3 squares per row, 3px each
				float x = i * 3;
				u32 color = ((i + (int)(y / 3)) % 2 == 0) ? C2D_Color32(0, 0, 0, 255) : C2D_Color32(73, 73, 73, 255);
				C2D_DrawRectSolid(x + 41, y, 0.0f, 3, 3, color);
			}
		}
		C2D_DrawRectSolid(40, 0, 0.0f, 1, 240, whitecol);
		C2D_DrawRectSolid(59, 0, 0.0f, 1, 240, whitecol);
		C2D_DrawRectSolid(359, 0, 0.0f, 1, 240, whitecol);

		//C2D_DrawImageAt(imgCanvasCopy.image, 32, 18, 0, NULL, 1, 1);
		C2D_DrawImageAt(canv6burn.image, 62, 18, 0, NULL, 1, 1);
		



		C2D_DrawTriangle(50 / 2, SCREEN_HEIGHT - 50, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF),
			0,  SCREEN_HEIGHT, C2D_Color32(0xFF, 0x15, 0x00, 0xFF),
			50, SCREEN_HEIGHT, C2D_Color32(0x27, 0x69, 0xE5, 0xFF), 0);

		// ========================
		// BOTTOM SCREEN \/\/\/\/\/
		// ========================

		C2D_TargetClear(bottom, bgcol); // Clear screen
		C2D_SceneBegin(bottom);


		//img = C2D_SpriteSheetGetImage(spriteSheet, 3);
		//C2D_DrawImageAt(img, 0, 0, 0, NULL, 320, 1);
		// grey bg with the lightgrey line every 5px like pictochat (that does 4px but we bigger res now)
		//C2D_DrawRectSolid(0, 0, 0, SCREEN_WIDTH_BOTTOM, SCREEN_HEIGHT, C2D_Color32(170, 170, 170, 255)); // base grey
		for (int y = 2; y < SCREEN_HEIGHT; y += 5) {
			C2D_DrawRectSolid(0, y, 0, SCREEN_WIDTH_BOTTOM, 1, C2D_Color32(186, 186, 186, 255)); }

		if (stateScreen == MENU) {
			C2D_AlphaImageTint(&tintMenuBorders, 200); // set opacity
			C2D_DrawImageAt(img, 0, 0, 0, &tintMenuBorders, 1.0f, -1.0f);
			C2D_DrawImageAt(img, 0, SCREEN_HEIGHT - 30, 0, &tintMenuBorders, 1.0f, 1.0f);
		}

		img = C2D_SpriteSheetGetImage(spriteSheet, 1);
		C2D_DrawImageAt(img, 32, 127, 0, NULL, 1, 1);

		img = C2D_SpriteSheetGetImage(spriteSheet, 2);
		C2D_DrawImageAt(img, 32, 18, 0, NULL, 1, 1);

		img = C2D_SpriteSheetGetImage(spriteSheet, 4);
		C2D_DrawImageAt(img, 0, 0, 0, NULL, 1, 1);

		hidTouchRead(&touch);
		u32 kHeld = hidKeysHeld();
		bool mouseDown = kHeld & KEY_TOUCH;

		C2D_DrawRectSolid(touch.px, touch.py, 0, 1.5, 1.5, C2D_Color32(255, 0, 0, 255)); // debug square that follows stylus

		snprintf(printText + strlen(printText), sizeof(printText) - strlen(printText), "X: %d Y: %d", touch.px, touch.py);


		// use this for drawing to keyboard at start
		//for (int i = 2; i < KEY_COUNT; i++) {
		//	if (button_hitboxes[i].type == NORMALKEY || button_hitboxes[i].type == SPECIALKEY) {
		//		img = C2D_SpriteSheetGetImage(fontSheet, (i - keyboardBegin));
		//		float xCenter = roundf(button_hitboxes[i].x + (button_hitboxes[i].width - img.subtex->width) / 2.0f);
		//		C2D_DrawImageAt(img, xCenter, button_hitboxes[i].y + 2, 0.0f, &tintTextBlack, 1.0f, 1.0f);}}

		if (mouseDown) {
			if (stateClick == 0) {
				for (int i = 2; i < KEY_COUNT; i++) { // i = 2 because 0 = off and 1 = none and these two aint buttons
					if (button_hitboxes[i].id != 0) {
						buttonCode(button_hitboxes[i].id, touch.px, touch.py, mouseDown, &button_hitboxes[i]);
					}
				}
			}
			else if (stateClick > 1) {
				buttonCode(keyLast->id, touch.px, touch.py, mouseDown, keyLast);
			}
			if (stateClick > 1) {
				// do keyboard graphic things
				if (keyLast->type == NORMALKEY || keyLast->type == SPECIALKEY) {
					img = C2D_SpriteSheetGetImage(fontSheet, (stateClick - keyboardBegin));
					float xCenter = roundf(keyLast->x + (keyLast->width - img.subtex->width) / 2.0f);
					if (stateClickAndTouch > 1) {
						C2D_DrawImageAt(img, xCenter, keyLast->y + 2, 0.0f, &tintTextHover, 1.0f, 1.0f);
					}
					else if (keyLast->type == SPECIALKEY) {
						C2D_DrawImageAt(img, xCenter, keyLast->y + 2, 0.0f, &tintTextSpecial, 1.0f, 1.0f);
					}
					else if (keyLast->type == NORMALKEY) {
						C2D_DrawImageAt(img, xCenter, keyLast->y + 2, 0.0f, &tintTextBlack, 1.0f, 1.0f);
					}
				}
			}
		}

		// before updating lastTouch, use it for canvas drawing
		if (mouseDown && keyLast->type == CANVAS) {
			bool wasDrawing = false;
			if (lastClick == drawID) wasDrawing = true;
			DrawOnCanvas(&drawSpaceImg, touch.px, touch.py, lastTouchX, lastTouchY, 2, &rawMask, 32, 18, &drawPixelsFrame, lastClick);
			UpdateImage(&drawSpaceImg, &drawPixelsFrame);
		}
		lastClick = stateClick; // use this so the canvas knows its drawing even while dragged off of it
		C2D_DrawImageAt(drawSpaceImg.image, 32, 18, 0, NULL, 1, 1);
		// check if clicking nothing, check if stylus was on button when lifted, check if stylus is lifted anyway
		if (mouseDown && stateClick == 0) { // 0="off" state, stylus is lifted
			stateClick = 1;  // 1="none" state, pressed down on no button
			stateClickAndTouch = 0;
			keyLast = &button_hitboxes[0];
		}
		else if (stateClick > 1) {
			if (!mouseDown) {
				buttonCode(stateClick, touch.px, touch.py, mouseDown, keyLast);
				stateClick = 0;
				stateClickAndTouch = 0;
				keyLast = &button_hitboxes[0];
			}
			else {
				lastTouchX = touch.px;
				lastTouchY = touch.py;
			}
		}
		else if (!mouseDown) {
			stateClick = 0;
			stateClickAndTouch = 0;
			keyLast = &button_hitboxes[0];
		}

		snprintf(printText + strlen(printText), sizeof(printText) - strlen(printText),
			"\nlast: %d\nindex: %d\ndata offset: 0x%08X\nlength: %d",
			keyLast->id, typedGlyphsIndex,
			(unsigned int)(drawSpaceImg.image.tex->data),
			286 * 105 * 4);

		DrawTypedGlyphs();
		

		//bool isRotated = Tex3DS_SubTextureRotated((&aaaSprite)->subtex);

		//float left = (&aaaSprite)->subtex->left;
		//float top = (&aaaSprite)->subtex->top;
		//float right = (&aaaSprite)->subtex->right;
		//float bottom = (&aaaSprite)->subtex->bottom;

		//float width = right - left;
		//float height = bottom - top;
		//skip:
		snprintf(printText + strlen(printText), sizeof(printText) - strlen(printText), "\n%d\n%d - %d\n", stateClick, stateClickAndTouch, buttonTimer);
		
		//snprintf(printText + strlen(printText), sizeof(printText) - strlen(printText), "\nused: %llu bytes / %llu bytes", (unsigned long long)mem_used, (unsigned long long)mem_total);
		C2D_TextBufClear(g_staticBuf);
		C2D_TextFontParse(&g_staticText, font, g_staticBuf, printText);
		C2D_TextOptimize(&g_staticText);
		// Draw minecraft text lol
		C2D_DrawText(&g_staticText, 0, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f);

		drawTriangleIfOn();

		C2D_Flush();
		C3D_FrameEnd(0);
	}

	C2D_SpriteSheetFree(spriteSheet);
	C2D_SpriteSheetFree(fontSheet);

	// Deinit libs
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}