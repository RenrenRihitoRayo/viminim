#include "../vcommons.h"
#define MAX_SPANS 64

static const char *c_keywords[] = {
    "if","else","while","for","do","switch","case","break","continue",
    "return","goto","sizeof","typedef","struct","union","enum","static",
    "extern","const","volatile","register","inline","unsigned","signed","auto","default",
    "NULL","true","false", NULL
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

static const char *c_func[] = {
    "printf","scanf","sprintf","snprintf","vsnprintf","fprintf","fgets","fopen","fclose","ftell","fseek",
    "strcmp","strncmp","strlen","strcpy","strdup",
    "memcpy","memset",
    "calloc","malloc","free","alloca","realloc",
    "exec","fork", NULL
};

static bool is_c_func(const char *line, int start, int wlen)
{
    for (int i = 0; c_func[i]; i++) {
        size_t klen = strlen(c_func[i]);
        if ((size_t)wlen == klen && strncmp(line + start, c_func[i], klen) == 0)
            return true;
    }
    return false;
}

static const char *c_types[] = {
    "void","int","char",
    "short","long","float","double",
    "bool", "uint32_t", "uint64_t", "uint16_t",
    "int32_t", "int64_t", "int_16_t",
    "short",
    "FILE", "size_t", "ssize_t", NULL
};

static bool is_c_types(const char *line, int start, int wlen)
{
    for (int i = 0; c_types[i]; i++) {
        size_t klen = strlen(c_types[i]);
        if ((size_t)wlen == klen && strncmp(line + start, c_types[i], klen) == 0)
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
    
    // Handle entry state (continuing from previous line)
    if (state == HL_STATE_BLOCK_COMMENT) {
        int start = 0;
        int i = 0;
        while (i < len && !(line[i] == '*' && i+1 < len && line[i+1] == '/'))
            i++;
        if (i+1 < len) {
            i += 2;
            state = HL_STATE_NORMAL;
        } else {
            i = len;
        }
        spans[ns++] = (ColorSpan){ 0, (size_t)i, F_GREEN };
        // Continue processing rest of line if comment ended
        if (state == HL_STATE_NORMAL) {
            // Fall through to continue processing
        } else {
            // Still in block comment, return
            if (ns == 0) { free(spans); *out = NULL; }
            else {
                spans[ns] = (ColorSpan){ 0, 0, 0 };
                *out = realloc(spans, sizeof(ColorSpan) * (ns + 1));
            }
            return state;
        }
    }
    
    if (state == HL_STATE_STRING) {
        int start = 0;
        int i = 0;
        while (i < len) {
            if (line[i] == '\\' && i+1 < len) { i += 2; continue; }
            if (line[i] == '"') { i++; state = HL_STATE_NORMAL; break; }
            i++;
        }
        spans[ns++] = (ColorSpan){ 0, (size_t)i, F_RED };
        // If string didn't close, return in STRING state
        if (state == HL_STATE_STRING) {
            if (ns == 0) { free(spans); *out = NULL; }
            else {
                spans[ns] = (ColorSpan){ 0, 0, 0 };
                *out = realloc(spans, sizeof(ColorSpan) * (ns + 1));
            }
            return state;
        }
        // Continue processing rest of line
    }
    
    if (state == HL_STATE_CHAR) {
        int start = 0;
        int i = 0;
        while (i < len) {
            if (line[i] == '\\' && i+1 < len) { i += 2; continue; }
            if (line[i] == '\'') { i++; state = HL_STATE_NORMAL; break; }
            i++;
        }
        spans[ns++] = (ColorSpan){ 0, (size_t)i, F_RED };
        // If char didn't close, return in CHAR state
        if (state == HL_STATE_CHAR) {
            if (ns == 0) { free(spans); *out = NULL; }
            else {
                spans[ns] = (ColorSpan){ 0, 0, 0 };
                *out = realloc(spans, sizeof(ColorSpan) * (ns + 1));
            }
            return state;
        }
    }
    
    // Main loop for normal processing
    state = HL_STATE_NORMAL;
    for (int i = 0; i < len && ns < MAX_SPANS; ) {
        // Skip whitespace
        if (isspace((unsigned char)line[i])) {
            i++;
            continue;
        }
        
        // Preprocessor directive
        if (line[i] == '#') {
            spans[ns++] = (ColorSpan){ (size_t)i, (size_t)(len - i), F_MAGENTA };
            break;
        }
        
        // Single-line comment
        if (line[i] == '/' && i+1 < len && line[i+1] == '/') {
            spans[ns++] = (ColorSpan){ (size_t)i, (size_t)(len - i), F_GREEN };
            break;
        }
        
        // Block comment start
        if (line[i] == '/' && i+1 < len && line[i+1] == '*') {
            int start = i;
            i += 2;
            while (i < len && !(line[i] == '*' && i+1 < len && line[i+1] == '/'))
                i++;
            if (i+1 < len) {
                i += 2;
                state = HL_STATE_NORMAL;
            } else {
                i = len;
                state = HL_STATE_BLOCK_COMMENT;
            }
            spans[ns++] = (ColorSpan){ (size_t)start, (size_t)(i - start), F_GREEN };
            continue;
        }
        
        // String literal
        if (line[i] == '"') {
            int start = i;
            i++;
            while (i < len) {
                if (line[i] == '\\' && i+1 < len) { i += 2; continue; }
                if (line[i] == '"') { i++; break; }
                i++;
            }
            spans[ns++] = (ColorSpan){ (size_t)start, (size_t)(i - start), F_RED };
            // If string didn't close, transition to STRING state for next line
            if (i >= len && (i == len || line[i-1] != '"')) {
                state = HL_STATE_STRING;
            }
            continue;
        }
        
        // Character literal
        if (line[i] == '\'') {
            int start = i;
            i++;
            while (i < len) {
                if (line[i] == '\\' && i+1 < len) { i += 2; continue; }
                if (line[i] == '\'') { i++; break; }
                i++;
            }
            spans[ns++] = (ColorSpan){ (size_t)start, (size_t)(i - start), F_RED };
            // If char literal didn't close, transition to CHAR state for next line
            if (i >= len && (i == len || line[i-1] != '\'')) {
                state = HL_STATE_CHAR;
            }
            continue;
        }
        
        // Number literal
        if (isdigit((unsigned char)line[i]) ||
            (line[i] == '.' && i+1 < len && isdigit((unsigned char)line[i+1])))
        {
            int start = i;
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '.' || line[i] == 'e' || line[i] == 'E')) {
                i++;
            }
            spans[ns++] = (ColorSpan){ (size_t)start, (size_t)(i - start), F_YELLOW };
            continue;
        }
        
        // Identifier or keyword
        if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            int start = i;
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '_')) {
                i++;
            }
            int wlen = i - start;
            if (is_c_keyword(line, start, wlen))
                spans[ns++] = (ColorSpan){ (size_t)start, (size_t)wlen, F_BLUE };
            if (is_c_func(line, start, wlen))
                spans[ns++] = (ColorSpan){ (size_t)start, (size_t)wlen, F_CYAN };
            if (is_c_types(line, start, wlen))
                spans[ns++] = (ColorSpan){ (size_t)start, (size_t)wlen, F_TEAL };
            continue;
        }
        
        // Skip other characters (operators, punctuation)
        i++;
    }
    
    if (ns == 0) { free(spans); *out = NULL; }
    else {
        spans[ns] = (ColorSpan){ 0, 0, 0 };
        *out = realloc(spans, sizeof(ColorSpan) * (ns + 1));
    }
    return state;
}