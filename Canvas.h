// Canvas.h : an in-memory "screen". Draw into a grid of cells, then print once.
// Each cell holds one symbol (a string, because box characters are UTF-8) and a color.
#ifndef CANVAS_H
#define CANVAS_H

#include <iostream>
#include <string>
#include <vector>
using namespace std;

// ANSI color codes
const int COLOR_NONE   = 0;
const int COLOR_RED    = 31; //terminal understand 31 as red
const int COLOR_YELLOW = 33;
const int COLOR_CYAN   = 36;

class Canvas {
    int width, height;
    vector<string> cell;     // symbol of each cell, row by row
    vector<int> color;       // color of each cell

    bool inside(int x, int y) { //true if :x is between 0 and width-1 and y is between 0 and height-1
        return x >= 0 && x < width && y >= 0 && y < height;
    }

public:
    Canvas(int w = 1, int h = 1) {
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        width = w;
        height = h;
        cell.assign(width * height, " ");
        color.assign(width * height, COLOR_NONE);
    }
	//getters
    int getWidth()  { return width; }
    int getHeight() { return height; }

    // what symbol is at (x, y)? 
    string symbolAt(int x, int y) {
        if (!inside(x, y)) return " "; //blank if outside
        return cell[y * width + x];
    }

    // draw one symbol
    void put(int x, int y, const string& symbol, int c = COLOR_NONE) {
        if (!inside(x, y)) return;
        cell[y * width + x] = symbol;
        color[y * width + x] = c;
    }

    // plain ASCII text
    //turns 1 letter into 1 char string
    void text(int x, int y, const string& s, int c = COLOR_NONE) {
        for (int i = 0; i < (int)s.size(); i++)
            put(x + i, y, string(1, s[i]), c);
    }
	//vline draws a vertical line by walking down from y1 to y2
    void vline(int x, int y1, int y2, int c = COLOR_NONE) {
        for (int y = y1; y <= y2; y++) 
			put(x, y, "│", c);
    }
	//hline draws a horizontal line by walking right from x1 to x2
    void hline(int y, int x1, int x2, int c = COLOR_NONE) {
        for (int x = x1; x <= x2; x++) put(x, y, "─", c);
    }

    void drawBox(int x, int y, int w, int h, const string& label, int c) {
        put(x, y, "┌", c); //top left corner
        put(x + w - 1, y, "┐", c); //top right corner
        put(x, y + h - 1, "└", c); //bottom left corner
        put(x + w - 1, y + h - 1, "┘", c); //top right corner
        hline(y, x + 1, x + w - 2, c); //top edge
        hline(y + h - 1, x + 1, x + w - 2, c); //bottom edge
        
        for (int r = 1; r < h - 1; r++) {
            put(x, y + r, "│", c); //left wall
            put(x + w - 1, y + r, "│", c); //right wall
            for (int i = 1; i < w - 1; i++)      // blank inside, so arrows behind the box vanish
                put(x + i, y + r, " ", c);
        }
        int room = w - 4;
        if (room < 0) room = 0;
        text(x + 2, y + 1, label.substr(0, room), c);
    }

    // build the whole picture as one string (colored or plain)
    string toString(bool useColor = true) {
        string out;
        for (int y = 0; y < height; y++) {
            int last = width - 1;                       // start at right end
            while (last >= 0 && cell[y * width + last] == " ") last--;
            for (int x = 0; x <= last; x++) {
                const string& s = cell[y * width + x];
                int c = color[y * width + x];
                if (useColor && c != COLOR_NONE)
                    out += "\033[" + to_string(c) + "m" + s + "\033[0m";   // code, symbol, reset
                else
                    out += s;
            }
            out += "\n";
        }
        return out;
    }

    void print(bool useColor = true) {
        cout << toString(useColor);
    }
};

#endif