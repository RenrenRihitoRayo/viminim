#include <stdlib.h>

#include "mila/mila.h"
#define ML_LIB
#include "mila/mila.c"
#include "vcommons_mila.c"
#include "viminim/event_handler.c"

#include "vcommons.h"
#include "parsers/c.c"
#include "parsers/mila.c"
#include "viminim/search.c"
#include "viminim/commands.c"

HLList hl_list[] = {
    {"c", (char *[]){"c", "h", "i", NULL}, highlight_c_line},
    {"mila", (char *[]){"mila", NULL}, highlight_mila_line},
    {"plain", (char *[]){"txt", NULL}, NULL},
    {NULL, NULL, NULL}};

SyntaxHL find_syntax_w_name(const char *name)
{
    for (int t = 0; hl_list[t].name; ++t)
    {
        if (strcmp(name, hl_list[t].name) == 0)
        {
            return hl_list[t].hltr;
        }
    }
    return NULL;
}

SyntaxHL find_syntax_w_ext(const char *name)
{
    for (int t = 0; hl_list[t].name; ++t)
    {
        for (int j = 0; hl_list[t].extensions[j]; ++j)
        {
            if (strcmp(name, hl_list[t].extensions[j]) == 0)
            {
                return hl_list[t].hltr;
            }
        }
    }
    return NULL;
}

int has_changes(Editor *e)
{
    for (int i = 0; i < e->count; ++i)
        if (e->buffers[i]->changed)
            return 1;
    return 0;
}

void rehighlight(Buffer *b)
{
    if (!b->hltr)
        return;
    if (!b->colors)
        b->colors = calloc(b->capacity, sizeof(ColorSpan *));
    if (!b->line_states)
        b->line_states = calloc(b->capacity, sizeof(HLState));

    HLState state = HL_STATE_NORMAL;
    for (size_t i = 0; i < b->line_count; i++)
    {
        b->line_states[i] = state;
        free(b->colors[i]);
        state = b->hltr(b->lines[i], state, &b->colors[i]);
    }
}

void rehighlight_from(Buffer *b, size_t row)
{
    if (!b->hltr)
        return;
    if (!b->colors)
        b->colors = calloc(b->capacity, sizeof(ColorSpan *));
    if (!b->line_states)
        b->line_states = calloc(b->capacity, sizeof(HLState));

    HLState state = (row == 0) ? HL_STATE_NORMAL : b->line_states[row];
    for (size_t i = row; i < b->line_count; i++)
    {
        HLState prev_entry = b->line_states[i];
        b->line_states[i] = state;
        free(b->colors[i]);
        state = b->hltr(b->lines[i], state, &b->colors[i]);
        if (i + 1 < b->line_count &&
            state == b->line_states[i + 1] &&
            prev_entry == b->line_states[i])
            break;
    }
}

void remove_buffer(Editor* e, int id) {
    buffer_free(e->buffers[id]);
    e->count--;
    for (int i=id; i<e->count; ++i)
    {
        e->buffers[i] = e->buffers[i+1];
    }
}

void move_cursor_to(Editor *e, size_t new_cy, size_t new_cx)
{
    Buffer *b = e->buffers[e->current];
    if (b->line_count == 0)
    {
        e->cy = e->cx = 0;
        return;
    }
    if (new_cy >= b->line_count)
        new_cy = b->line_count - 1;
    e->cy = new_cy;
    size_t ll = strlen(b->lines[e->cy]);
    if (new_cx > ll)
        new_cx = ll;
    e->cx = new_cx;

    int max_rows = LINES - 1;
    int max_cols = COLS;
    int lnw = e->show_line_numbers ? e->num_padding : 0;

    if (e->cy < e->scroll_y)
    {
        e->scroll_y = e->cy;
        erase();
        draw(e);
    }
    else if (e->cy >= e->scroll_y + (size_t)max_rows)
    {
        e->scroll_y = e->cy - max_rows + 1;
        erase();
        draw(e);
    }

    if (e->cx < e->scroll_x)
    {
        e->scroll_x = e->cx;
        erase();
        draw(e);
    }
    else if (e->cx >= e->scroll_x + (size_t)(max_cols - lnw))
    {
        e->scroll_x = e->cx - (max_cols - lnw) + 1 + 2;
        erase();
        draw(e);
    }
}

Buffer *buffer_create(const char *name)
{
    Buffer *b = malloc(sizeof(Buffer));
    b->capacity = 100;
    b->lines = malloc(sizeof(char *) * b->capacity);
    b->colors = calloc(b->capacity, sizeof(ColorSpan *));
    b->line_states = calloc(b->capacity, sizeof(HLState));
    b->name = strdup(name);
    b->changed = false;
    b->syntax = NULL;
    b->lines[0] = strdup("");
    b->line_count = 1;
    b->hltr = NULL;
    b->editor = NULL;
    return b;
}

void buffer_free(Buffer* b)
{
    for (size_t line = 0; line < b->line_count; ++line)
        free(b->lines[line]);
    free(b->lines);
    free(b->colors);
    free(b->line_states);
    free(b->name);
    free(b);
}

Editor *editor_create()
{
    Editor *e = malloc(sizeof(Editor));
    e->buffers = malloc(sizeof(Buffer *) * 10);
    e->count = 0;
    e->capacity = 10;
    e->current = 0;
    e->cx = e->cy = 0;
    e->scroll_x = e->scroll_y = 0;
    e->mode = NORMAL;
    e->show_line_numbers = true;
    e->num_padding = 2;
    e->wrap_width = 0;
    e->cmd_len = 0;
    e->command[0] = '\0';
    memset(e->message, 0, sizeof(e->message));
    e->message_ptr = 0;
    e->matched_lines = NULL;
    e->cur_match = 0;
    e->search_d = 0; //forward
    return e;
}

void editor_free(Editor* e)
{
    for (int i=0; i<e->count; ++e)
        buffer_free(e->buffers[i]);
    free(e->buffers);
    while (e->message_ptr)
    {
        Message m = get_message(e);
        free(m.message);
    }
    free(e);
}

Buffer *create_file_buffer(const char *filename)
{
    Buffer *b = buffer_create(filename);

    const char *dot = strrchr(filename, '.');
    if (dot)
    {
        b->hltr = find_syntax_w_ext(dot + 1);
        if (b->hltr)
            b->syntax = strdup(dot + 1);
    }

    FILE *f = fopen(filename, "r");
    if (!f)
        return b;

    free(b->lines[0]);
    b->line_count = 0;

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, f)) != -1)
    {
        if (line[read - 1] == '\n')
            line[read - 1] = '\0';
        if (b->line_count >= b->capacity)
        {
            size_t old = b->capacity;
            b->capacity *= 2;
            b->lines = realloc(b->lines, sizeof(char *) * b->capacity);
            b->colors = realloc(b->colors, sizeof(ColorSpan *) * b->capacity);
            b->line_states = realloc(b->line_states, sizeof(HLState) * b->capacity);
            memset(b->colors + old, 0, sizeof(ColorSpan *) * (b->capacity - old));
            memset(b->line_states + old, 0, sizeof(HLState) * (b->capacity - old));
        }
        b->lines[b->line_count] = strdup(line);
        b->colors[b->line_count] = NULL;
        b->line_states[b->line_count] = HL_STATE_NORMAL;
        b->line_count++;
    }
    free(line);
    fclose(f);

    if (b->line_count == 0)
    {
        b->lines[0] = strdup("");
        b->colors[0] = NULL;
        b->line_states[0] = HL_STATE_NORMAL;
        b->line_count = 1;
    }

    b->changed = false;
    rehighlight(b);
    return b;
}

int editor_add_buffer(Editor *e, Buffer *b)
{
    if (e->count >= e->capacity)
    {
        e->capacity *= 2;
        e->buffers = realloc(e->buffers, sizeof(Buffer *) * e->capacity);
    }
    b->editor = e;
    e->buffers[e->count++] = b;
    return e->count - 1;
}

void insert_char(Editor *e, char c)
{
    if (!isprint(c))
        return;
    Buffer *b = e->buffers[e->current];

    if (e->cy >= b->line_count)
    {
        if (b->line_count >= b->capacity)
        {
            size_t old = b->capacity;
            b->capacity *= 2;
            b->lines = realloc(b->lines, sizeof(char *) * b->capacity);
            b->colors = realloc(b->colors, sizeof(ColorSpan *) * b->capacity);
            b->line_states = realloc(b->line_states, sizeof(HLState) * b->capacity);
            memset(b->colors + old, 0, sizeof(ColorSpan *) * (b->capacity - old));
            memset(b->line_states + old, 0, sizeof(HLState) * (b->capacity - old));
        }
        b->lines[b->line_count] = malloc(2);
        b->lines[b->line_count][0] = c;
        b->lines[b->line_count][1] = '\0';
        b->colors[b->line_count] = NULL;
        b->line_states[b->line_count] = HL_STATE_NORMAL;
        b->line_count++;
        rehighlight_from(b, e->cy - 1);
    }
    else
    {
        size_t len = strlen(b->lines[e->cy]);
        b->lines[e->cy] = realloc(b->lines[e->cy], len + 2);
        memmove(b->lines[e->cy] + e->cx + 1, b->lines[e->cy] + e->cx, len - e->cx + 1);
        b->lines[e->cy][e->cx] = c;
        rehighlight_from(b, e->cy);
    }

    b->changed = true;
    move_cursor_to(e, e->cy, e->cx + 1);
}

void insert_string(Editor *e, char *string)
{
    while (*string)
    {
        if (*string == 10)
            insert_newline(e);
        else
            insert_char(e, *string);
        string++;
    }
}

void insert_newline(Editor *e)
{
    Buffer *b = e->buffers[e->current];

    if (b->line_count >= b->capacity)
    {
        size_t old = b->capacity;
        b->capacity *= 2;
        b->lines = realloc(b->lines, sizeof(char *) * b->capacity);
        b->colors = realloc(b->colors, sizeof(ColorSpan *) * b->capacity);
        b->line_states = realloc(b->line_states, sizeof(HLState) * b->capacity);
        memset(b->colors + old, 0, sizeof(ColorSpan *) * (b->capacity - old));
        memset(b->line_states + old, 0, sizeof(HLState) * (b->capacity - old));
    }

    if (e->cy >= b->line_count)
    {
        b->lines[b->line_count] = strdup("");
        b->colors[b->line_count] = NULL;
        b->line_states[b->line_count] = HL_STATE_NORMAL;
        b->line_count++;
        move_cursor_to(e, e->cy + 1, 0);
        b->changed = true;
        erase();
        draw(e);
        return;
    }

    char *line = b->lines[e->cy];
    char *new_line = strdup(line + e->cx);
    line[e->cx] = '\0';
    b->lines[e->cy] = realloc(line, e->cx + 1);

    for (size_t i = b->line_count; i > e->cy + 1; i--)
    {
        b->lines[i] = b->lines[i - 1];
        b->colors[i] = b->colors[i - 1];
        b->line_states[i] = b->line_states[i - 1];
    }

    b->lines[e->cy + 1] = new_line;
    b->colors[e->cy + 1] = NULL;
    b->line_states[e->cy + 1] = HL_STATE_NORMAL;
    b->line_count++;

    rehighlight(b);
    move_cursor_to(e, e->cy + 1, 0);
    b->changed = true;
    erase();
    draw(e);
}

void backspace_char(Editor *e)
{
    Buffer *b = e->buffers[e->current];
    if (e->cy >= b->line_count)
        return;

    if (e->cx > 0)
    {
        size_t len = strlen(b->lines[e->cy]);
        memmove(b->lines[e->cy] + e->cx - 1,
                b->lines[e->cy] + e->cx,
                len - e->cx + 1);
        e->cx--;
        b->changed = true;
        rehighlight_from(b, e->cy);
    }
    else if (e->cy > 0)
    {
        size_t prev_len = strlen(b->lines[e->cy - 1]);
        size_t curr_len = strlen(b->lines[e->cy]);

        b->lines[e->cy - 1] = realloc(b->lines[e->cy - 1], prev_len + curr_len + 1);
        memcpy(b->lines[e->cy - 1] + prev_len, b->lines[e->cy], curr_len + 1);

        free(b->lines[e->cy]);
        free(b->colors[e->cy]);

        for (size_t i = e->cy; i < b->line_count - 1; i++)
        {
            b->lines[i] = b->lines[i + 1];
            b->colors[i] = b->colors[i + 1];
            b->line_states[i] = b->line_states[i + 1];
        }
        b->lines[b->line_count - 1] = NULL;
        b->colors[b->line_count - 1] = NULL;
        b->line_states[b->line_count - 1] = HL_STATE_NORMAL;
        b->line_count--;

        e->cy--;
        e->cx = prev_len;
        b->changed = true;
    }

    erase();
    draw(e);
}

void insert_line(Buffer *b, size_t line_num, char *line)
{
    if (!line)
        return;

    // If lines array doesn't exist, allocate it
    if (!b->lines)
    {
        b->capacity = 100;
        b->lines = malloc(sizeof(char *) * b->capacity);
        b->colors = calloc(b->capacity, sizeof(ColorSpan *));
        b->line_states = calloc(b->capacity, sizeof(HLState));
        b->line_count = 0;
    }

    if (line_num > b->line_count)
        line_num = b->line_count;

    // Count how many lines we need to insert (split by newlines)
    int line_count_to_insert = 1;
    for (const char *p = line; *p; p++)
    {
        if (*p == '\n')
            line_count_to_insert++;
    }

    // Ensure we have enough capacity for all new lines
    while (b->line_count + line_count_to_insert > b->capacity)
    {
        size_t old = b->capacity;
        b->capacity *= 2;
        b->lines = realloc(b->lines, sizeof(char *) * b->capacity);
        b->colors = realloc(b->colors, sizeof(ColorSpan *) * b->capacity);
        b->line_states = realloc(b->line_states, sizeof(HLState) * b->capacity);
        memset(b->colors + old, 0, sizeof(ColorSpan *) * (b->capacity - old));
        memset(b->line_states + old, 0, sizeof(HLState) * (b->capacity - old));
    }

    // Shift lines down from line_num onwards to make room for new lines
    for (int i = (int)b->line_count + line_count_to_insert - 1; i >= (int)(line_num + line_count_to_insert); i--)
    {
        b->lines[i] = b->lines[i - line_count_to_insert];
        b->colors[i] = b->colors[i - line_count_to_insert];
        b->line_states[i] = b->line_states[i - line_count_to_insert];
    }

    // Split the input string by newlines and insert each piece
    const char *start = line;
    const char *end;
    size_t insert_pos = line_num;

    while ((end = strchr(start, '\n')) != NULL)
    {
        size_t len = end - start;
        char *new_line = malloc(len + 1);
        if (!new_line)
            return;

        memcpy(new_line, start, len);
        new_line[len] = '\0';

        b->lines[insert_pos] = new_line;
        b->colors[insert_pos] = NULL;
        b->line_states[insert_pos] = HL_STATE_NORMAL;
        insert_pos++;

        start = end + 1;
    }

    // Insert the last piece (after the final newline or if there are no newlines)
    if (*start || start == line)
    {
        b->lines[insert_pos] = strdup(start);
        b->colors[insert_pos] = NULL;
        b->line_states[insert_pos] = HL_STATE_NORMAL;
    }

    b->line_count += line_count_to_insert;

    // Rehighlight from the inserted section onwards
    rehighlight_from(b, line_num);
    b->changed = true;
}

void set_line(Buffer* b, size_t line_num, char* line)
{
    b->changed = 1;
    if (line_num > 0 && line_num < b->line_count) return;
    free(b->lines[line_num]);
    b->lines[line_num] = strdup(line);
}

char* get_line(Buffer* b, size_t line_num)
{
    if (line_num > 0 && line_num < b->line_count) return NULL;
    return b->lines[line_num];
}

void delete_line(Buffer *b, int line_num)
{
    if (line_num < 0 || line_num >= (int)b->line_count)
        return;

    // If it's the last line, just clear it
    if (b->line_count == 1)
    {
        free(b->lines[0]);
        b->lines[0] = strdup("");
        free(b->colors[0]);
        b->colors[0] = NULL;
        b->line_states[0] = HL_STATE_NORMAL;
        b->changed = true;
        return;
    }

    // Free the line being deleted
    free(b->lines[line_num]);
    free(b->colors[line_num]);

    // Shift lines up from line_num onwards
    for (int i = line_num; i < (int)b->line_count - 1; i++)
    {
        b->lines[i] = b->lines[i + 1];
        b->colors[i] = b->colors[i + 1];
        b->line_states[i] = b->line_states[i + 1];
    }

    b->line_count--;
    b->lines[b->line_count] = NULL;
    b->colors[b->line_count] = NULL;

    // Rehighlight from the deletion point onwards
    if (line_num < (int)b->line_count)
        rehighlight_from(b, line_num);

    b->changed = true;
}

void draw_line_with_highlights(int y, char *line, ColorSpan *spans,
                               int max_cols, int scroll_x, int x_start)
{
    int len = (int)strlen(line);
    int col = 0;

    while (col < max_cols - x_start)
    {
        int src = col + scroll_x;
        if (src >= len)
            break;

        bool in_span = false;
        if (spans)
        {
            for (int si = 0; spans[si].length > 0; si++)
            {
                int s0 = (int)spans[si].line_x;
                int s1 = s0 + (int)spans[si].length;
                if (src >= s0 && src < s1)
                {
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
        if (!in_span)
        {
            mvaddch(y, x_start + col, line[src]);
            col++;
        }
    }
}

Message get_message(Editor *e)
{
    return e->message[--e->message_ptr];
}

void push_message(Editor *e, char *msg)
{
    if (!msg)
        return;

    const char *start = msg;
    const char *end;

    while ((end = strchr(start, '\n')) != NULL)
    {
        size_t len = end - start;

        char *line = malloc(len + 1);
        if (!line)
            return;

        memcpy(line, start, len);
        line[len] = '\0';

        e->message[e->message_ptr].level = INFO;
        e->message[e->message_ptr++].message = line;

        start = end + 1;
    }
    if (*start)
    {
        e->message[e->message_ptr].level = INFO;
        e->message[e->message_ptr++].message = strdup(start);
    }
}

void push_message_log(Editor *e, char *msg, MessageSeverity level)
{
    if (!msg)
        return;

    const char *start = msg;
    const char *end;

    while ((end = strchr(start, '\n')) != NULL)
    {
        size_t len = end - start;

        char *line = malloc(len + 1);
        if (!line)
            return;

        memcpy(line, start, len);
        line[len] = '\0';

        e->message[e->message_ptr].level = level;
        e->message[e->message_ptr++].message = line;

        start = end + 1;
    }
    if (*start)
    {
        e->message[e->message_ptr].level = level;
        e->message[e->message_ptr++].message = strdup(start);
    }
}

void draw(Editor *e)
{
    Buffer *b = e->buffers[e->current];
    if (strcmp(b->name, "*") == 0) b->changed = 0;
    if (b->name[0] == '[' && b->name[strlen(b->name)-1] == ']') b->changed = 0;
    int max_rows = LINES - 1;
    int max_cols = COLS;

    if (e->cy < e->scroll_y)
        e->scroll_y = e->cy;
    else if (e->cy >= e->scroll_y + (size_t)max_rows)
        e->scroll_y = e->cy - max_rows + 1;

    int num_len = snprintf(NULL, 0, "%zu", b->line_count) + 1;
    e->num_padding = num_len;

    int line_num_width = e->show_line_numbers ? num_len : 0;
    if (e->cx < e->scroll_x)
        e->scroll_x = e->cx;
    else if (e->cx >= e->scroll_x + (size_t)(max_cols - line_num_width))
        e->scroll_x = e->cx - (max_cols - line_num_width) + 1;

    if (e->mode == BUFFER_SWITCH)
    {
        erase();
        mvprintw(0, (COLS / 2) - strlen("Available Buffers"), "Available Buffers");
        int margin = snprintf(NULL, 0, "%zu", e->count);
        for (size_t buf = 0; buf < e->count; ++buf)
        {
            mvprintw(buf + 2, 2, " %*zu - %s", margin, buf, e->buffers[buf]->name);
        }
    }
    else
    {
        // Draw file content
        for (size_t i = 0; i < b->line_count; i++)
        {
            if ((int)i < (int)e->scroll_y)
                continue;
            int screen_y = (int)i - (int)e->scroll_y;
            if (screen_y >= max_rows)
                break;

            int x_start = e->show_line_numbers ? num_len + 1 : 0;
            if (e->show_line_numbers)
                mvprintw(screen_y, 0, "%*zu ", num_len, i + 1);

            if (i == e->cy)
                clrtoeol();
            draw_line_with_highlights(screen_y, b->lines[i],
                                      b->colors ? b->colors[i] : NULL,
                                      max_cols, e->scroll_x, x_start);
        }

        // Draw tilde markers for empty lines
        for (int i = (int)(b->line_count - e->scroll_y); i < max_rows; i++)
            mvprintw(i, 0, "~%*s", num_len, "");
    }

    // Draw status bar
    move(LINES - 1, 0);
    clrtoeol();

    if (e->mode == COMMAND)
    {
        move(LINES - 2, 0);
        clrtoeol();
        int chars = snprintf(NULL, 0, "%s '%s%s' %zuL",
                             e->mode == INSERT ? "INSERT" : "NORMAL",
                             b->changed ? "*" : "",
                             b->name, b->line_count);
        int offset = snprintf(NULL, 0, "%s %zu, %zu  ",
                              b->syntax ? b->syntax : "???", e->cy + 1, e->cx);
        mvprintw(LINES - 2, 0, "%s '%s%s' %zuL%*s %zu, %zi",
                 e->mode == INSERT ? "INSERT" : "NORMAL",
                 b->changed ? "*" : "",
                 b->name, b->line_count,
                 COLS - (chars + offset), b->syntax ? b->syntax : "???",
                 e->cy + 1, e->cx);
        mvprintw(LINES - 1, 0, ":%s", e->command);
        return;
    }
    else if (e->mode == SEARCH)
    {
        move(LINES - 2, 0);
        clrtoeol();
        int chars = snprintf(NULL, 0, "SEARCH '%s%s' %zuL",
                             b->changed ? "*" : "",
                             b->name, b->line_count);
        int offset = snprintf(NULL, 0, "%s %zu, %zu  ",
                              b->syntax ? b->syntax : "???", e->cy + 1, e->cx);
        mvprintw(LINES - 2, 0, "SEARCH '%s%s' %zuL%*s %zu, %zi",
                 b->changed ? "*" : "",
                 b->name, b->line_count,
                 COLS - (chars + offset), b->syntax ? b->syntax : "???",
                 e->cy + 1, e->cx);

        // Show search direction and pattern
        char search_prompt[256];
        snprintf(search_prompt, sizeof(search_prompt), "%s%s",
                 !e->search_d ? "/" : "?", e->command);
        mvprintw(LINES - 1, 0, "%s", search_prompt);
        refresh();
        return;
    }
    else if (e->mode == BUFFER_SWITCH)
    {
        move(LINES - 2, 0);
        clrtoeol();
        int chars = snprintf(NULL, 0, "BUFFER '%s%s' %zuL",
                             b->changed ? "*" : "",
                             b->name, b->line_count);
        int offset = snprintf(NULL, 0, "%s %zu, %zu  ",
                              b->syntax ? b->syntax : "???", e->cy + 1, e->cx);
        mvprintw(LINES - 2, 0, "BUFFER '%s%s' %zuL%*s %zu, %zi",
                 b->changed ? "*" : "",
                 b->name, b->line_count,
                 COLS - (chars + offset), b->syntax ? b->syntax : "???",
                 e->cy + 1, e->cx);

        // Show search direction and pattern
        char search_prompt[256];
        snprintf(search_prompt, sizeof(search_prompt), "%s", e->command);
        mvprintw(LINES - 1, 0, "%s", search_prompt);
        refresh();
        return;
    }
    else
    {
        // Normal mode status bar
        int chars = snprintf(NULL, 0, "%s '%s%s' %zuL",
                             e->mode == INSERT ? "INSERT" : "NORMAL",
                             b->changed ? "*" : "",
                             b->name, b->line_count);
        int offset = snprintf(NULL, 0, "%s %zu, %zu  ",
                              b->syntax ? b->syntax : "???", e->cy + 1, e->cx);
        mvprintw(LINES - 1, 0, "%s '%s%s' %zuL%*s %zu, %zi",
                 e->mode == INSERT ? "INSERT" : "NORMAL",
                 b->changed ? "*" : "",
                 b->name, b->line_count,
                 COLS - (chars + offset), b->syntax ? b->syntax : "???",
                 e->cy + 1, e->cx);
    }

    if (e->message_ptr)
    {
        int start = 0;
        while (e->message_ptr)
        {
            Message message = get_message(e);
            attron(COLOR_PAIR(message.level));
            if (message.level == IMPORTANT_ERROR)
            {
                int line = LINES / 2;
                int cols = COLS / 2 - strlen(message.message) / 2;
                mvprintw(line - 1, cols, "%*s", (int)strlen(message.message) + 2, "[ERROR]");
                mvprintw(line, cols, " %s ", message.message);
                mvprintw(line + 1, cols, "[ERROR]%*s", (int)strlen(message.message) - 5, "");
                attroff(COLOR_PAIR(message.level));
                while (!isprint(getch()))
                    ; // user must type a character
                clear();
                draw(e);
                continue;
            }
            else
                mvprintw(start, COLS - strlen(message.message), "%s", message.message);
            free(message.message);
            start++;
            attroff(COLOR_PAIR(message.level));
        }
    }

    if (!e->show_line_numbers)
        line_num_width--;
    move((int)(e->cy - e->scroll_y), (int)(e->cx - e->scroll_x) + line_num_width + 1);
    refresh();
}

void save_buffer(Buffer *b)
{
    if (!b->changed)
    {
        push_message_log(b->editor, "Buffer unchanged!", WARNING);
        return;
    }
    if ((!b->name) || (!strlen(b->name)))
    {
        push_message_log(b->editor, "Buffer Unnamed!", ERROR);
        return;
    }
    FILE *f = fopen(b->name, "w");
    if (!f)
        return;
    size_t count = 0;
    for (size_t i = 0; i < b->line_count; i++)
        count += fprintf(f, "%s\n", b->lines[i]);
    int size = snprintf(NULL, 0, "Wrote %zu bytes", count);
    char *str = (char *)malloc(sizeof(char) * size + 1);
    sprintf(str, "Wrote %zu bytes", count);
    push_message(b->editor, str);
    free(str);
    fclose(f);
    b->changed = false;
}

void editor_command(Editor *e, char* cmd)
{
    Buffer* b = e->buffers[e->current];
    bool force = false;
    size_t len = strlen(cmd);
    if (len > 0 && cmd[len - 1] == '!')
    {
        force = true;
        cmd[len - 1] = '\0';
    }

    Command *command_struct = parse_command(cmd);
    char **command = command_struct->command;
    int argc = command_struct->argc;

    if (strcmp(command[0], "w") == 0)
    {
        if (argc == 1)
            save_buffer(b);
        else if (argc == 2)
        {
            b->name = strdup(command[1]);
            save_buffer(b);
        }
    }
    else if (strcmp(command[0], "wq") == 0)
    {
        if (argc == 1)
            save_buffer(b);
        else if (argc == 2)
        {
            b->name = strdup(command[1]);
            save_buffer(b);
        }
        endwin();
        exit(0);
    }
    else if (strcmp(command[0], "q") == 0 && argc == 1)
    {
        if (force || !has_changes(e))
        {
            endwin();
            exit(0);
        }
        else
        {
            push_message_log(e, "Unsaved Buffers!", ERROR);
        }
    }
    else if (strcmp(command[0], "syn") == 0 && argc == 2)
    {
        free(b->syntax);
        char *arg = command[1];
        if (strcmp(arg, "off") == 0 || strcmp(arg, "none") == 0)
        {
            b->syntax = NULL;
            if (b->colors)
                for (size_t i = 0; i < b->line_count; i++)
                {
                    free(b->colors[i]);
                    b->colors[i] = NULL;
                }
        }
        else
        {
            SyntaxHL fn = find_syntax_w_name(arg);
            if (fn)
            {
                b->hltr = fn;
                b->syntax = strdup(arg);
                rehighlight(b);
            }
        }
    }
    else if ((strcmp(command[0], "l") == 0 || strcmp(command[0], "line") == 0) && argc == 2)
    {
        int line = 0;
        if (strcmp(command[1], "end") == 0)
            line = b->line_count - 1;
        else if (strcmp(command[1], "begin") == 0)
            line = 0;
        else if (strcmp(command[1], "e") == 0)
            line = b->line_count - 1;
        else if (strcmp(command[1], "b") == 0)
            line = 0;
        else
            line = atoi(command[1]) - 1;
        if (line < 0)
            line = 0;
        else if (line > b->line_count)
            line = b->line_count - 1;
        move_cursor_to(e, line, 0);
    }
    else if (strcmp(command[0], "new") == 0 || strcmp(command[0], "n") == 0)
    {
        Buffer *b = create_file_buffer(argc == 2 ? command[1] : "");
        int id = editor_add_buffer(e, b);
        e->current = id;
    }
    else if (strcmp(command[0], "close") == 0 && (argc == 1 || argc == 2))
    {
        int id = argc == 2 ? atoi(command[1]) : e->current;
        if (id == e->current && e->count == 1) // closed remaing buffer
        {
            if (e->buffers[0]->changed && !force) {
                push_message_log(e, "Buffer unsaved!", WARNING);
                goto command_clean;
            }
            endwin();
            exit(0);
        }
        if (!(id > 0 && id < e->count))
        {
            push_message_log(e, "No such buffer to remove!", ERROR);
            goto command_clean;
        }
        if (e->buffers[id]->changed && !force)
        {
            push_message_log(e, "Buffer unsaved!", WARNING);
            goto command_clean;
        }
        if (e->current == id && e->current > 0) e->current--;
        remove_buffer(e, id);
    }
    else if (strcmp(command[0], "name") == 0 && argc == 2)
    {
        if (b->name)
            free(b->name);
        b->name = strdup(command[1]);
    }
    else if (strcmp(command[0], "msg") == 0 && argc == 2)
    {
        push_message(e, command[1]);
    }
    else if (strcmp(command[0], "msg") == 0 && argc == 3)
    {
        MessageSeverity level = INFO;
        if (strcmp(command[1], "info") == 0)
            level = INFO;
        if (strcmp(command[1], "warn") == 0)
            level = WARNING;
        if (strcmp(command[1], "error") == 0)
            level = ERROR;
        if (strcmp(command[1], "i_error") == 0)
            level = IMPORTANT_ERROR;
        push_message_log(e, command[2], level);
    }
    else if (strcmp(command[0], "source") == 0 && argc == 2)
    {
        if (run_file(command[1], mila_globals) > 1) {
            editor_free(editor);
            exit(1);
        }
    }
    else if (strcmp(command[0], "t") == 0 && argc >= 1)
    {
        for (int i=1; i<argc; ++i)
            push_message(e, command[i]);
    }
    else if (strcmp(command[0], "r") == 0 && argc == 1)
    {
        char* text = linearize(b->lines, b->line_count);
        Value* res = eval_str(text, mila_globals);
        if (IS_ERROR(res)) {
            push_message_log(e, res->v.message, ERROR);
        }
        val_release(res);
        free(text);
    }
    else if (argc >= 1) { // treat it as a call to a MiLa function
        Value* fn = env_get(mila_globals, command[0]);
        if (MILA_GET_TYPE(fn) == T_FUNCTION || MILA_GET_TYPE(fn) == T_NATIVE) {
            Value** args = (Value**)malloc(sizeof(Value*) * argc-1);
            for (int i=1; i<argc; ++i) args[i-1] = vstring_dup(command[i]);
            Value* res = call_function(fn, mila_globals, argc-1, args);
            val_release(res);
            for (int i=0; i<argc-1; ++i) val_release(args[i]);
        }
    }

command_clean:;
    e->cmd_len = 0;
    e->command[0] = '\0';
    e->mode = NORMAL;
    free_command(command_struct);
}

void handle_command(Editor *e)
{
    Buffer *b = e->buffers[e->current];
    char cmd[1024];
    strcpy(cmd, e->command);
    if (strlen(cmd) == 0)
    {
        e->mode = NORMAL;
        erase();
        return;
    }
    
    if (cmd[0] == '.')
    {
        def_prog_mode();
        endwin();
        Value* res = eval_str(cmd+1, mila_globals);
        if (IS_ERROR(res)) print_value_debug(res);
        val_release(res);
        getch();
        reset_prog_mode();
        refresh();
    }

    editor_command(e, cmd);
    erase();
}

void handle_buffer_switch(Editor *e)
{
    Buffer *b = e->buffers[e->current];
    char buffer[256];
    strcpy(buffer, e->command);
    int id = -1;

    if (buffer[0] == ',')
        id = atoi(buffer + 1);
    if (id != -1 && id < e->count)
    {
        e->current = id;
        erase();
        e->cmd_len = 0;
        e->command[0] = '\0';
        e->mode = NORMAL;
        return;
    }

    for (size_t buf = 0; buf < e->count; ++buf)
    {
        if (strcmp(buffer, e->buffers[buf]->name) == 0)
        {
            e->current = buf;
            goto clean;
        }
    }

    push_message_log(e, "No such buffer found", WARNING);
clean:;
    e->cmd_len = 0;
    e->command[0] = '\0';
    e->mode = NORMAL;
    erase();
}

void handle_input(Editor *e, int ch)
{
    switch (ch)
    {
    case KEY_LEFT:
        move_cursor_to(e, e->cy, e->cx > 0 ? e->cx - 1 : 0);
        break;
    case KEY_RIGHT:
        move_cursor_to(e, e->cy, e->cx + 1);
        break;
    case KEY_DOWN:
        move_cursor_to(e, e->cy + 1, e->cx);
        break;
    case KEY_UP:
        move_cursor_to(e, e->cy > 0 ? e->cy - 1 : 0, e->cx);
        break;
    case KEY_NPAGE:
        move_cursor_to(e, e->cy + LINES, e->cx);
        break;
    case KEY_PPAGE:
        move_cursor_to(e, (ssize_t)(e->cy) - (ssize_t)(LINES) > 0 ? e->cy - LINES : 0, e->cx);
        break;
    case KEY_HOME:
        move_cursor_to(e, e->cy, 0);
        break;
    case KEY_END:
        move_cursor_to(e, e->cy, 9999);
        break;
    default:
        if (e->mode == NORMAL)
        {
            switch (ch)
            {
            case 'i':
                e->mode = INSERT;
                break;
            case 'h':
                move_cursor_to(e, e->cy, e->cx > 0 ? e->cx - 1 : 0);
                break;
            case 'l':
                move_cursor_to(e, e->cy, e->cx + 1);
                break;
            case 'j':
                move_cursor_to(e, e->cy + 1, e->cx);
                break;
            case 'k':
                move_cursor_to(e, e->cy > 0 ? e->cy - 1 : 0, e->cx);
                break;
            // TODO: fix segfault!
            case '/':
                // Forward search
                e->mode = SEARCH;
                e->search_d = false;
                e->cmd_len = 0;
                e->command[0] = '\0';
                break;
            case '?':
                // Backward search
                e->mode = SEARCH;
                e->search_d = true;
                e->cmd_len = 0;
                e->command[0] = '\0';
                break;
            case 'N':
                if (e->matched_lines && e->matches > 0)
                {
                    if (e->cur_match <= 0)
                        e->cur_match = e->matches - 1;
                    else
                        e->cur_match--;

                    e->cy = e->matched_lines[e->cur_match] - 1;
                    move_cursor_to(e, e->cy, e->cx);
                }
                break;

            case 'n':
                if (e->matched_lines && e->matches > 0)
                {
                    if (e->cur_match >= e->matches - 1)
                        e->cur_match = 0;
                    else
                        e->cur_match++;

                    e->cy = e->matched_lines[e->cur_match] - 1;
                    move_cursor_to(e, e->cy, e->cx);
                }
                break;
            case ':':
                e->mode = COMMAND;
                e->cmd_len = 0;
                e->command[0] = '\0';
                break;
            case 'b':
                e->mode = BUFFER_SWITCH;
                e->cmd_len = 0;
                e->command[0] = '\0';
                break;
            }
        }
        else if (e->mode == INSERT)
        {
            if (ch == 27)
                e->mode = NORMAL;
            else if (ch == 10 || ch == KEY_ENTER)
                insert_newline(e);
            else if (ch == 127 || ch == KEY_BACKSPACE)
                backspace_char(e);
            else if (ch == 127 || ch == KEY_DC)
                e->cx++, backspace_char(e);
            else if (ch == '\t')
                insert_string(e, "    ");
            else
                insert_char(e, ch);
        }
        else if (e->mode == SEARCH)
        {
            if (ch == 10 || ch == KEY_ENTER)
            {
                handle_search(e);
                e->cy = e->matched_lines[e->cur_match] - 1;
                move_cursor_to(e, e->cy, e->cx);
                e->mode = NORMAL;
                e->cmd_len = 0;
                e->command[0] = '\0';
                erase();
            }
            else if (ch == 27)
            {
                e->mode = NORMAL;
                e->cmd_len = 0;
                e->command[0] = '\0';
            }
            else if (ch == 127 || ch == KEY_BACKSPACE)
            {
                if (e->cmd_len > 0)
                {
                    e->command[--e->cmd_len] = '\0';
                }
                else
                {
                    e->command[0] = '\0';
                    e->mode = NORMAL;
                }
            }
            else if (isprint(ch) && e->cmd_len < SEARCH_BUFFER_SIZE - 1)
            {
                e->command[e->cmd_len++] = ch;
                e->command[e->cmd_len] = '\0';
            }
        }
        else if (e->mode == COMMAND)
        {
            // Handle command mode input
            if (ch == 10 || ch == KEY_ENTER)
                handle_command(e);
            else if (ch == 27)
            {
                e->mode = NORMAL;
                e->cmd_len = 0;
                e->command[0] = '\0';
                erase();
            }
            else if (ch == 127 || ch == KEY_BACKSPACE)
            {
                if (e->cmd_len > 0)
                    e->command[--e->cmd_len] = '\0';
                else
                {
                    e->command[0] = '\0';
                    e->mode = NORMAL;
                }
                erase();
            }
            else if (e->cmd_len < sizeof(e->command) - 1)
            {
                e->command[e->cmd_len++] = ch;
                e->command[e->cmd_len] = '\0';
            }
        }
        else if (e->mode == BUFFER_SWITCH)
        {
            // Handle command mode input
            if (ch == 10 || ch == KEY_ENTER)
                handle_buffer_switch(e);
            else if (ch == 27)
            {
                e->mode = NORMAL;
                e->cmd_len = 0;
                e->command[0] = '\0';
                erase();
            }
            else if (ch == 127 || ch == KEY_BACKSPACE)
            {
                if (e->cmd_len > 0)
                    e->command[--e->cmd_len] = '\0';
                else
                {
                    e->command[0] = '\0';
                    e->mode = NORMAL;
                }
                erase();
            }
            else if (e->cmd_len < sizeof(e->command) - 1)
            {
                e->command[e->cmd_len++] = ch;
                e->command[e->cmd_len] = '\0';
            }
        }
    }
}

char* linearize(char** lines, size_t count)
{
    if (count == 0 || lines == NULL)
        return NULL;
    size_t total_size = 0;
    for (size_t i = 0; i < count; i++)
        if (lines[i] != NULL)
            total_size += strlen(lines[i]);
    total_size += count - 1;    
    total_size += 1;
    char* result = (char*)malloc(total_size);
    if (result == NULL)
        return NULL; 
    char* current = result;
    for (size_t i = 0; i < count; i++)
    {
        if (lines[i] != NULL)
        {
            size_t len = strlen(lines[i]);
            memcpy(current, lines[i], len);
            current += len;
        }
        if (i < count - 1)
            *current++ = '\n';
    }
    *current = '\0';
    return result;
}

int main(int argc, char *argv[])
{
    event_handler = event_handler_init(16);
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);

    mila_globals = mila_init();
    editor = editor_create();
    for (int i = 1; i < argc; ++i)
        editor_add_buffer(editor, create_file_buffer(argv[i]));
    if (argc == 1)
    {
        editor_add_buffer(editor, create_file_buffer("*"));
    }
    env_set_local_raw(mila_globals, "editor", vopaque(editor));
    register_editor_bindings(mila_globals);

    char* init_path = home("~/.vmmrc.mila");
    if (run_file(init_path, mila_globals) > 1) {
        editor_free(editor);
        free(init_path);
        event_handler_destroy(event_handler);
        return 1;
    }
    free(init_path);

    set_escdelay(0);

    start_color();
    use_default_colors();
    init_color(9, 90, 90, 90);
    init_pair(9, -1, -1);
    bkgdset(COLOR_PAIR(9));
    init_pair(1, COLOR_BLUE, 9);
    init_color(COLOR_RED, 1000, 400, 300);
    init_pair(2, COLOR_RED, 9);
    init_pair(3, COLOR_GREEN, 9);
    init_pair(4, COLOR_CYAN, 9);
    init_pair(5, COLOR_YELLOW, 9);
    init_pair(6, COLOR_MAGENTA, 9);
    init_pair(7, COLOR_BLACK, COLOR_RED);
    init_color(8, 100, 602, 602);
    init_pair(8, 8, 9);

    erase();
    int ch;
    while (1)
    {
        draw(editor);
        ch = getch();
        event_call(event_handler, ev_keypress, &ch);
        handle_input(editor, ch);
    }
}
