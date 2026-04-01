#include "../vcommons.h"

#define MAX_SPANS 64

static const char *c_keywords[] = {
    "if","else","while","for","do","switch","case","break","continue",
    "return","goto","sizeof","typedef","struct","union","enum","static",
    "extern","const","volatile","register","inline","void","int","char",
    "short","long","float","double","unsigned","signed","auto","default",
    "NULL","true","false","bool", NULL
};

static bool is_c_keyword(const char *line, int start, int wlen)
{
    for (int i = 0; c_keywords[i]; i++) {
        size_t klen = strlen(c_keywords[i]);
        if ((size_t)wlen == klen && strncmp(line + start, c_keywords[i], klen) == 0)
            return true;
    }
    return false;
}

static HLState highlight_c_line(const char *line, HLState entry, ColorSpan **out)
{
    int len = (int)strlen(line);
    ColorSpan *spans = malloc(sizeof(ColorSpan) * (MAX_SPANS + 1));
    int ns = 0;
    HLState state = entry;

    for (int i = 0; i < len && ns < MAX_SPANS; ) {
        if (state == HL_STATE_BLOCK_COMMENT) {
            int start = i;
            while (i < len && !(line[i] == '*' && i+1 < len && line[i+1] == '/'))
                i++;
            if (i+1 < len) { i += 2; state = HL_STATE_NORMAL; }
            else i = len;
            spans[ns++] = (ColorSpan){ (size_t)start, (size_t)(i - start), 3 };
            continue;
        }
        while (isspace(line[++i]));
        i--;
        if (line[i] == '#') {
            spans[ns++] = (ColorSpan){ i, (size_t)len, 6 };
            break;
        }
        if (line[i] == '/' && i+1 < len && line[i+1] == '/') {
            spans[ns++] = (ColorSpan){ (size_t)i, (size_t)(len - i), 3 };
            break;
        }
        if (line[i] == '/' && i+1 < len && line[i+1] == '*') {
            int start = i; i += 2;
            state = HL_STATE_BLOCK_COMMENT;
            while (i < len && !(line[i] == '*' && i+1 < len && line[i+1] == '/'))
                i++;
            if (i+1 < len) { i += 2; state = HL_STATE_NORMAL; }
            else i = len;
            spans[ns++] = (ColorSpan){ (size_t)start, (size_t)(i - start), 3 };
            continue;
        }
        if (line[i] == '"') {
            int start = i++;
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == '"')  { i++;    break; }
                i++;
            }
            spans[ns++] = (ColorSpan){ (size_t)start, (size_t)(i - start), 2 };
            continue;
        }
        if (line[i] == '\'') {
            int start = i++;
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == '\'') { i++;    break; }
                i++;
            }
            spans[ns++] = (ColorSpan){ (size_t)start, (size_t)(i - start), 2 };
            continue;
        }
        if (isdigit((unsigned char)line[i]) ||
            (line[i] == '.' && i+1 < len && isdigit((unsigned char)line[i+1])))
        {
            int start = i;
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '.')) i++;
            spans[ns++] = (ColorSpan){ (size_t)start, (size_t)(i - start), 5 };
            continue;
        }
        if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            int start = i;
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            int wlen = i - start;
            if (is_c_keyword(line, start, wlen))
                spans[ns++] = (ColorSpan){ (size_t)start, (size_t)wlen, 1 };
            continue;
        }
        i++;
    }

    if (ns == 0) { free(spans); *out = NULL; }
    else {
        spans[ns] = (ColorSpan){ 0, 0, 0 };
        *out = realloc(spans, sizeof(ColorSpan) * (ns + 1));
    }
    return state;
}