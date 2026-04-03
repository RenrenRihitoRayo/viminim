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

static char *substr(const char *src, size_t start, size_t len)
{
    char *s = malloc(len + 1);
    if (s)
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
    if (!cmd) return;
    for (int i = 0; cmd->command[i]; ++i)
        free(cmd->command[i]);
    free(cmd->command);
    free(cmd);
}

Command *parse_command(const char *input)
{
    if (!input) return NULL;
    
    Command* cmd = malloc(sizeof(Command));
    if (!cmd) return NULL;
    
    int cap = INITIAL_CAP, count = 0;
    char **tokens = malloc(cap * sizeof(char*));
    if (!tokens) {
        free(cmd);
        return NULL;
    }
    
    size_t input_len = strlen(input);
    size_t i = 0;
    
    while (i < input_len) {
        if (isspace((unsigned char)input[i])) {
            i++;
            continue;
        }
        
        if (input[i] == '"') {
            i++;
            char *buf = malloc(input_len + 1);
            if (!buf) goto cleanup;
            
            size_t bi = 0;
            while (i < input_len && input[i] != '"') {
                if (input[i] == '\\' && i + 1 < input_len) {
                    i++;
                    buf[bi++] = translate_escape(input[i++]);
                } else {
                    buf[bi++] = input[i++];
                }
            }
            if (i < input_len && input[i] == '"') i++;
            buf[bi] = '\0';
            push_token(&tokens, &count, &cap, buf);
            continue;
        }
        
        if (isdigit((unsigned char)input[i]) || 
            (input[i] == '.' && i + 1 < input_len && isdigit((unsigned char)input[i+1]))) {
            size_t start = i;
            int has_dot = 0;
            if (input[i] == '.') has_dot = 1;
            
            while (i < input_len && (isdigit((unsigned char)input[i]) || input[i] == '.')) {
                if (input[i] == '.') {
                    if (has_dot) break;
                    has_dot = 1;
                }
                i++;
            }
            push_token(&tokens, &count, &cap, substr(input, start, i - start));
            continue;
        }
        
        if (strchr("=!<>+-*/", input[i])) {
            size_t start = i;
            if ((input[i] == '=' && i + 1 < input_len && input[i+1] == '=') ||
                (input[i] == '!' && i + 1 < input_len && input[i+1] == '=') ||
                (input[i] == '>' && i + 1 < input_len && input[i+1] == '=') ||
                (input[i] == '<' && i + 1 < input_len && input[i+1] == '=') ||
                (input[i] == '*' && i + 1 < input_len && input[i+1] == '*')) {
                i += 2;
            } else {
                i++;
            }
            push_token(&tokens, &count, &cap, substr(input, start, i - start));
            continue;
        }
        
        if (strchr("(){}", input[i])) {
            push_token(&tokens, &count, &cap, substr(input, i, 1));
            i++;
            continue;
        }
        
        if (isalpha((unsigned char)input[i]) || strchr("\\/.[]", input[i])) {
            size_t start = i;
            while (i < input_len && (isalnum((unsigned char)input[i]) || strchr("\\/.[]", input[i])))
                i++;
            push_token(&tokens, &count, &cap, substr(input, start, i - start));
            continue;
        }
        
        i++;
    }
    
    tokens[count] = NULL;
    cmd->command = tokens;
    cmd->argc = count;
    return cmd;

cleanup:
    for (int j = 0; j < count; j++)
        free(tokens[j]);
    free(tokens);
    free(cmd);
    return NULL;
}