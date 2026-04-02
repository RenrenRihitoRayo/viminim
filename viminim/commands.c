#include "../vcommons.h"

#define INITIAL_CAP 16

static void push_token(char ***tokens, int *count, int *cap, char *tok)
{
    if (*count >= *cap) {
        *cap *= 2;
        *tokens = realloc(*tokens, (*cap) * sizeof(char *));
    }
    (*tokens)[(*count)++] = tok;
}

static char *substr(const char *src, int start, int len)
{
    char *s = malloc(len + 1);
    memcpy(s, src + start, len);
    s[len] = '\0';
    return s;
}

static char translate_escape(char c)
{
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '\\': return '\\';
        case '"': return '"';
        case '\'': return '\'';
        case '0': return '\0';
        default: return c;
    }
}

void free_command(Command* cmd)
{
    for (int i=0; cmd->command[i]; ++i)
        free(cmd->command[i]);
    free(cmd->command);
    free(cmd);
}

Command *parse_command(const char *input)
{
    Command* cmd = (Command*)malloc(sizeof(Command));
    int cap = INITIAL_CAP, count = 0;
    char **tokens = malloc(cap * sizeof(char*));

    int i = 0;
    while (input[i]) {
        // skip whitespace
        if (isspace(input[i])) {
            i++;
            continue;
        }

        // --- STRING ---
        if (input[i] == '"') {
            i++; // skip opening quote
            char *buf = malloc(strlen(input) + 1);
            int bi = 0;

            while (input[i] && input[i] != '"') {
                if (input[i] == '\\') {
                    i++;
                    if (input[i])
                        buf[bi++] = translate_escape(input[i++]);
                } else {
                    buf[bi++] = input[i++];
                }
            }

            if (input[i] == '"') i++; // closing quote
            buf[bi] = '\0';

            push_token(&tokens, &count, &cap, buf);
            continue;
        }

        // --- NUMBER (int or float) ---
        if (isdigit(input[i]) || (input[i] == '.' && isdigit(input[i+1]))) {
            int start = i;
            int has_dot = 0;

            if (input[i] == '.') has_dot = 1;

            while (isdigit(input[i]) || input[i] == '.') {
                if (input[i] == '.') {
                    if (has_dot) break;
                    has_dot = 1;
                }
                i++;
            }

            push_token(&tokens, &count, &cap,
                       substr(input, start, i - start));
            continue;
        }

        // --- OPERATORS (multi-char first) ---
        if (strchr("=!<>+-*/", input[i])) {
            int start = i;

            // check for 2-char operators
            if ((input[i] == '=' && input[i+1] == '=') ||
                (input[i] == '!' && input[i+1] == '=') ||
                (input[i] == '>' && input[i+1] == '=') ||
                (input[i] == '<' && input[i+1] == '=') ||
                (input[i] == '*' && input[i+1] == '*')) {
                i += 2;
            } else {
                i++;
            }

            push_token(&tokens, &count, &cap,
                       substr(input, start, i - start));
            continue;
        }

        // --- PUNCTUATION ---
        if (strchr("(){}", input[i])) {
            push_token(&tokens, &count, &cap,
                       substr(input, i, 1));
            i++;
            continue;
        }

        // --- IDENTIFIER / WORD ---
        if (isalpha(input[i]) || strchr("\\/.[]", input[i])) {
            int start = i;
            while (isalnum(input[i]) || strchr("\\/.[]", input[i]))
                i++;

            push_token(&tokens, &count, &cap,
                       substr(input, start, i - start));
            continue;
        }

        // unknown char fallback
        i++;
    }

    tokens[count] = NULL;
    cmd->command = tokens;
    cmd->argc = count;
    return cmd;
}
