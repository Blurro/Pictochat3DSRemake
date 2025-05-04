#include <iostream>
#include <conio.h>
#include <windows.h>

struct CanvasEntry {
    int id;
    int height;
};

#define MAX_BACKLOG 50
#define MAX_LOADED 35
#define MAX_TOTAL_HEIGHT 840
#define LOADED_BELOW_VIEW 3

CanvasEntry backlog[MAX_BACKLOG];
CanvasEntry loaded[MAX_LOADED];
int loadedCount = 0;
int backlogStartIndex = 0;
int lookingAtIndex = 0;

int main() {
    // initialize backlog with random heights
    for (int i = 0; i < MAX_BACKLOG; ++i) {
        backlog[i].id = i;
        backlog[i].height = 105; // height between 40 and 105
    }

    std::cout << "Press UP / DOWN to scroll. ESC to quit.\n";

    loadedCount = 0;
    int totalHeight = 0;
    for (int i = backlogStartIndex; i < MAX_BACKLOG && loadedCount < MAX_LOADED; ++i) {
        if (totalHeight + backlog[i].height > MAX_TOTAL_HEIGHT) break;
        loaded[loadedCount++] = backlog[i];
        totalHeight += backlog[i].height;
    }

    while (true) {

        // input loop
        while (!_kbhit()) Sleep(10);
        int ch = _getch();
        if (ch == 27) break; // ESC
        if (ch == 0 || ch == 224) {
            ch = _getch();
            if (ch == 72) { // up
                if (lookingAtIndex < MAX_BACKLOG - 1) {
                    lookingAtIndex++;
                }
                if (lookingAtIndex > backlogStartIndex + LOADED_BELOW_VIEW) { // ensure backlogStartIndex is 2 items behind
                    backlogStartIndex = lookingAtIndex - LOADED_BELOW_VIEW;
                }
            }
            else if (ch == 80) { // down
                if (lookingAtIndex > 0) {
                    lookingAtIndex--;
                }
                if (lookingAtIndex < backlogStartIndex + LOADED_BELOW_VIEW) {
                    if (lookingAtIndex >= LOADED_BELOW_VIEW)
                        backlogStartIndex = lookingAtIndex - LOADED_BELOW_VIEW;
                    else
                        backlogStartIndex = 0;
                }
            }
        }
        loadedCount = 0;
        int totalHeight = 0;
        for (int i = backlogStartIndex; i < MAX_BACKLOG && loadedCount < MAX_LOADED; ++i) {
            if (totalHeight + backlog[i].height > MAX_TOTAL_HEIGHT) break;
            loaded[loadedCount++] = backlog[i];
            totalHeight += backlog[i].height;
        }

        system("cls");

        // print loaded state with '<--' marking the lookingAtIndex
        std::cout << "Loaded canvases (start index " << backlogStartIndex << "):\n";
        for (int i = loadedCount - 1; i >= 0; --i) {
            int idx = i + backlogStartIndex;  // Calculate the actual index in the backlog
            if (idx == lookingAtIndex) {
                std::cout << "<-- ";  // Show the marker for the 'looking at' item
            }
            std::cout << "#" << loaded[i].id << " (" << loaded[i].height << " px)\n";
        }
        std::cout << "Total loaded height: " << totalHeight << " px\n";
    }

    return 0;
}