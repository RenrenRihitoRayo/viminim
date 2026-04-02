#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SEARCH_BUFFER_SIZE 256

typedef enum { NORMAL, INSERT, COMMAND, SEARCH, BUFFER_SWITCH } Mode;

typedef enum {
    F_BLUE = 1,
    F_RED,
    F_GREEN,
    F_CYAN,
    F_YELLOW,
    F_MAGENTA,
    B_RED,
    F_TEAL,
} Colors;

typedef struct {
    char** command;
    int argc;
} Command;

typedef struct {
    size_t line_num;
    size_t* col_positions;
    size_t match_count;
} SearchResult;

typedef struct {
    SearchResult* results;
    size_t count;
    size_t capacity;
} SearchResultList;

typedef struct {
    char pattern[SEARCH_BUFFER_SIZE];
    size_t last_found_line;
    size_t last_found_col;
    bool found;
    bool direction_forward;
} SearchState;

typedef enum {
    HL_STATE_NORMAL = 0,
    HL_STATE_BLOCK_COMMENT,
    HL_STATE_STRING,
    HL_STATE_CHAR,
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

typedef struct Editor Editor;
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
    Editor*     editor;
} Buffer;

// Value corresponds to color pair
typedef enum {
    IMPORTANT_ERROR = 7,
    ERROR = 2,
    WARNING = 5,
    INFO = 0
} MessageSeverity;

typedef struct {
    char* message;
    MessageSeverity level;
} Message;

struct Editor {
    // Buffers
    Buffer **buffers;
    size_t   count;
    size_t   capacity;
    size_t   current;
    
    // Logic
    size_t   cx, cy;
    size_t   scroll_x, scroll_y, num_padding;
    
    // Editing
    Mode     mode;
    bool     show_line_numbers;
    int      wrap_width;
    
    // commands
    char     command[1024];
    size_t   cmd_len;
    
    // messages
    Message  message[100];
    size_t   message_ptr;
    
    // search
    size_t  *matched_lines;
    size_t   matches;
    size_t   cur_match;
    bool     search_d;
};

SyntaxHL find_syntax_w_name(const char* name);
SyntaxHL find_syntax_w_ext(const char* name);
void rehighlight(Buffer *b);
void rehighlight_from(Buffer *b, size_t row);
void move_cursor_to(Editor *e, size_t new_cy, size_t new_cx);
int editor_add_buffer(Editor *e, Buffer *b);
void insert_char(Editor *e, char c);
void insert_newline(Editor *e);
void backspace_char(Editor *e);
void draw(Editor *e);
void save_buffer(Buffer *b);
void handle_command(Editor *e);
void handle_input(Editor *e, int ch);
Message get_message(Editor* e);
void push_message(Editor* e, char* msg);
