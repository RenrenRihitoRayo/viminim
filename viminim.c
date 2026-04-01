#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "vcommons.h"
#include "parsers/c.c"
#include "parsers/mila.c"

HLList hl_list[] = {
    {"c", (char*[]){"c", "h", "i", NULL}, highlight_c_line},
    {"mila", (char*[]){"mila", NULL}, highlight_mila_line},
    {NULL, NULL, NULL}
};

SyntaxHL find_syntax_w_name(char* name) {
    for (int t=0; hl_list[t].name; ++t) {
        if (strcmp(name, hl_list[t].name) == 0) {
            return hl_list[t].hltr;
        }
    }
    return NULL;
}

SyntaxHL find_syntax_w_ext(char* name) {
    for (int t=0; hl_list[t].name; ++t) {
        for (int j=0; hl_list[t].extensions[j]; ++j) {
            if (strcmp(name, hl_list[t].extensions[j]) == 0) {
                return hl_list[t].hltr;
            }
        }
    }
    return NULL;
}

static void rehighlight(Buffer *b)
{
    if ((!b->syntax) && !b->hltr) return;
    if (!b->colors)      b->colors      = calloc(b->capacity, sizeof(ColorSpan *));
    if (!b->line_states) b->line_states = calloc(b->capacity, sizeof(HLState));

    HLState state = HL_STATE_NORMAL;
    for (size_t i = 0; i < b->line_count; i++) {
        b->line_states[i] = state;
        free(b->colors[i]);
        state = b->hltr(b->lines[i], state, &b->colors[i]);
    }
}

static void rehighlight_from(Buffer *b, size_t row)
{
    if (!b->syntax || strcmp(b->syntax, "c") != 0) return;
    if (!b->colors)      b->colors      = calloc(b->capacity, sizeof(ColorSpan *));
    if (!b->line_states) b->line_states = calloc(b->capacity, sizeof(HLState));

    HLState state = (row == 0) ? HL_STATE_NORMAL : b->line_states[row];
    for (size_t i = row; i < b->line_count; i++) {
        HLState prev_entry = b->line_states[i];
        b->line_states[i] = state;
        free(b->colors[i]);
        state = highlight_c_line(b->lines[i], state, &b->colors[i]);
        if (i+1 < b->line_count &&
            state == b->line_states[i+1] &&
            prev_entry == b->line_states[i])
            break;
    }
}

void move_cursor_to(Editor *e, size_t new_cy, size_t new_cx)
{
    Buffer *b = e->buffers[e->current];
    if (b->line_count == 0) { e->cy = e->cx = 0; return; }
    if (new_cy >= b->line_count) new_cy = b->line_count - 1;
    e->cy = new_cy;
    size_t ll = strlen(b->lines[e->cy]);
    if (new_cx > ll) new_cx = ll;
    e->cx = new_cx;

    int max_rows = LINES - 1;
    int max_cols = COLS;
    int lnw = e->show_line_numbers ? e->num_padding : 0;

    if (e->cy < e->scroll_y) e->scroll_y = e->cy;
    else if (e->cy >= e->scroll_y + (size_t)max_rows)
        e->scroll_y = e->cy - max_rows + 1;

    if (e->cx < e->scroll_x) e->scroll_x = e->cx;
    else if (e->cx >= e->scroll_x + (size_t)(max_cols - lnw))
        e->scroll_x = e->cx - (max_cols - lnw) + 1;
}

Buffer *buffer_create(const char *name)
{
    Buffer *b    = malloc(sizeof(Buffer));
    b->capacity  = 100;
    b->lines     = malloc(sizeof(char *)      * b->capacity);
    b->colors    = calloc(b->capacity, sizeof(ColorSpan *));
    b->line_states = calloc(b->capacity, sizeof(HLState));
    b->name      = strdup(name);
    b->changed   = false;
    b->syntax    = NULL;
    b->lines[0]  = strdup("");
    b->line_count = 1;
    b->hltr      = NULL;
    return b;
}

Editor *editor_create()
{
    Editor *e   = malloc(sizeof(Editor));
    e->buffers  = malloc(sizeof(Buffer *) * 10);
    e->count    = 0;
    e->capacity = 10;
    e->current  = 0;
    e->cx = e->cy = 0;
    e->scroll_x = e->scroll_y = 0;
    e->mode     = NORMAL;
    e->show_line_numbers = true;
    e->num_padding = 2;
    e->wrap_width = 0;
    e->cmd_len  = 0;
    e->command[0] = '\0';
    return e;
}

Buffer *create_file_buffer(const char *filename)
{
    Buffer *b = buffer_create(filename);

    const char *dot = strrchr(filename, '.');
    if (dot) {
        b->hltr = find_syntax_w_ext(dot+1);
        if (b->hltr) b->syntax = strdup(dot+1);
    }

    FILE *f = fopen(filename, "r");
    if (!f) return b;

    free(b->lines[0]);
    b->line_count = 0;

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, f)) != -1) {
        if (line[read - 1] == '\n') line[read - 1] = '\0';
        if (b->line_count >= b->capacity) {
            size_t old = b->capacity;
            b->capacity *= 2;
            b->lines       = realloc(b->lines,       sizeof(char *)      * b->capacity);
            b->colors      = realloc(b->colors,      sizeof(ColorSpan *) * b->capacity);
            b->line_states = realloc(b->line_states, sizeof(HLState)     * b->capacity);
            memset(b->colors      + old, 0, sizeof(ColorSpan *) * (b->capacity - old));
            memset(b->line_states + old, 0, sizeof(HLState)     * (b->capacity - old));
        }
        b->lines[b->line_count]       = strdup(line);
        b->colors[b->line_count]      = NULL;
        b->line_states[b->line_count] = HL_STATE_NORMAL;
        b->line_count++;
    }
    free(line);
    fclose(f);

    if (b->line_count == 0) {
        b->lines[0]       = strdup("");
        b->colors[0]      = NULL;
        b->line_states[0] = HL_STATE_NORMAL;
        b->line_count     = 1;
    }

    b->changed = false;
    rehighlight(b);
    return b;
}

void editor_add_buffer(Editor *e, Buffer *b)
{
    if (e->count >= e->capacity) {
        e->capacity *= 2;
        e->buffers = realloc(e->buffers, sizeof(Buffer *) * e->capacity);
    }
    e->buffers[e->count++] = b;
}

void insert_char(Editor *e, char c)
{
    if (!isprint(c)) return;
    Buffer *b = e->buffers[e->current];

    if (e->cy >= b->line_count) {
        if (b->line_count >= b->capacity) {
            size_t old = b->capacity;
            b->capacity *= 2;
            b->lines       = realloc(b->lines,       sizeof(char *)      * b->capacity);
            b->colors      = realloc(b->colors,      sizeof(ColorSpan *) * b->capacity);
            b->line_states = realloc(b->line_states, sizeof(HLState)     * b->capacity);
            memset(b->colors      + old, 0, sizeof(ColorSpan *) * (b->capacity - old));
            memset(b->line_states + old, 0, sizeof(HLState)     * (b->capacity - old));
        }
        b->lines[b->line_count]       = malloc(2);
        b->lines[b->line_count][0]    = c;
        b->lines[b->line_count][1]    = '\0';
        b->colors[b->line_count]      = NULL;
        b->line_states[b->line_count] = HL_STATE_NORMAL;
        b->line_count++;
        rehighlight_from(b, e->cy);
    } else {
        size_t len = strlen(b->lines[e->cy]);
        b->lines[e->cy] = realloc(b->lines[e->cy], len + 2);
        memmove(b->lines[e->cy] + e->cx + 1, b->lines[e->cy] + e->cx, len - e->cx + 1);
        b->lines[e->cy][e->cx] = c;
        rehighlight_from(b, e->cy);
    }

    b->changed = true;
    move_cursor_to(e, e->cy, e->cx + 1);
}

void insert_newline(Editor *e)
{
    Buffer *b = e->buffers[e->current];

    if (b->line_count >= b->capacity) {
        size_t old = b->capacity;
        b->capacity *= 2;
        b->lines       = realloc(b->lines,       sizeof(char *)      * b->capacity);
        b->colors      = realloc(b->colors,      sizeof(ColorSpan *) * b->capacity);
        b->line_states = realloc(b->line_states, sizeof(HLState)     * b->capacity);
        memset(b->colors      + old, 0, sizeof(ColorSpan *) * (b->capacity - old));
        memset(b->line_states + old, 0, sizeof(HLState)     * (b->capacity - old));
    }

    if (e->cy >= b->line_count) {
        b->lines[b->line_count]       = strdup("");
        b->colors[b->line_count]      = NULL;
        b->line_states[b->line_count] = HL_STATE_NORMAL;
        b->line_count++;
        move_cursor_to(e, e->cy + 1, 0);
        b->changed = true;
        return;
    }

    char *line     = b->lines[e->cy];
    char *new_line = strdup(line + e->cx);
    line[e->cx]    = '\0';
    b->lines[e->cy] = realloc(line, e->cx + 1);

    for (size_t i = b->line_count; i > e->cy + 1; i--) {
        b->lines[i]       = b->lines[i - 1];
        b->colors[i]      = b->colors[i - 1];
        b->line_states[i] = b->line_states[i - 1];
    }

    b->lines[e->cy + 1]       = new_line;
    b->colors[e->cy + 1]      = NULL;
    b->line_states[e->cy + 1] = HL_STATE_NORMAL;
    b->line_count++;

    rehighlight(b);
    move_cursor_to(e, e->cy + 1, 0);
    b->changed = true;
}

void backspace_char(Editor *e)
{
    Buffer *b = e->buffers[e->current];
    if (e->cy >= b->line_count) return;

    if (e->cx > 0) {
        size_t len = strlen(b->lines[e->cy]);
        memmove(b->lines[e->cy] + e->cx - 1,
                b->lines[e->cy] + e->cx,
                len - e->cx + 1);
        e->cx--;
        rehighlight_from(b, e->cy);
        b->changed = true;
    } else if (e->cy > 0) {
        size_t prev_len = strlen(b->lines[e->cy - 1]);
        size_t curr_len = strlen(b->lines[e->cy]);

        b->lines[e->cy - 1] = realloc(b->lines[e->cy - 1], prev_len + curr_len + 1);
        memcpy(b->lines[e->cy - 1] + prev_len, b->lines[e->cy], curr_len + 1);

        free(b->lines[e->cy]);
        free(b->colors[e->cy]);

        for (size_t i = e->cy; i < b->line_count - 1; i++) {
            b->lines[i]       = b->lines[i + 1];
            b->colors[i]      = b->colors[i + 1];
            b->line_states[i] = b->line_states[i + 1];
        }
        b->lines[b->line_count - 1]       = NULL;
        b->colors[b->line_count - 1]      = NULL;
        b->line_states[b->line_count - 1] = HL_STATE_NORMAL;
        b->line_count--;

        e->cy--;
        e->cx = prev_len;
        rehighlight_from(b, e->cy);
        b->changed = true;
    }
}

void draw_line_with_highlights(int y, char *line, ColorSpan *spans,
                                int max_cols, int scroll_x, int x_start)
{
    int len = (int)strlen(line);
    int col = 0;

    while (col < max_cols - x_start) {
        int src = col + scroll_x;
        if (src >= len) break;

        bool in_span = false;
        if (spans) {
            for (int si = 0; spans[si].length > 0; si++) {
                int s0 = (int)spans[si].line_x;
                int s1 = s0 + (int)spans[si].length;
                if (src >= s0 && src < s1) {
                    attron(COLOR_PAIR(spans[si].color));
                    while (col < max_cols - x_start &&
                           col + scroll_x < s1 &&
                           col + scroll_x < len)
                    {
                        mvaddch(y, x_start + col, line[col + scroll_x]);
                        col++;
                    }
                    attroff(COLOR_PAIR(spans[si].color));
                    in_span = true;
                    break;
                }
            }
        }
        if (!in_span) { mvaddch(y, x_start + col, line[src]); col++; }
    }
}

void draw(Editor *e)
{
    clear();
    Buffer *b = e->buffers[e->current];

    int max_rows = LINES - 1;
    int max_cols = COLS;

    if (e->cy < e->scroll_y) e->scroll_y = e->cy;
    else if (e->cy >= e->scroll_y + (size_t)max_rows)
        e->scroll_y = e->cy - max_rows + 1;

    int line_num_width = e->show_line_numbers ? 5 : 0;
    if (e->cx < e->scroll_x) e->scroll_x = e->cx;
    else if (e->cx >= e->scroll_x + (size_t)(max_cols - line_num_width))
        e->scroll_x = e->cx - (max_cols - line_num_width) + 1;

    int num_len = snprintf(NULL, 0, "%zu", b->line_count) + 1;
    e->num_padding = num_len;

    for (size_t i = 0; i < b->line_count; i++) {
        if ((int)i < (int)e->scroll_y) continue;
        int screen_y = (int)i - (int)e->scroll_y;
        if (screen_y >= max_rows) break;

        int x_start = e->show_line_numbers ? num_len + 1 : 0;
        if (e->show_line_numbers)
            mvprintw(screen_y, 0, "%*zu ", num_len, i + 1);

        draw_line_with_highlights(screen_y, b->lines[i],
                                  b->colors ? b->colors[i] : NULL,
                                  max_cols, e->scroll_x, x_start);
    }

    for (int i = (int)(b->line_count - e->scroll_y); i < max_rows; i++)
        mvprintw(i, 0, "%*s", num_len, "~");

    if (e->mode == COMMAND)
    {
        int chars = snprintf(NULL, 0, "[%s MODE] Buffer: %s (%zu lines)",
                             e->mode == INSERT ? "INSERT" : "NORMAL",
                             b->name, b->line_count);
        int offset = snprintf(NULL, 0, "%s %zu, %zu    ", b->syntax ? b->syntax : "???", e->cy + 1, e->cx);
        mvprintw(LINES - 2, 0, "[%s MODE] Buffer: %s (%zu lines)%*s %zu, %zi",
                 e->mode == INSERT ? "INSERT" : "NORMAL",
                 b->name, b->line_count,
                 COLS - (chars + offset), b->syntax ? b->syntax : "???",
                 e->cy + 1, e->cx);
        mvprintw(LINES - 1, 0, ":%s⟨", e->command);
    }
    else {
        int chars = snprintf(NULL, 0, "[%s MODE] Buffer: %s (%zu lines)",
                             e->mode == INSERT ? "INSERT" : "NORMAL",
                             b->name, b->line_count);
        int offset = snprintf(NULL, 0, "%s %zu, %zu    ", b->syntax ? b->syntax : "???", e->cy + 1, e->cx);
        mvprintw(LINES - 1, 0, "[%s MODE] Buffer: %s (%zu lines)%*s %zu, %zi",
                 e->mode == INSERT ? "INSERT" : "NORMAL",
                 b->name, b->line_count,
                 COLS - (chars + offset), b->syntax ? b->syntax : "???",
                 e->cy + 1, e->cx);
    }

    move((int)(e->cy - e->scroll_y), (int)(e->cx - e->scroll_x) + e->num_padding + 1);
    refresh();
}

void save_buffer(Buffer *b)
{
    FILE *f = fopen(b->name, "w");
    if (!f) return;
    for (size_t i = 0; i < b->line_count; i++)
        fprintf(f, "%s\n", b->lines[i]);
    fclose(f);
    b->changed = false;
}

void handle_command(Editor *e)
{
    Buffer *b = e->buffers[e->current];
    char cmd[256];
    strcpy(cmd, e->command);
    bool force = false;
    size_t len = strlen(cmd);
    if (len > 0 && cmd[len - 1] == '!') { force = true; cmd[len - 1] = '\0'; }

    if (strcmp(cmd, "w") == 0)
        save_buffer(b);
    else if (strcmp(cmd, "q") == 0 && (!b->changed || force))
        endwin(), exit(0);
    else if (strcmp(cmd, "wq") == 0)
        save_buffer(b), endwin(), exit(0);
    else if (strncmp(cmd, "l ", 2) == 0)
        move_cursor_to(e, atoi(cmd + 2), 0);
    else if (strncmp(cmd, "syn ", 4) == 0) {
        free(b->syntax);
        const char *arg = cmd + 4;
        if (strcmp(arg, "off") == 0 || strcmp(arg, "none") == 0) {
            b->syntax = NULL;
            if (b->colors)
                for (size_t i = 0; i < b->line_count; i++) {
                    free(b->colors[i]);
                    b->colors[i] = NULL;
                }
        } else {
            SyntaxHL fn = find_syntax_w_name(arg);
            if (fn) {
                b->hltr = fn;
                b->syntax = strdup(arg);
                rehighlight(b);
            }
        }
    }

    e->mode = NORMAL;
    e->cmd_len = 0;
    e->command[0] = '\0';
}

void handle_input(Editor *e, int ch)
{
    if (e->mode == NORMAL) {
        switch (ch) {
        case 'i': e->mode = INSERT; break;
        case 'h': move_cursor_to(e, e->cy, e->cx > 0 ? e->cx - 1 : 0); break;
        case 'l': move_cursor_to(e, e->cy, e->cx + 1); break;
        case 'j': move_cursor_to(e, e->cy + 1, e->cx); break;
        case 'k': move_cursor_to(e, e->cy > 0 ? e->cy - 1 : 0, e->cx); break;
        case 'n': e->show_line_numbers = !e->show_line_numbers; break;
        case ':':
            e->mode = COMMAND;
            e->cmd_len = 0;
            e->command[0] = '\0';
            break;
        }
    } else if (e->mode == INSERT) {
        if      (ch == 27)                         e->mode = NORMAL;
        else if (ch == 10 || ch == KEY_ENTER)      insert_newline(e);
        else if (ch == 127 || ch == KEY_BACKSPACE) backspace_char(e);
        else                                       insert_char(e, ch);
    } else if (e->mode == COMMAND) {
        if (ch == 10 || ch == KEY_ENTER)
            handle_command(e);
        else if (ch == 27) {
            e->mode = NORMAL;
            e->cmd_len = 0;
            e->command[0] = '\0';
        } else if (ch == 127 || ch == KEY_BACKSPACE) {
            if (e->cmd_len > 0) e->command[--e->cmd_len] = '\0';
            else {
                e->command[0] = '\0';
                e->mode = NORMAL;
            }
        } else if (e->cmd_len < sizeof(e->command) - 1) {
            e->command[e->cmd_len++] = ch;
            e->command[e->cmd_len]   = '\0';
        }
    }
}

int main(int argc, char *argv[])
{
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);

    start_color();
    init_pair(1, COLOR_BLUE,    COLOR_BLACK);
    init_pair(2, COLOR_RED,     COLOR_BLACK);
    init_pair(3, COLOR_GREEN,   COLOR_BLACK);
    init_pair(4, COLOR_CYAN,    COLOR_BLACK);
    init_pair(5, COLOR_YELLOW,  COLOR_BLACK);
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK);

    Editor *ed = editor_create();
    const char *filename = argc > 1 ? argv[1] : "viminim.c";
    editor_add_buffer(ed, create_file_buffer(filename));

    int ch;
    while (1) {
        draw(ed);
        ch = getch();
        handle_input(ed, ch);
    }

    endwin();
    return 0;
}