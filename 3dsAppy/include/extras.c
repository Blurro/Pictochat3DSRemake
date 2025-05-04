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

char printText[1024];

const u32 next_pow2(u32 n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}
const u32 clamp(u32 n, u32 lower, u32 upper) {
    if (n < lower)
        return lower;
    if (n > upper)
        return upper;
    return n;
}

typedef struct {
    C2D_Image image;
} MemoryImage;

typedef struct {
    int x;
    int y;
    u32 color;
} PixelData;

typedef struct {
    PixelData* data;
    u32 count;
} PixelArray;

typedef struct {
    C2D_Image* img;
    int x_offset;
    int y_offset;
} ImgPos;

// DO NOT DO NULL CHECKS the 3ds just hates null pointers EVEN IF YOU DO A "if (thing == NULL)" CHECK FOR THEM, JUST KEEP TRACK MANUALLY AND CRY BOUT IT
static void AllocImageFromData(MemoryImage* img, u32* rgba_raw, u32 image_widthy, u32 image_heighty, bool scaleUpNearest)
{
    // Allocate the texture and subtexture memory directly
    img->image.tex = (C3D_Tex*)linearAlloc(sizeof(C3D_Tex));
    img->image.subtex = (Tex3DS_SubTexture*)linearAlloc(sizeof(Tex3DS_SubTexture));

    // Set tex width and height, ensuring they are clamped to the nearest power of 2
    C3D_Tex* tex = img->image.tex;
    tex->width = clamp(next_pow2(image_widthy), 64, 1024);
    tex->height = clamp(next_pow2(image_heighty), 64, 1024);

    // Set subtex width and height
    Tex3DS_SubTexture* subtex = img->image.subtex;
    subtex->width = image_widthy;
    subtex->height = image_heighty;

    // Set the (U, V) coordinates for subtexture
    subtex->left = 0.0f;
    subtex->top = 1.0f;
    subtex->right = (float)image_widthy / (float)tex->width;
    subtex->bottom = 1.0f - ((float)image_heighty / (float)tex->height);

    // Initialize the texture with a specific format (RGBA)
    C3D_TexInit(tex, tex->width, tex->height, GPU_RGBA8);
    C3D_TexSetFilter(tex, scaleUpNearest ? GPU_NEAREST : GPU_LINEAR, GPU_NEAREST);

    // Allocate texture data
    tex->data = (u32*)linearAlloc(tex->width * tex->height * sizeof(u32));
    memset(tex->data, 0, tex->width * tex->height * sizeof(u32));

    // Process the pixel data and store it in the texture with swizzling
    for (int i = 0; i < image_widthy; i++) {
        for (int j = 0; j < image_heighty; j++) {
            // Swizzle magic to convert into a t3x format
            u32 dst_ptr_offset = ((((j >> 3) * (tex->width >> 3) + (i >> 3)) << 6) +
                ((i & 1) | ((j & 1) << 1) | ((i & 2) << 1) |
                    ((j & 2) << 2) | ((i & 4) << 2) | ((j & 4) << 3)));

            // Store the swizzled pixel in the texture if within bounds
            u32 tex_max_pixels = tex->width * tex->height;
            if (dst_ptr_offset < tex_max_pixels) {
                ((u32*)tex->data)[dst_ptr_offset] = rgba_raw[j * image_widthy + i];
            }
        }
    }
}

void AllocDataFromImage(u32** rgba_out, C2D_Image* img) {
    C3D_Tex* tex = img->tex;
    Tex3DS_SubTexture* subtex = (Tex3DS_SubTexture*)img->subtex;
    u32 width = subtex->width;
    u32 height = subtex->height;

    u32 total_pixels = clamp(next_pow2(width), 64, 1024) * clamp(next_pow2(height), 64, 1024);

    //if (*rgba_out) linearFree(*rgba_out);  // commented out cus null checks are dumb the 3ds hates null pointers including checking them
    *rgba_out = (u32*)linearAlloc(sizeof(u32) * total_pixels);  // Allocate new memory
    memset(*rgba_out, 0, sizeof(u32) * total_pixels);  // Clear memory

    if (!Tex3DS_SubTextureRotated(subtex)) {
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                u32 src_x = i;
                u32 src_y = j;
                src_x += (int)(subtex->left * tex->width); // uses uv to get correct sprite coords
                src_y += (int)((1.0f - subtex->top) * tex->height); // spritesheet be flipped vertically, top is 1
                u32 src_offset = ((((src_y >> 3) * (tex->width >> 3) + (src_x >> 3)) << 6) + // the swizzler
                    ((src_x & 1) | ((src_y & 1) << 1) | ((src_x & 2) << 1) |
                        ((src_y & 2) << 2) | ((src_x & 4) << 2) | ((src_y & 4) << 3)));
                (*rgba_out)[j * width + i] = ((u32*)tex->data)[src_offset];
            }
        }
    }
    else { // 90-degree CCW rotation, swap the x and y coordinates
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) { // Calculate the new position as if it's rotated
                int new_x = j;  // new x comes from old y
                int new_y = (width - 1) - i;  // new y comes from old x, reversed to account for rotation
                u32 src_x = new_x + (int)((subtex->top) * tex->width); // uses uvs to get correct sprite coords
                u32 src_y = new_y + (int)((1.0f - subtex->right) * tex->height);
                u32 src_offset = ((((src_y >> 3) * (tex->width >> 3) + (src_x >> 3)) << 6) +
                    ((src_x & 1) | ((src_y & 1) << 1) | ((src_x & 2) << 1) |
                        ((src_y & 2) << 2) | ((src_x & 4) << 2) | ((src_y & 4) << 3)));
                (*rgba_out)[j * width + i] = ((u32*)tex->data)[src_offset];
            }
        }
    }
    snprintf(printText + strlen(printText), sizeof(printText) - strlen(printText), "\n%d %d\n %.3f %.3f %.3f %.3f (rotated: %s)\n", tex->width, tex->height, subtex->left, subtex->top, subtex->right, subtex->bottom, Tex3DS_SubTextureRotated(subtex) ? "true" : "false");
}

void UpdateImage(MemoryImage* img, PixelArray* pixelDataList)
{
    //if (!img || !img->image.tex || !img->image.tex->data || !img->image.subtex) return;
    C3D_Tex* tex = img->image.tex;
    for (int i = 0; i < pixelDataList->count; i++) {
        int x = pixelDataList->data[i].x;
        int y = pixelDataList->data[i].y;
        if (x < 0 || x >= img->image.subtex->width || y < 0 || y >= img->image.subtex->height) continue; // skips rogue pixels incase the replacing image is larger or sum
        u32 color = pixelDataList->data[i].color;
        u32 swizzled_pos = ((((y >> 3) * (tex->width >> 3) + (x >> 3)) << 6) +
            ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
                ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3)));
        ((u32*)tex->data)[swizzled_pos] = color;
    }
}

void AllocPixelsFromImages(PixelArray* out, const ImgPos* images, int imageCount, u32 tint)
{
    int totalPixelCapacity = 0;
    for (int i = 0; i < imageCount; i++) {
        C2D_Image* img = images[i].img;
        Tex3DS_SubTexture* subtex = (Tex3DS_SubTexture*)img->subtex;
        totalPixelCapacity += subtex->width * subtex->height;
    }
    //if (out->data) linearFree(out->data);
    out->data = (PixelData*)linearAlloc(sizeof(PixelData) * totalPixelCapacity);
    memset(out->data, 0, sizeof(PixelData) * totalPixelCapacity);
    out->count = 0;

    for (int i = 0; i < imageCount; i++) {
        C3D_Tex* tex = images[i].img->tex;
        Tex3DS_SubTexture* subtex = (Tex3DS_SubTexture*)images[i].img->subtex;
        int offsetX = images[i].x_offset;
        int offsetY = images[i].y_offset;
        int width = subtex->width;
        int height = subtex->height;

        int rotated = Tex3DS_SubTextureRotated(subtex);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int src_x, src_y;
                if (!rotated) {
                    src_x = x + (int)(subtex->left * tex->width);
                    src_y = (int)((1.0f - subtex->top) * tex->height) + y;
                }
                else {
                    src_x = y + (int)(subtex->top * tex->width);
                    src_y = (int)((1.0f - subtex->right) * tex->height) + (width - 1 - x);
                }

                u32 offset = ((((src_y >> 3) * (tex->width >> 3) + (src_x >> 3)) << 6) +
                    ((src_x & 1) | ((src_y & 1) << 1) | ((src_x & 2) << 1) |
                        ((src_y & 2) << 2) | ((src_x & 4) << 2) | ((src_y & 4) << 3)));

                u32 color = ((u32*)tex->data)[offset];
                u8 r = ((color >> 24) & 0xFF) * ((tint >> 24) & 0xFF) / 255;
                u8 g = ((color >> 16) & 0xFF) * ((tint >> 16) & 0xFF) / 255;
                u8 b = ((color >> 8) & 0xFF) * ((tint >> 8) & 0xFF) / 255;
                u8 a = (color & 0xFF) * (tint & 0xFF) / 255;

                if (a == 0) continue; // skip fully transparent pixels

                // calculate final coordinates with offsets
                int finalX = x + offsetX;
                int finalY = y + offsetY;
                if (finalX < 0 || finalY < 0 || out->count >= totalPixelCapacity) continue;
                out->data[out->count].x = finalX;
                out->data[out->count].y = finalY;
                out->data[out->count].color = (r << 24) | (g << 16) | (b << 8) | a;
                out->count++;
            }
        }
    }
}

void DrawOnCanvas(MemoryImage* img, u16 X, u16 Y, u16 lastTouchX, u16 lastTouchY, u8 pen, u32** rawMask, u8 canvX, u8 canvY, PixelArray* pixels, bool wasDrawing)
{
    u16 maskWidth = img->image.subtex->width;
    u16 maskHeight = img->image.subtex->height;

    if (!wasDrawing) {
        lastTouchX = X;
        lastTouchY = Y;
    }

    int startX = lastTouchX - canvX;
    int startY = lastTouchY - canvY;
    int endX = X - canvX;
    int endY = Y - canvY;

    //if (startX < 0 || startY < 0 || startX >= maskWidth || startY >= maskHeight) {  }
    //if (endX < 0 || endY < 0 || endX >= maskWidth || endY >= maskHeight) return;

    pixels->count = 0;

    int drawOffsets[9][2] = {
        {0, 0}, // center
        {-1, 0}, {0, -1}, {-1, -1}, // pen 2
        {0, 1}, {1, 0}, {1, 1}, {1, 1}, {1, -1} // pen 3
    };

    int start = 0, end = 0;
    if (pen == 1) { start = 0; end = 1; }
    else if (pen == 2) { start = 0; end = 4; }
    else if (pen == 3) { start = 0; end = 9; }
    else return; // pen types 4+ not handled yet

    int dx = endX - startX;
    int dy = endY - startY;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (steps == 0) steps = 1;

    float stepX = dx / (float)steps;
    float stepY = dy / (float)steps;

    float x = startX;
    float y = startY;

    for (int s = 0; s <= steps; s++) {
        int px = (int)(x + 0.5f);
        int py = (int)(y + 0.5f);

        for (int i = start; i < end; i++) {
            int drawX = px + drawOffsets[i][0];
            int drawY = py - drawOffsets[i][1];

            if (drawX >= 0 && drawX < maskWidth && drawY >= 0 && drawY < maskHeight) {
                int pixelIndex = drawY * maskWidth + drawX;
                if ((((*rawMask)[pixelIndex] >> 24) & 0xFF) != 0xFF) continue; // mask pixel must be full opacity

                if (pixels->count < 3200) { // hard limit protection
                    pixels->data[pixels->count].x = drawX;
                    pixels->data[pixels->count].y = drawY;
                    pixels->data[pixels->count].color = 0x000000FF;
                    pixels->count++;
                }
            }
        }
        x += stepX;
        y += stepY;
    }
}