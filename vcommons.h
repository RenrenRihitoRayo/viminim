#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum { NORMAL, INSERT, COMMAND } Mode;

typedef struct {
    char** command;
    int argc;
} Command;

typedef enum {
    HL_STATE_NORMAL = 0,
    HL_STATE_BLOCK_COMMENT,
    HL_STATE_ML_STRING,
    HL_STATE_ML
} HLState;

typedef struct {
    size_t line_x;
    size_t length;
    int color;
} ColorSpan;

typedef HLState(*SyntaxHL)(const char* line, HLState state, ColorSpan** out);

typedef struct {
    char* name;
    char** extensions;
    SyntaxHL hltr;
} HLList;

typedef struct {
    char      **lines;
    ColorSpan **colors;
    HLState    *line_states;
    char       *syntax;
    SyntaxHL    hltr;
    size_t      line_count;
    size_t      capacity;
    char       *name;
    bool        changed;
} Buffer;

typedef struct {
    Buffer **buffers;
    size_t   count;
    size_t   capacity;
    size_t   current;
    size_t   cx, cy;
    size_t   scroll_x, scroll_y, num_padding;
    Mode     mode;
    bool     show_line_numbers;
    int      wrap_width;
    char     command[256];
    size_t   cmd_len;
} Editor;

SyntaxHL find_syntax_w_name(const char* name);
SyntaxHL find_syntax_w_ext(const char* name);
void rehighlight(Buffer *b);
void rehighlight_from(Buffer *b, size_t row);
void move_cursor_to(Editor *e, size_t new_cy, size_t new_cx);
void editor_add_buffer(Editor *e, Buffer *b);
void insert_char(Editor *e, char c);
void insert_newline(Editor *e);
void backspace_char(Editor *e);
void draw(Editor *e);
void save_buffer(Buffer *b);
void handle_command(Editor *e);
void handle_input(Editor *e, int ch);
int main(int argc, char *argv[]);
