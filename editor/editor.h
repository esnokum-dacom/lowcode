#ifndef EDITOR_H
#define EDITOR_H

#include "../main.h"
#include "../command.h"

void ed_scroll();
void ed_statusbar(struct abuf *ab);
void ed_cmd_bar(struct abuf *ab);

int edit_read_key();
void ed_updtRow(erow *row);

int get_c_pos(int *cols, int *rows);
int get_winsize(int *rows, int *cols);

char *ed_rto_str(int *buflen);
void ed_move_c(int key);

void ed_appendrow(char *s, size_t len);

void ed_freerow(erow *row);

void ed_delrow(int at);
void ed_inrows(int at, char *s, size_t len);
void ed_inrowsch(erow *row, int at, int c);

void ed_rowappndstr(erow *row, char *s, size_t len);
void ed_delrowsch(erow *row, int at);

void ed_inch(int c);

void in_nw();

void ed_delch();

void ed_set_status(const char *fmt, ...);

void ed_exec_cmd(char *cmd, int argc, char *argv[]);

enum Commands parse_command(const char *cmd);

void ed_parse_and_exec(char *input);

void ed_move_word_right();

void ed_move_word_left();

#endif
