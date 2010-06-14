
#ifndef __DRAW_H
#define __DRAW_H

#define UP_KEY 1
#define DOWN_KEY 2
#define LEFT_KEY 3
#define RIGHT_KEY 4
#define OK_KEY 5
#define CANCEL_KEY 6
void set_draw_color(u32 fgcol, u32 bgcol);
void video_drawstring (int xx, int yy, char *s);
void clear_screen();
void lcd_printf(int x, int y, const char *fmt, ...);

int get_keypress();

#endif
