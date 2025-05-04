import os
from PIL import Image

os.chdir(os.path.dirname(os.path.abspath(__file__)))
img = Image.open("spritesheet2.png").convert("RGBA")
pixels = img.load()

glyphs = []
width, height = img.size

# assume single row, auto-detect x boundaries
start_x = None
for x in range(width):
    column_empty = all(pixels[x, y][3] == 0 for y in range(height))  # check alpha
    if not column_empty and start_x is None:
        start_x = x
    elif column_empty and start_x is not None:
        glyphs.append((start_x, 0, x - start_x, height))
        start_x = None

names = [
    "comma", "dot", "excl", "ques", "col", "semicol", "acute", 
    "leftparan", "rightparan", "leftsqu", "rightsqu", "leftcode", 
    "rightcode", "flipexcl", "flipques", "leftquot", "rightquot", 
    "elips", "apost", "strquote", "at", "hash", "caret", "and", 
    "star", "forslash", "bacslash", "under", "longquot", "tilde", 
    "degree", "straight", "fordia", "bacdia", "bigdot"
]

for i, (x, y, w, h) in enumerate(glyphs):
    glyph_img = img.crop((x, y, x + w, y + h))
    
    # Ensure there are enough names
    if i < len(names):
        # Save the image with the corresponding name from the 'names' list
        glyph_img.save(f"out/char_{names[i]}.png")
    else:
        print(f"Warning: No name available for index {i}")