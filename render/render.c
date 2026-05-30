#include "render.h"
#include "../editor/editor.h"

void
rows(struct abuf *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++){
	int filerow = y + E.rowoff;
	if (filerow >= E.nrows) {
	    if (E.nrows == 0 && y == E.screenrows / 3) {
		char welcome[80];
		int welcomelen = snprintf(welcome, sizeof(welcome), "Low Code");
		if (welcomelen > E.screencols) welcomelen = E.screencols;
		int padding = (E.screencols - welcomelen) / 2;
		if (padding) 
		{
		    abAppend(ab, "~", 1);
		    padding--;
		}
		while (padding--) abAppend(ab, " ", 1);
		    abAppend(ab, welcome, welcomelen);
		} else {
		abAppend(ab, "~", 1);
		}
	    } else {
	    int len = E.row[filerow].rszs - E.coloff;
	    if (len < 0) len = 0;
	    if (len > E.screencols) len = E.screencols;
		abAppend(ab, &E.row[filerow].render[E.coloff], len);
	    }

	    abAppend(ab, "\x1b[K", 3);
	    abAppend(ab, "\r\n", 2);
    }
}

int
edit_rowto_cxtorx(erow *row, int cx)
{
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (TAB_STOP - 1) - (rx % TAB_STOP);
    rx++;
  }
  return rx;
}

void
app_row(char *s, size_t len) 
{
    E.row = realloc(E.row, sizeof(erow) * (E.nrows + 1));

    int at = E.nrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);

    memcpy(E.row[at].chars, s, len);

    E.row[at].chars[len] = '\0';
    E.row[at].rszs = 0;
    E.row[at].render = NULL;
    E.nrows++;
}

void
refresh_screen()
{
    ed_scroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    if (E.file_mode)
        fp_render(&ab);
    else
        rows(&ab);

    ed_statusbar(&ab);
    ed_cmd_bar(&ab);

    char buf[32];

    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
                                            (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);
    
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void
fp_render(struct abuf *ab)
{
    int drawn = 0;
    for (int i = 0; i < E.file_count && drawn < E.screenrows; i++) {
        if (E.file_qlen > 0 && !strstr(E.file_list[i], E.file_query))
            continue;

        char line[256];
        int len;
        if (i == E.file_sel)
            len = snprintf(line, sizeof(line), "> %s", E.file_list[i]);
        else
            len = snprintf(line, sizeof(line), "  %s", E.file_list[i]);

	if (E.file_count == 0) {
	    abAppend(ab, "  (empty)\x1b[K\r\n", 14);
	    for (int i = 1; i < E.screenrows; i++)
		abAppend(ab, "~\x1b[K\r\n", 7);
	    return;
	}

        if (len > E.screencols) len = E.screencols;
        abAppend(ab, line, len);
        abAppend(ab, "\x1b[K\r\n", 5);
        drawn++;
    }

    while (drawn++ < E.screenrows)
        abAppend(ab, "~\x1b[K\r\n", 7);
}
