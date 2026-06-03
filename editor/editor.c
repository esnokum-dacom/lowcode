#include "editor.h"
#include "../render/render.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include "../command.h"
#include <sys/ioctl.h>
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>

void
ed_scroll()
{
    E.rx = 0;
    if (E.cy < E.nrows) {
	E.rx = edit_rowto_cxtorx(&E.row[E.cy], E.cx);
    }
    if (E.cy < E.rowoff)
    {
	E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows)
    {
	E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff)
    {
	E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + E.screencols)
    {
	E.coloff = E.rx - E.screencols + 1;
    }
}

void
ed_statusbar(struct abuf *ab) 
{
    abAppend(ab, "\x1b[7m", 4);

    char status[80], rstatus[80];

    int len = snprintf(status, sizeof(status), "%.20s - %d lines",E.filen ? E.filen : "[No Name]", E.nrows);

    int rlen = snprintf(rstatus, sizeof(rstatus), "%d-%d", E.cy + 1, E.nrows);

    if (len > E.screencols) len = E.screencols;
	abAppend(ab, status, len);
    while (len < E.screencols) {
	if (E.screencols - len == rlen)
	{
	    abAppend(ab, rstatus, rlen);
	    break;
	} else {
	    abAppend(ab, " ", 1);
	    len++;
	}
    }
    abAppend(ab, "\x1b[m", 3);
}

void
ed_set_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
}


void
cmd_autoc()
{
    E.cmd_sugg[0] = '\0';
    if (E.cmdlen == 0) return;

    char *prefix = "open ";
    int prefixlen = strlen(prefix);
    if (strncmp(E.cmdbuf, prefix, prefixlen) != 0) return;

    char *partial = E.cmdbuf + prefixlen;
    int partallen = E.cmdlen - prefixlen;
    if (partallen <= 0) return;

    DIR *d = opendir(".");
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strncmp(entry->d_name, partial, partallen) == 0) {
            snprintf(E.cmd_sugg, sizeof(E.cmd_sugg), "open %s", entry->d_name);
            closedir(d);
            return;
        }
    }
    closedir(d);
}

void
ed_search()
{
    if (E.search_qlen == 0) return;

    for (int i = 0; i < E.nrows; i++) {
        int row = (E.cy + i) % E.nrows;
        char *match = strstr(E.row[row].render, E.search_pattern);
        if (match) {
            E.cy = row;
            E.cx = match - E.row[row].render;
            E.rowoff = E.cy; 
            return;
        }
    }
}

void
ed_search_next(int dir)
{
    if (E.search_qlen == 0) return;

    for (int i = 1; i <= E.nrows; i++) {
        int row = (E.cy + i * dir + E.nrows) % E.nrows;
        char *match = strstr(E.row[row].render, E.search_pattern);
        if (match) {
            E.cy = row;
            E.cx = match - E.row[row].render;
            E.rowoff = E.cy; 
            return;
        }
    }
    ed_set_status("No matches");
}

int
ed_search_matches()
{
    int count = 0;
    for (int i = 0; i < E.nrows; i++)
    {
	char *p = E.row[i].render;
	while ((p = strstr(p, E.search_pattern)) != NULL)
	{
	    count++;
	    p++;
	}
    }
    return count;
}

void
ed_del_selection()
{
    if (!E.sel_mode) return;
    
    if (E.st_row == E.sb_row) {
        erow *row = &E.row[E.st_row];
        for (int i = E.cx2 - 1; i >= E.cx1; i--)
	{
            ed_delrowsch(row, i);
	    ed_copyrowsch();
	}
        E.cx = E.cx1;
        E.cy = E.st_row;
    } else {
        erow *first = &E.row[E.st_row];
        for (int i = first->size - 1; i >= E.cx1; i--){
            ed_delrowsch(first, i);
	    ed_copyrowsch();
	}
    
	for (int i = E.sb_row; i > E.st_row + 1; i--)
	{
	    ed_delrow(i);
	    ed_copyrowsch();
	}
	
	erow *last = &E.row[E.st_row + 1];
	for (int i = E.cx2 - 1; i >= 0; i--)
	{
	    ed_delrowsch(last, i);
	    ed_copyrowsch();
	}
	
	ed_rowappndstr(&E.row[E.st_row], last->chars, last->size);
	ed_delrow(E.st_row + 1);
	
	E.cx = E.cx1;
	E.cy = E.st_row;
    }

    E.sel_mode = 0;
    E.cx1 = 0;
    E.cx2 = 0;
}

void
ed_del_row_selection()
{
    if (!E.sel_row_mode) return;

    struct abuf ab = ABUF_INIT;
    
    for (int i = E.sb_row; i >= E.st_row; i--)
    {
	abAppend(&ab, E.row[i].chars, E.row[i].size);
	abAppend(&ab, "\n", 1);
	ed_delrow(i);
    }

    ed_copy(ab.b, ab.len);
    E.cy = E.st_row;
    if (E.cy >= E.nrows) E.cy = E.nrows - 1;
    if (E.cy < 0) E.cy = 0;
    E.cx = 0;
    E.sel_row_mode = 0;
    abFree(&ab);
    ed_set_status("Deleted %d lines", E.sb_row - E.st_row + 1);
}

void
ed_cmd_bar(struct abuf *ab)
{
    abAppend(ab, "\r\n", 2);
    abAppend(ab, "\x1b[K", 3);
    if (E.cmd_mode) {
        char line[256];
        int len = snprintf(line, sizeof(line), ":%s", E.cmdbuf);
        if (len > E.screencols) len = E.screencols;
        abAppend(ab, line, len);
	cmd_autoc();
        if (E.cmd_sugg[0] != '\0') {
            abAppend(ab, "\x1b[2m", 4); 
            abAppend(ab, E.cmd_sugg + E.cmdlen,
                     strlen(E.cmd_sugg) - E.cmdlen);
            abAppend(ab, "\x1b[m", 3); 
        }
    } else if (E.search_mode) {
	char line[256];
	int len = snprintf(line, sizeof(line), "/%s", E.search_pattern);
        if (len > E.screencols) len = E.screencols;
	abAppend(ab, line, len);
    } else {
	int len = strlen(E.statusmsg);
	if (len > E.screencols) len = E.screencols;
	abAppend(ab, E.statusmsg, len);
    }
}

int
edit_read_key()
{
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1))!= 1)
    {
	if (nread == -1 && errno != EAGAIN) die("read"); 
    }

  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';

    if (seq[0] == '\x7f') return DEL_WORD_LEFT;

    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
	if (seq[1] == '1') {
	    char seq2[2];
	    if (read(STDIN_FILENO, &seq2[0], 1) != 1) return '\x1b';
	    if (read(STDIN_FILENO, &seq2[1], 1) != 1) return '\x1b';
	    char seq3[1];
	    if (read(STDIN_FILENO, &seq3[0], 1) != 1) return '\x1b';
	    if (seq2[1] == '5') {
		switch (seq3[0]) {
		    case 'C': return CTRL_AR_RIGHT;
		    case 'D': return CTRL_AR_LEFT;
            }
        }
    }
    switch (seq[1]) {
	case 'A': return AR_UP;
	case 'B': return AR_DOWN;
	case 'C': return AR_RIGHT;
	case 'D': return AR_LEFT;
    }
}
    return '\x1b';
  } else {
    return c;
  }
}

void
ed_move_word_right()
{
    if (E.cy >= E.nrows) return;
    erow *row = &E.row[E.cy];
    while (E.cx < row->size && row->chars[E.cx] != ' ')
	E.cx++;
    while (E.cx < row->size && row->chars[E.cx] == ' ')
	E.cx++;
}

void
ed_move_word_left()
{
    if (E.cy >= E.nrows) return;
    erow *row = &E.row[E.cy];

    if (E.cx > 0) E.cx--;
    while (E.cx > 0 && row->chars[E.cx] == ' ') E.cx--;
    while (E.cx > 0 && row->chars[E.cx - 1] != ' ') E.cx--;
}

void
ed_del_word_left()
{
    if (E.cy >= E.nrows) return;
    int end = E.cx;

    ed_move_word_left();
    while (end > E.cx)
	ed_delrowsch(&E.row[E.cy], --end);
}

void
ed_del_row()
{
    if (E.cy >= E.nrows) return;
    ed_delrow(E.cy);
    if (E.cy > 0 && E.cy >= E.nrows) E.cy--;
    E.cx = 0;
}

char
*ed_rto_str(int *buflen)
{
    int totlen = 0;
    int j;
    for (j = 0; j < E.nrows; j++)
	totlen += E.row[j].size + 1;
    
    *buflen = totlen;
    char *buf = malloc(totlen);
    char *p = buf;
    for (j = 0; j < E.nrows; j++) {
	memcpy(p, E.row[j].chars, E.row[j].size);
	p += E.row[j].size;
	*p = '\n';
	p++;
    }
    return buf;
}

void
ed_move_c(int key) 
{
    erow *row = (E.cy >= E.nrows) ? NULL : &E.row[E.cy];

    switch (key) 
    {
	case AR_LEFT:
	    if (E.cx != 0) {
		E.cx--;
	    } else if (E.cy > 0) {
		E.cy--;
		E.cx = E.row[E.cy].size;
	    }
	    break;
	case AR_RIGHT:
	    if (row && E.cx < row->size){
		E.cx++;
	    } else if (row && E.cx == row->size) {
		E.cy++;
		E.cx = 0;
	    }
	    break;
	case AR_UP:
	if (E.cy != 0) {
	    E.cy--;
	}
	break;
	case AR_DOWN:
	    if (E.cy < E.nrows) {
		E.cy++;
	}
    }
    row = (E.cy >= E.nrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen)
	E.cx = rowlen;
}

int
get_c_pos(int *cols, int *rows)
{
    char buf[32];
    unsigned long i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1)
    {
	if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
	if (buf[i] == 'R') break;
    }

    buf[i] = '\0';
    
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

void
ed_updtRow(erow *row) 
{
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
	if (row->chars[j] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs*(TAB_STOP - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++) {
	if (row->chars[j] == '\t') {
	    row->render[idx++] = ' ';
	    while (idx % TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
	row->render[idx++] = row->chars[j];
    }
    }
	row->render[idx] = '\0';
	row->rszs = idx;
}

int
get_winsize(int *rows, int *cols)
{
    struct winsize ws;
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
	if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return 1;
	return get_c_pos(cols, rows);
    }
    else 
    {
	*cols = ws.ws_col;
	*rows = ws.ws_row;
	return 0;
    }
}

void 
ed_appendrow(char *s, size_t len)
{
    E.row = realloc(E.row, sizeof(erow) * (E.nrows + 1));
    int at = E.nrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.nrows++;
    ed_updtRow(&E.row[at]);
}

void
ed_freerow(erow *row)
{
  free(row->render);
  free(row->chars);
}

void
ed_delrow(int at)
{
  if (at < 0 || at >= E.nrows) return;
  ed_freerow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.nrows - at - 1));
  E.nrows--;
}

void
ed_inrows(int at, char *s, size_t len)
{
    if (at < 0 || at > E.nrows) return;
    E.row = realloc(E.row, sizeof(erow) * (E.nrows + 1));

    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.nrows - at));

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rszs = 0;
    E.row[at].render = NULL;
    ed_updtRow(&E.row[at]);
    E.nrows++;
}

void
ed_inrowsch(erow *row, int at, int c)
{
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);

    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    ed_updtRow(row);
}

void
ed_rowappndstr(erow *row, char *s, size_t len) 
{
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  ed_updtRow(row);
}

void
ed_delrowsch(erow *row, int at) 
{
  if (at < 0 || at >= row->size) return;

  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);

  row->size--;
  ed_updtRow(row);
}

void
ed_copy(const char *t, size_t l)
{
    FILE *cp = popen("xclip -selection clipboard", "w");
    if (!cp){
	ed_set_status("clipboard not founded");
	return; 
    }
    fwrite(t, 1, l, cp);
    pclose(cp);
}

void
ed_copyrowsch()
{
    if (!E.sel_mode) return;

    struct abuf ab = ABUF_INIT;

    if (E.st_row == E.sb_row)
    {
	erow *row = &E.row[E.st_row];
	abAppend(&ab, row->chars + E.cx1, E.cx2 - E.cx1);
    } else {
	erow *rowp = &E.row[E.st_row];
	abAppend(&ab, rowp->chars + E.cx1, rowp->size - E.cx1);
	abAppend(&ab, "\n", 1);

	for (int i = E.st_row + 1; i < E.sb_row; i++)
	{
	    abAppend(&ab, E.row[i].chars, E.row[i].size);
	    abAppend(&ab, "\n", 1);
	}
	
	erow *last = &E.row[E.sb_row];
	abAppend(&ab, last->chars, E.cx2);
    }
    ed_copy(ab.b, ab.len);
    abFree(&ab);

    E.sel_mode = 0;
    ed_set_status("Yanked %d words", ab.len);
}

void
ed_copy_row_selection()
{
    if (!E.sel_row_mode) return;

    struct abuf ab = ABUF_INIT;
    
    for (int i = E.st_row; i <= E.sb_row; i++)
    {
	abAppend(&ab, E.row[i].chars, E.row[i].size);
	abAppend(&ab, "\n", 1);
    }

    ed_copy(ab.b, ab.len);
    ed_set_status("Yanked [%d] lines", E.sb_row - E.st_row + 1);
    abFree(&ab);

    E.sel_row_mode = 0;
    E.st_row = -1;
    E.sb_row = -1;
}

void
ed_inch(int c) 
{
  if (E.cy == E.nrows) {
    ed_inrows(E.nrows, " ", 0);
  }
  ed_inrowsch(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void
in_nw()
{
  if (E.cx == 0) {
    ed_inrows(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    ed_inrows(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    ed_updtRow(row);
  }
  E.cy += 1;
  E.cx = 0;
}

void
ed_delch() 
{
    if (E.cy == E.nrows) return;
    if (E.cx == 0 && E.cy == 0) return;

    erow *row = &E.row[E.cy];
    if (E.cx > 0) {
	ed_delrowsch(row, E.cx - 1);
	E.cx--;
    } else {
	E.cx = E.row[E.cy - 1].size;
	ed_rowappndstr(&E.row[E.cy - 1], row->chars, row->size);
	ed_delrow(E.cy);
	E.cy--;
    }
}

enum Commands
parse_command(char *cmd)
{
    if (strcmp(cmd, "quit") == 0)
        return QUIT;
    if (strcmp(cmd, "write") == 0)
        return WRITE;
    if (strcmp(cmd, "explorer") == 0)
        return EXPLORER;
    if (strncmp(cmd, "open", 4) == 0)
	return OPEN;
    if (strncmp(cmd, "new", 3) == 0) 
	return NEW;
    if (strncmp(cmd, "clear", 3) == 0)
	return CLEAR;
    return -1;
}

void
ed_exec_cmd(char *cmd)
{
    switch (parse_command(cmd)){
	case QUIT:
	    write(STDOUT_FILENO, "\x1b[2J", 4);
	    write(STDOUT_FILENO, "\x1b[H", 3);
	    exit(0);
	    break;
	case WRITE:
	    saveD();
	    write(STDOUT_FILENO, "\x1b[2J", 4);
	    write(STDOUT_FILENO, "\x1b[H", 3);
	    ed_set_status("File writted");
	    break;
	case EXPLORER:
	    fp_load();
	    E.file_mode = 1;
	    E.file_query[0] = '\0';
	    E.file_qlen = 0;
	    ed_set_status("Explorer: Type file name");
	    break;
	case OPEN:
	    char *fl = strchr(cmd, ' ');
	    if (fl) {
		fl++; 
		openD(fl);
		ed_set_status("%s opened successfully", fl);
	    } else {
		ed_set_status("Use: open <filename>");
	    }
	    break;
	case NEW:
	    char *fln = cmd + 4;
	    int fd = open(fln, O_CREAT | O_WRONLY, 0644);
	    if (fd != -1) {
		close(fd);
		openD(fln);
		ed_set_status("%s created", fln);
	    } else {
		ed_set_status("Error creating file");
	    }
	    break;
	case CLEAR:
	    E.search_pattern[0] = '\0';
	    E.search_qlen = 0;
	    ed_set_status("Matches cleared [%d]", E.search_qlen);
	    break;
    }
}

void
ed_parse_and_exec(char *input)
{
    int argc = 0;

    char *tok = strtok(input, " ");
    char *cmd = tok;

    tok = strtok(NULL, " ");
    while (tok != NULL && argc < 15) {
        tok = strtok(NULL, " ");
    }

    ed_exec_cmd(cmd);
}
