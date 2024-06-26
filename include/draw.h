#ifndef DRAW_H
#define DRAW_H

#include <openssl/ssl.h>
#include <ncurses.h>

#include "color.h"
#include "state.h"

#define LINESMIN 23
#define COLSMIN 75
#define NEWTESTLINESMIN 46

void mvwaddnstrtab(WINDOW *win, int y, int x, const char *str, int n);

void draw_border(WINDOW *win, struct color *bg, struct color *upper, struct color *lower, int fill, int ymin, int xmin, int ysize, int xsize);

void draw_resize(struct state *st);

void draw_fill(WINDOW *win, struct color *bg, int ymin, int xmin, int ysize, int xsize);

#endif
