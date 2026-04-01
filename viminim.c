#include <complex.h>
#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include "vcommons.h"
#include "parsers/c.c"
#include "parsers/mila.c"
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
        e->scroll_x = e->cx - (max_cols - lnw) + 1;
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
    return b;
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
    return e;
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

void editor_add_buffer(Editor *e, Buffer *b)
{
    if (e->count >= e->capacity)
    {
        e->capacity *= 2;
        e->buffers = realloc(e->buffers, sizeof(Buffer *) * e->capacity);
    }
    e->buffers[e->count++] = b;
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

        erase();
        draw(e);

        e->cy--;
        e->cx = prev_len;
        b->changed = true;
    }
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
        // Show command mode in status bar
        move(LINES - 2, 0);
        clrtoeol();
        int chars = snprintf(NULL, 0, "[%s MODE] Buffer: %s (%zu lines)",
                             e->mode == INSERT ? "INSERT" : "NORMAL",
                             b->name, b->line_count);
        int offset = snprintf(NULL, 0, "%s %zu, %zu    ",
                              b->syntax ? b->syntax : "???", e->cy + 1, e->cx);
        mvprintw(LINES - 2, 0, "[%s MODE] Buffer: %s (%zu lines)%*s %zu, %zi",
                 e->mode == INSERT ? "INSERT" : "NORMAL",
                 b->name, b->line_count,
                 COLS - (chars + offset), b->syntax ? b->syntax : "???",
                 e->cy + 1, e->cx);
        mvprintw(LINES - 1, 0, ":%s", e->command);
        refresh();
        return;
    }
    else if (e->mode == SEARCH)
    {
        // Show search mode in status bar
        move(LINES - 2, 0);
        clrtoeol();
        int chars = snprintf(NULL, 0, "[%s MODE] Buffer: %s (%zu lines)",
                             "SEARCH",
                             b->name, b->line_count);
        int offset = snprintf(NULL, 0, "%s %zu, %zu    ",
                              b->syntax ? b->syntax : "???", e->cy + 1, e->cx);
        mvprintw(LINES - 2, 0, "[%s MODE] Buffer: %s (%zu lines)%*s %zu, %zi",
                 "SEARCH",
                 b->name, b->line_count,
                 COLS - (chars + offset), b->syntax ? b->syntax : "???",
                 e->cy + 1, e->cx);

        // Show search direction and pattern
        char search_prompt[256];
        snprintf(search_prompt, sizeof(search_prompt), "%s%s",
                 e->search.direction_forward ? "/" : "?", e->command);
        mvprintw(LINES - 1, 0, "%s", search_prompt);
        refresh();
        return;
    }
    else if (e->mode == BUFFER_SWITCH)
    {
        // Show search mode in status bar
        move(LINES - 2, 0);
        clrtoeol();
        int chars = snprintf(NULL, 0, "[%s MODE] Buffer: %s (%zu lines)",
                             "BUFFER",
                             b->name, b->line_count);
        int offset = snprintf(NULL, 0, "%s %zu, %zu    ",
                              b->syntax ? b->syntax : "???", e->cy + 1, e->cx);
        mvprintw(LINES - 2, 0, "[%s MODE] Buffer: %s (%zu lines)%*s %zu, %zi",
                 "BUFFER",
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
        int chars = snprintf(NULL, 0, "[%s MODE] Buffer: %s (%zu lines)",
                             e->mode == INSERT ? "INSERT" : "NORMAL",
                             b->name, b->line_count);
        int offset = snprintf(NULL, 0, "%s %zu, %zu    ",
                              b->syntax ? b->syntax : "???", e->cy + 1, e->cx);
        mvprintw(LINES - 1, 0, "[%s MODE] Buffer: %s (%zu lines)%*s %zu, %zi",
                 e->mode == INSERT ? "INSERT" : "NORMAL",
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
    if ((!b->name) || (!strlen(b->name)))
    {
        return;
    }
    FILE *f = fopen(b->name, "w");
    if (!f)
        return;
    for (size_t i = 0; i < b->line_count; i++)
        fprintf(f, "%s\n", b->lines[i]);
    fclose(f);
    b->changed = false;
}

static size_t *find_in_line(const char *line, const char *pattern, size_t *out_count)
{
    if (!line || !pattern || *pattern == '\0')
    {
        *out_count = 0;
        return NULL;
    }

    size_t pattern_len = strlen(pattern);
    size_t line_len = strlen(line);
    size_t capacity = 10;
    size_t count = 0;
    size_t *positions = malloc(capacity * sizeof(size_t));

    if (!positions)
        return NULL;

    for (size_t i = 0; i <= line_len - pattern_len; i++)
    {
        if (strncmp(&line[i], pattern, pattern_len) == 0)
        {
            if (count >= capacity)
            {
                capacity *= 2;
                size_t *temp = realloc(positions, capacity * sizeof(size_t));
                if (!temp)
                {
                    free(positions);
                    return NULL;
                }
                positions = temp;
            }
            positions[count++] = i;
        }
    }

    *out_count = count;
    if (count == 0)
    {
        free(positions);
        return NULL;
    }

    return positions;
}

static size_t *find_in_line_icase(const char *line, const char *pattern, size_t *out_count)
{
    if (!line || !pattern || *pattern == '\0')
    {
        *out_count = 0;
        return NULL;
    }

    size_t pattern_len = strlen(pattern);
    size_t line_len = strlen(line);
    size_t capacity = 10;
    size_t count = 0;
    size_t *positions = malloc(capacity * sizeof(size_t));

    if (!positions)
        return NULL;

    char *lower_line = malloc(line_len + 1);
    char *lower_pattern = malloc(pattern_len + 1);

    if (!lower_line || !lower_pattern)
    {
        free(positions);
        free(lower_line);
        free(lower_pattern);
        return NULL;
    }

    for (size_t i = 0; line[i]; i++)
    {
        lower_line[i] = tolower((unsigned char)line[i]);
    }
    lower_line[line_len] = '\0';

    for (size_t i = 0; pattern[i]; i++)
    {
        lower_pattern[i] = tolower((unsigned char)pattern[i]);
    }
    lower_pattern[pattern_len] = '\0';

    for (size_t i = 0; i <= line_len - pattern_len; i++)
    {
        if (strncmp(&lower_line[i], lower_pattern, pattern_len) == 0)
        {
            if (count >= capacity)
            {
                capacity *= 2;
                size_t *temp = realloc(positions, capacity * sizeof(size_t));
                if (!temp)
                {
                    free(positions);
                    free(lower_line);
                    free(lower_pattern);
                    return NULL;
                }
                positions = temp;
            }
            positions[count++] = i;
        }
    }

    free(lower_line);
    free(lower_pattern);

    *out_count = count;
    if (count == 0)
    {
        free(positions);
        return NULL;
    }

    return positions;
}

SearchResult *search_all_buffers(Editor *e, size_t *out_count)
{
    if (!e || !e->buffers || !e->search.pattern[0])
    {
        *out_count = 0;
        return NULL;
    }

    SearchResultList results = {0};
    results.capacity = 100;
    results.results = malloc(results.capacity * sizeof(SearchResult));

    if (!results.results)
    {
        *out_count = 0;
        return NULL;
    }

    // Search through all buffers
    for (size_t buf_idx = 0; buf_idx < e->count; buf_idx++)
    {
        Buffer *b = e->buffers[buf_idx];
        if (!b || !b->lines)
            continue;

        // Search through all lines in buffer
        for (size_t line_num = 0; line_num < b->line_count; line_num++)
        {
            size_t match_count = 0;
            size_t *matches = find_in_line(b->lines[line_num], e->search.pattern, &match_count);

            if (matches)
            {
                if (results.count >= results.capacity)
                {
                    results.capacity *= 2;
                    SearchResult *temp = realloc(results.results, results.capacity * sizeof(SearchResult));
                    if (!temp)
                    {
                        free(matches);
                        goto cleanup;
                    }
                    results.results = temp;
                }

                results.results[results.count].line_num = line_num;
                results.results[results.count].col_positions = matches;
                results.results[results.count].match_count = match_count;
                results.count++;
            }
        }
    }

    *out_count = results.count;
    return results.results;

cleanup:
    for (size_t i = 0; i < results.count; i++)
    {
        free(results.results[i].col_positions);
    }
    free(results.results);
    *out_count = 0;
    return NULL;
}

SearchResult *search_buffer(Editor *e, Buffer *b, size_t *out_count)
{
    if (!e || !b || !b->lines || !e->search.pattern[0])
    {
        *out_count = 0;
        return NULL;
    }

    SearchResultList results = {0};
    results.capacity = 50;
    results.results = malloc(results.capacity * sizeof(SearchResult));

    if (!results.results)
    {
        *out_count = 0;
        return NULL;
    }

    for (size_t line_num = 0; line_num < b->line_count; line_num++)
    {
        size_t match_count = 0;
        size_t *matches = find_in_line(b->lines[line_num], e->search.pattern, &match_count);

        if (matches)
        {
            if (results.count >= results.capacity)
            {
                results.capacity *= 2;
                SearchResult *temp = realloc(results.results, results.capacity * sizeof(SearchResult));
                if (!temp)
                {
                    free(matches);
                    goto cleanup;
                }
                results.results = temp;
            }

            results.results[results.count].line_num = line_num;
            results.results[results.count].col_positions = matches;
            results.results[results.count].match_count = match_count;
            results.count++;
        }
    }

    *out_count = results.count;
    return results.results;

cleanup:
    for (size_t i = 0; i < results.count; i++)
    {
        free(results.results[i].col_positions);
    }
    free(results.results);
    *out_count = 0;
    return NULL;
}

bool find_next(Editor *e, bool forward)
{
    if (!e || !e->buffers || !e->search.pattern[0])
    {
        e->search.found = false;
        return false;
    }

    Buffer *b = e->buffers[e->current];
    if (!b || !b->lines)
    {
        e->search.found = false;
        return false;
    }

    size_t start_line = 0;
    size_t start_col = b->line_count;

    if (forward)
    {
        for (size_t line = start_line; line < b->line_count; line++)
        {
            size_t match_count = 0;
            size_t *matches = find_in_line(b->lines[line], e->search.pattern, &match_count);

            if (matches)
            {
                size_t col = 0;

                if (line == start_line)
                {
                    for (size_t i = 0; i < match_count; i++)
                    {
                        if (matches[i] > start_col)
                        {
                            col = matches[i];
                            break;
                        }
                    }
                    if (col == 0 && line + 1 < b->line_count)
                    {
                        free(matches);
                        continue;
                    }
                }
                else
                {
                    col = matches[0];
                }

                e->search.last_found_line = line;
                e->search.last_found_col = col;
                e->search.found = true;
                move_cursor_to(e, line, col);
                free(matches);
                return true;
            }

            free(matches);
        }
    }
    else
    {
        for (size_t line = start_line + 1; line > 0; line--)
        {
            size_t line_idx = line - 1;
            size_t match_count = 0;
            size_t *matches = find_in_line(b->lines[line_idx], e->search.pattern, &match_count);

            if (matches)
            {
                size_t col = 0;

                if (line_idx == start_line)
                {
                    for (int i = (int)match_count - 1; i >= 0; i--)
                    {
                        if (matches[i] < start_col)
                        {
                            col = matches[i];
                            break;
                        }
                    }
                    if (col == 0 && line_idx > 0)
                    {
                        free(matches);
                        continue;
                    }
                }
                else
                {
                    col = matches[match_count - 1];
                }

                e->search.last_found_line = line_idx;
                e->search.last_found_col = col;
                e->search.found = true;
                move_cursor_to(e, line_idx, col);
                free(matches);
                return true;
            }

            free(matches);
        }
    }

    e->search.found = false;
    return false;
}

void free_search_results(SearchResult *results, size_t count)
{
    if (!results)
        return;
    for (size_t i = 0; i < count; i++)
    {
        free(results[i].col_positions);
    }
    free(results);
}

void handle_command(Editor *e)
{
    Buffer *b = e->buffers[e->current];
    char cmd[256];
    strcpy(cmd, e->command);
    if (strlen(cmd) == 0)
    {
        e->mode = NORMAL;
        erase();
        return;
    }
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
        if (force || !b->changed)
        {
            endwin();
            exit(0);
        }
        else
            push_message_log(e, "Unsaved changes!", ERROR);
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
    else if (strcmp(command[0], "l") == 0 && argc == 2)
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
    else if (strcmp(command[0], "search") == 0 && argc == 2)
    {
        strcpy(e->search.pattern, command[1]);
    }
    else if (strcmp(command[0], "new") == 0 && argc == 2)
    {
        Buffer *b = create_file_buffer("");
        editor_add_buffer(e, b);
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

    e->cmd_len = 0;
    e->command[0] = '\0';
    e->mode = NORMAL;
    free_command(command_struct);
    erase();
}

void handle_buffer_switch(Editor *e)
{
    Buffer *b = e->buffers[e->current];
    char buffer[256];
    strcpy(buffer, e->command);

    for (size_t buf = 0; buf < e->count; ++buf)
    {
        if (strcmp(buffer, e->buffers[buf]->name) == 0)
        {
            e->current = buf;
        }
    }

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
            // case '/':
            //     // Forward search
            //     e->mode = SEARCH;
            //     e->search.direction_forward = true;
            //     e->search.last_found_line = e->cy;
            //     e->search.last_found_col = e->cx;
            //     e->cmd_len = 0;
            //     e->command[0] = '\0';
            //     break;
            // case '?':
            //     // Backward search
            //     e->mode = SEARCH;
            //     e->search.direction_forward = false;
            //     e->search.last_found_line = e->cy;
            //     e->search.last_found_col = e->cx;
            //     e->cmd_len = 0;
            //     e->command[0] = '\0';
            //     break;
            // case 'N':
            //     // Find previous match (opposite direction)
            //     if (e->search.pattern[0] != '\0') {
            //         find_next(e, !e->search.direction_forward);
            //     }
            //     break;
            // case 'n':
            //     // Find next match
            //     if (e->search.pattern[0] != '\0') {
            //         find_next(e, e->search.direction_forward);
            //     }
            //     break;
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
            // Handle search mode input
            if (ch == 10 || ch == KEY_ENTER)
            {
                // Execute search on Enter
                if (e->command[0] != '\0')
                {
                    strncpy(e->search.pattern, e->command, SEARCH_BUFFER_SIZE - 1);
                    e->search.pattern[SEARCH_BUFFER_SIZE - 1] = '\0';

                    // Find first match from current position
                    e->search.last_found_line = e->cy;
                    e->search.last_found_col = e->cx;
                    find_next(e, e->search.direction_forward);
                }
                e->mode = NORMAL;
                e->cmd_len = 0;
                e->command[0] = '\0';
            }
            else if (ch == 27)
            {
                // Cancel search on Escape
                e->mode = NORMAL;
                e->cmd_len = 0;
                e->command[0] = '\0';
            }
            else if (ch == 127 || ch == KEY_BACKSPACE)
            {
                // Backspace in search
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
                // Add character to search pattern
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

int main(int argc, char *argv[])
{
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);

    set_escdelay(0);

    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLUE, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(7, COLOR_BLACK, COLOR_RED);
    init_color(8, 100, 602, 602);
    init_pair(8, 8, -1);

    Editor *ed = editor_create();
    const char *filename = argc > 1 ? argv[1] : "meep.txt";
    editor_add_buffer(ed, create_file_buffer(filename));

    int ch;
    while (1)
    {
        draw(ed);
        ch = getch();
        handle_input(ed, ch);
    }

    endwin();
    return 0;
}
