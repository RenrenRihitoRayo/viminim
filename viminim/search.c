#include "../vcommons.h"

int vmm_match_range(const char **pat, char c) {
    int negate = 0;
    if (**pat == '!') {
        negate = 1;
        (*pat)++;
    }

    int matched = 0;
    while (**pat && **pat != ']') {
        if ((*pat)[1] == '-' && (*pat)[2] && (*pat)[2] != ']') {
            // Handle range a-z
            if (c >= **pat && c <= (*pat)[2]) matched = 1;
            *pat += 3;  // skip 'a-z'
        } else {
            if (c == **pat) matched = 1;
            (*pat)++;
        }
    }

    if (**pat == ']') (*pat)++; // skip closing bracket
    return negate ? !matched : matched;
}

int vmm_match(const char *pattern, const char *str) {
    while (*pattern) {
        if (*pattern == '*') {
            pattern++;
            if (!*pattern) return 1;
            while (*str) {
                if (vmm_match(pattern, str)) return 1;
                str++;
            }
            return 0;
        } else if (*pattern == '?') {
            if (!*str) return 0;
            pattern++;
            str++;
        } else if (*pattern == '[') {
            pattern++;
            if (!*str || !vmm_match_range(&pattern, *str)) return 0;
            str++;
        } else {
            if (*pattern != *str) return 0;
            pattern++;
            str++;
        }
    }
    return !*str;
}

void handle_search(Editor *s)
{
    char **lines = s->buffers[s->current]->lines;
    char* pattern = s->command;
    if (!s || !lines || !pattern) return;

    // free previous results if any
    if (s->matched_lines) {
        free(s->matched_lines);
        s->matched_lines = NULL;
    }

    size_t capacity = 16;
    size_t count = 0;
    size_t *matches = malloc(sizeof(size_t) * capacity);
    if (!matches) return;

    size_t total = s->buffers[s->current]->line_count;

    if (s->search_d) {
        // search UP (reverse)
        for (ssize_t i = (ssize_t)total - 1; i >= 0; i--) {
            if (vmm_match(pattern, lines[i])) {
                if (!s->cur_match) s->cur_match;
                if (count + 1 >= capacity) {
                    capacity *= 2;
                    matches = realloc(matches, sizeof(size_t) * capacity);
                    if (!matches) return;
                }
                matches[count++] = i + 1; // 1-based
            }
        }
    } else {
        // search DOWN (normal)
        for (size_t i = 0; i < total; i++) {
            if (vmm_match(pattern, lines[i])) {
                if (!s->cur_match) s->cur_match;
                if (count + 1 >= capacity) {
                    capacity *= 2;
                    matches = realloc(matches, sizeof(size_t) * capacity);
                    if (!matches) return;
                }
                matches[count++] = i + 1; // 1-based
            }
        }
    }

    // null-terminate with 0
    if (count + 1 >= capacity) {
        matches = realloc(matches, sizeof(size_t) * (count + 1));
        if (!matches) return;
    }
    matches[count] = 0;
    s->matched_lines = matches;
    s->matches = count;
}
