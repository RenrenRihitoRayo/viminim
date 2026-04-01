#pragma once

#include <stdbool.h>
#include <stdlib.h>

typedef enum { NORMAL, INSERT, COMMAND } Mode;

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