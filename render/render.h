#ifndef	RENDER_H
#define	RENDER_H

#include "../main.h"

#define LINENUM_WIDTH 4

void rows(struct abuf *ab);
int edit_rowto_cxtorx(erow *row, int cx);
void refresh_screen();
void app_row(char *s, size_t len);

void fp_render(struct abuf *ab);

#endif 
