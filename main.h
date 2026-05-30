#ifndef MAIN_H
#define MAIN_H

// libs

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>


// defines

#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT   {NULL, 0}
#define TAB_STOP    8

// structs

typedef struct erow {
    int size;
    int rszs;
    char *chars;
    char *render;
} erow;

struct abuf
{
    char *b;
    int len;
};

struct ed_conf
{
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
    int screencols;
    int screenrows;
    int nrows;
    int cmd_mode;
    char cmdbuf[256];
    int cmdlen;
    erow *row;
    char *filen;

    int	file_mode;
    char **file_list;
    int file_count;
    int file_sel;
    char file_query[128];
    int file_qlen;

    struct termios traw_backup;
};

extern struct ed_conf E;

// enums

enum editorKey {
  BACKSPACE = 127,
  AR_LEFT = 1000,
  AR_RIGHT,
  AR_UP,
  AR_DOWN,
  CTRL_AR_LEFT,
  CTRL_AR_RIGHT,
};

// functions

void die (const char* s);

void abAppend(struct abuf *ab, char *s, int len);
void abFree(struct abuf *ab);

void raw_mode();
void init_ed();

void openD(char *filename);
void saveD();

void fp_load();

void process_key_press();

int main (int argc, char *argv[]);

#endif 
