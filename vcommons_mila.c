#include "vcommons.h"
#include <ncurses.h>

typedef enum {
    CHANGED,
} BuffStatus;

__attribute__((format(printf, 2, 3)))
Value *vtagged_error(ErrorType err, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    // First pass: find length
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (len < 0)
    {
        va_end(ap);
        return NULL;
    }

    char *buf = mila_malloc(len + 1);
    if (!buf)
    {
        va_end(ap);
        Value *v = val_new(T_ERROR);
        v->v.message = mila_strdup("verror could not allocate memory!");
        return v;
    }

    vsnprintf(buf, len + 1, fmt, ap);
    va_end(ap);
    push_message_log(editor, buf, ERROR);
    Value *v = val_new(T_TAGGED_ERROR);
    v->v.tagged_error.message = buf;
    v->v.tagged_error.type = err;
    return v;
}

__attribute__((format(printf, 1, 2)))
Value *verror(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    // First pass: find length
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (len < 0)
    {
        va_end(ap);
        return NULL;
    }

    char *buf = mila_malloc(len + 1);
    if (!buf)
    {
        va_end(ap);
        Value *v = val_new(T_ERROR);
        v->v.message = mila_strdup("verror could not allocate memory!");
        return v;
    }

    vsnprintf(buf, len + 1, fmt, ap);
    va_end(ap);
    push_message_log(editor, buf, ERROR);
    Value *v = val_new(T_ERROR);
    v->v.message = buf;
    return v;
}


Value* toggle_numbers(Env* e, int argc, Value** argv) {
    if(argc != 1) return verror("toggle_numbers(ed): Invalid number of arguments!");
    int flag = ((Editor*)(argv[0]->v.opaque))->show_line_numbers;
    ((Editor*)(argv[0]->v.opaque))->show_line_numbers = !flag;
    return vnull();
}

Value* set_numbers(Env* e, int argc, Value** argv) {
    if(argc != 2) return verror("set_numbers(ed): Invalid number of arguments!");
    ((Editor*)(argv[0]->v.opaque))->show_line_numbers = is_truthy(argv[1]) ? 1 : 0;
    return vnull();
}

Value* native_push_message(Env* e, int argc, Value** argv) {
    if(argc != 2) return verror("push_message(ed, msg): Invalid number of arguments!");
    push_message((Editor*)(argv[0]->v.opaque), GET_STRING(argv[1]));
    return vnull();
}

Value* native_push_message_log(Env* e, int argc, Value** argv) {
    if(argc != 3) return verror("push_message_log(ed, msg, level): Invalid number of arguments!");
    push_message_log((Editor*)(argv[0]->v.opaque), GET_STRING(argv[1]), (MessageSeverity)GET_INTEGER(argv[2]));
    return vnull();
}

Value* native_get_current_buffer(Env* e, int argc, Value** argv)
{
    if(argc!=1) return verror("get_current_buffer(ed): Invalid number of arguments!");
    Editor *ed = (Editor*)argv[0]->v.opaque;
    return vopaque(ed->buffers[ed->current]);
}

Value* native_insert_line(Env* e, int argc, Value** argv)
{
    if(argc!=3) return verror("insert_line(buf, line_num, line): Invalid number of arguments!");
    insert_line((Buffer*)argv[0]->v.opaque, GET_INTEGER(argv[1]), GET_STRING(argv[2]));
    return vnull();
}

Value* native_set_line(Env* e, int argc, Value** argv)
{
    if(argc!=3) return verror("set_line(buf, line_num, line): Invalid number of arguments!");
    set_line((Buffer*)argv[0]->v.opaque, GET_INTEGER(argv[1]), GET_STRING(argv[2]));
    return vnull();
}

Value* native_get_line(Env* e, int argc, Value** argv)
{
    if(argc!=2) return verror("set_line(buf, line_num): Invalid number of arguments!");
    return vstring_dup(get_line((Buffer*)argv[0]->v.opaque, GET_INTEGER(argv[1])));
}


Value* native_delete_line(Env* e, int argc, Value** argv)
{
    if(argc!=2) return verror("insert_line(buf, line_num): Invalid number of arguments!");
    delete_line((Buffer*)argv[0]->v.opaque, GET_INTEGER(argv[1]));
    return vnull();
}

Value* native_get_buffer_count(Env* e, int argc, Value** argv)
{
    if(argc!=1) return verror("get_buffer_count(ed): Invalid number of arguments!");
    return vint(((Editor*)argv[0]->v.opaque)->count);
}

Value* native_new_buffer(Env* e, int argc, Value** argv)
{
    if(argc!=2) return verror("new_buffer(ed, name): Invalid number of arguments!");
    Editor* ed = ((Editor*)argv[0]->v.opaque);
    Buffer* buffer = create_file_buffer(GET_STRING(argv[1]));
    return vint(editor_add_buffer(ed, buffer));
}

Value* native_set_buffer(Env* e, int argc, Value** argv)
{
    if(argc!=2) return verror("set_buffer(ed, id): Invalid number of arguments!");
    Editor* ed = ((Editor*)argv[0]->v.opaque);
    ed->current = GET_INTEGER(argv[1]);
    return vnull();
}

Value* native_get_buffer(Env* e, int argc, Value** argv)
{
    if(argc!=2) return verror("get_buffer(ed, id): Invalid number of arguments!");
    Editor* ed = ((Editor*)argv[0]->v.opaque);
    return vopaque(ed->buffers[GET_INTEGER(argv[1])]);
}

Value* native_get_cx(Env* e, int argc, Value** argv)
{
    if(argc!=1) return verror("get_cx(ed): Invalid number of arguments!");
    Editor* ed = ((Editor*)argv[0]->v.opaque);
    return vint(ed->cx);
}

Value* native_get_cy(Env* e, int argc, Value** argv)
{
    if(argc!=1) return verror("get_cy(ed): Invalid number of arguments!");
    Editor* ed = ((Editor*)argv[0]->v.opaque);
    return vint(ed->cy);
}

Value* native_set_cx(Env* e, int argc, Value** argv)
{
    if(argc!=2) return verror("set_cx(ed, cx): Invalid number of arguments!");
    Editor* ed = ((Editor*)argv[0]->v.opaque);
    ed->cx = GET_INTEGER(argv[1]);
    return vnull();
}

Value* native_set_cy(Env* e, int argc, Value** argv)
{
    if(argc!=2) return verror("set_cy(ed, cy): Invalid number of arguments!");
    Editor* ed = ((Editor*)argv[0]->v.opaque);
    ed->cx = GET_INTEGER(argv[1]);
    return vnull();
}

Value* native_set_buffer_status(Env* e, int argc, Value** argv)
{
    if(argc!=3) return verror("set_buffer_status(buffer, status, bool): Invalid number of arguments!");
    Buffer* buf = ((Buffer*)argv[0]->v.opaque);
    int id = GET_INTEGER(argv[1]);
    switch (id)
    {
    case CHANGED:
        buf->changed = is_truthy(argv[2]) ? 1 : 0; break;
    default:
        return vtagged_error(E_RUNTIME, "Invalid status id `%i`!", id);
    }
    return vnull();
}

Value* native_get_buffer_status(Env* e, int argc, Value** argv)
{
    if(argc!=2) return verror("set_buffer_status(buffer, status): Invalid number of arguments!");
    Buffer* buf = ((Buffer*)argv[0]->v.opaque);
    int id = GET_INTEGER(argv[1]);
    switch (id)
    {
    case CHANGED:
        return vbool(buf->changed);
    default:
        return vtagged_error(E_RUNTIME, "Invalid status id `%i`!", id);
    }
}

Value* native_update_cursor(Env* e, int argc, Value** argv)
{
    if(argc != 1) return verror("update_cursor(ed): Invalid number of arguments!");
    Editor* ed = ((Editor*)argv[0]->v.opaque);
    move_cursor_to(ed, ed->cy, ed->cx); // just update the cursor
    return vnull();
}

Value* native_update_screen(Env* e, int argc, Value** argv)
{
    if(argc != 1) return verror("update_screen(ed): Invalid number of arguments!");
    erase();
    Editor* ed = ((Editor*)argv[0]->v.opaque);
    draw(ed);
    return vnull();
}

Value* native_command(Env* e, int argc, Value** argv)
{
    if(argc != 2) return verror("command(ed, cmd): Invalid number of arguments!");
    Editor* ed = ((Editor*)argv[0]->v.opaque);
    editor_command(ed, GET_STRING(argv[1]));
    return vnull();
}

Value* native_get_whole_buffer(Env* e, int argc, Value** argv)
{
    if(argc != 1) return verror("get_whole_buffer(ed): Invalid number of arguments!");
    Buffer* b = (Buffer*)GET_OPAQUE(argv[0]);
    return vstring_take(linearize(b->lines, b->line_count));
}

Value* native_getch(Env* e, int argc, Value** argv)
{
    if(argc != 0) return verror("getch(): Invalid number of arguments!");
    char text[2] = {getch(), 0};
    return vstring_dup(text);
}

Value* native_catchch(Env* e, int argc, Value** argv)
{
    if(argc != 0) return verror("catchch(): Invalid number of arguments!");
    char text[2] = {getch(), 0};
    ungetch(text[0]);
    return vstring_dup(text);
}

Value* native_type(Env* e, int argc, Value** argv)
{
    // iterate args backwards
    for (int i = argc - 1; i >= 0; --i)
    {
        Value* arg = argv[i];

        if (MILA_GET_TYPE(arg) == T_STRING)
        {
            char* str = GET_STRING(arg);
            int len = strlen(str);
            // push characters in reverse
            for (int c = len - 1; c >= 0; --c)
            {
                ungetch(str[c]);
            }
        }
        else if (MILA_GET_TYPE(arg) == T_INT)
        {
            ungetch((char)GET_INTEGER(arg));
        }
        else if (MILA_GET_TYPE(arg) == T_UINT)
        {
            ungetch((char)GET_UINTEGER(arg));
        }
    }

    return vnull();
}


Value* native_subscribe(Env* e, int argc, Value** argv)
{
    if(argc!=2) return verror("subscribe(id, fn): Invalid number of arguments!");
    subscribe(event_handler, GET_INTEGER(argv[0]), argv[1]);
    return vnull();
}

Value* native_get_mode(Env* e, int argc, Value** argv)
{
    if(argc!=1) return verror("get_mode(ed): Invalid number of arguments!");
    Editor* ed = GET_OPAQUE(argv[0]);
    return vint(ed->mode);
}

// Every function exposed to mila by vmm
void register_editor_bindings(Env* e)
{
    // Aesthetic
    env_register_native(e, "toggle_numbers", toggle_numbers);
    env_register_native(e, "set_numbers", set_numbers);
    // Logging
    env_register_native(e, "push_message", native_push_message);
    env_register_native(e, "push_message_log", native_push_message_log);
    // Buffer manipulation
    env_register_native(e, "get_current_buffer", native_get_current_buffer);
    env_register_native(e, "insert_line", native_insert_line);
    env_register_native(e, "delete_line", native_delete_line);
    env_register_native(e, "set_line", native_set_line);
    env_register_native(e, "set_buffer", native_set_buffer);
    env_register_native(e, "new_buffer", native_new_buffer);
    // Buffer reading
    env_register_native(e, "get_line", native_get_line);
    env_register_native(e, "get_buffer", native_get_buffer);
    env_register_native(e, "get_buffer_count", native_get_buffer_count);
    env_register_native(e, "get_cy", native_get_cy);
    env_register_native(e, "get_cx", native_get_cx);
    env_register_native(e, "set_cy", native_set_cy);
    env_register_native(e, "set_cx", native_set_cx);
    env_register_native(e, "set_buffer_status", native_set_buffer_status);
    env_register_native(e, "get_buffer_status", native_get_buffer_status);
    env_register_native(e, "update_cursor", native_update_cursor);
    env_register_native(e, "update_screen", native_update_screen);
    env_register_native(e, "command", native_command);
    env_register_native(e, "type", native_type);
    env_register_native(e, "get_whole_buffer", native_get_whole_buffer);
    env_register_native(e, "getch", native_getch);
    env_register_native(e, "catchch", native_catchch);
    env_register_native(e, "subscribe", native_subscribe);
    env_register_native(e, "get_mode", native_get_mode);
    
    // Values
    env_set_local_raw(e, "IMPORTANT_ERROR", vint(IMPORTANT_ERROR));
    env_set_local_raw(e, "ERROR", vint(ERROR));
    env_set_local_raw(e, "WARNING", vint(WARNING));
    env_set_local_raw(e, "INFO", vint(INFO));
    // == Status IDs
    env_set_local_raw(e, "STATUS_CHANGED", vint(CHANGED));
    // == Events
    env_set_local_raw(e, "EVENT_KEYPRESS", vint(ev_keypress));
    env_set_local_raw(e, "EVENT_READ", vint(ev_read));
    env_set_local_raw(e, "EVENT_SAVE", vint(ev_save));
    env_set_local_raw(e, "EVENT_CREATE_BUFFER", vint(ev_buffer_create));
    env_set_local_raw(e, "EVENT_CLOSE_BUFFER", vint(ev_close_buffer));
    env_set_local_raw(e, "INSERT", vint(INSERT));
    env_set_local_raw(e, "BUFFER", vint(BUFFER_SWITCH));
    env_set_local_raw(e, "SEARCH", vint(SEARCH));
    env_set_local_raw(e, "NORMAL", vint(NORMAL));
    env_set_local_raw(e, "COMMAND", vint(COMMAND));

    int fn = 265;
    env_set_local_raw(e, "KEY_DOWN",    vint(0402));        
    env_set_local_raw(e, "KEY_UP",      vint(0403));        
    env_set_local_raw(e, "KEY_LEFT",    vint(0404));        
    env_set_local_raw(e, "KEY_RIGHT",   vint(0405));        
    env_set_local_raw(e, "KEY_HOME",      vint(0406));        
    env_set_local_raw(e, "KEY_BACKSPACE", vint(0407));        
    env_set_local_raw(e, "KEY_F1",        vint(fn++));        
    env_set_local_raw(e, "KEY_F2",        vint(fn++));        
    env_set_local_raw(e, "KEY_F3",        vint(fn++));        
    env_set_local_raw(e, "KEY_F4",        vint(fn++));        
    env_set_local_raw(e, "KEY_F5",        vint(fn++));        
    env_set_local_raw(e, "KEY_F6",        vint(fn++));        
    env_set_local_raw(e, "KEY_F7",        vint(fn++));        
    env_set_local_raw(e, "KEY_F8",        vint(fn++));        
    env_set_local_raw(e, "KEY_F9",        vint(fn++));        
    env_set_local_raw(e, "KEY_F10",        vint(fn++));        
    env_set_local_raw(e, "KEY_F11",        vint(fn++));        
    env_set_local_raw(e, "KEY_F12",        vint(fn++));        
    env_set_local_raw(e, "KEY_F13",        vint(fn++));        
    env_set_local_raw(e, "KEY_F14",        vint(fn++));        
    env_set_local_raw(e, "KEY_F15",        vint(fn++));        
    env_set_local_raw(e, "KEY_F16",        vint(fn++));        
    env_set_local_raw(e, "KEY_F17",        vint(fn++));        
    env_set_local_raw(e, "KEY_F18",        vint(fn++));        
    env_set_local_raw(e, "KEY_F19",        vint(fn++));        
    env_set_local_raw(e, "KEY_F20",        vint(fn++));        
    env_set_local_raw(e, "KEY_F21",        vint(fn++));        
    env_set_local_raw(e, "KEY_F22",        vint(fn++));        
    env_set_local_raw(e, "KEY_F23",        vint(fn++));        
    env_set_local_raw(e, "KEY_F24",        vint(fn++));        
    env_set_local_raw(e, "KEY_F25",        vint(fn++));        
    env_set_local_raw(e, "KEY_DL",        vint(0510));        
    env_set_local_raw(e, "KEY_IL",      vint(0511));        
    env_set_local_raw(e, "KEY_DC",      vint(0512));        
    env_set_local_raw(e, "KEY_IC",      vint(0513));        
    env_set_local_raw(e, "KEY_EIC",     vint(0514));        
    env_set_local_raw(e, "KEY_CLEAR",   vint(0515));        
    env_set_local_raw(e, "KEY_EOS",     vint(0516));        
    env_set_local_raw(e, "KEY_EOL",     vint(0517));        
    env_set_local_raw(e, "KEY_SF",      vint(0520));        
    env_set_local_raw(e, "KEY_SR",      vint(0521));        
    env_set_local_raw(e, "KEY_NPAGE",   vint(0522));        
    env_set_local_raw(e, "KEY_PPAGE",   vint(0523));        
    env_set_local_raw(e, "KEY_STAB",    vint(0524));        
    env_set_local_raw(e, "KEY_CTAB",    vint(0525));        
    env_set_local_raw(e, "KEY_CATAB",   vint(0526));        
    env_set_local_raw(e, "KEY_ENTER",   vint(0527));        
    env_set_local_raw(e, "KEY_PRINT",   vint(0532));        
    env_set_local_raw(e, "KEY_LL",      vint(0533));        
    env_set_local_raw(e, "KEY_A1",      vint(0534));        
    env_set_local_raw(e, "KEY_A3",      vint(0535));        
    env_set_local_raw(e, "KEY_B2",      vint(0536));        
    env_set_local_raw(e, "KEY_C1",      vint(0537));        
    env_set_local_raw(e, "KEY_C3",      vint(0540));        
    env_set_local_raw(e, "KEY_BTAB",    vint(0541));        
    env_set_local_raw(e, "KEY_BEG",     vint(0542));        
    env_set_local_raw(e, "KEY_CANCEL",  vint(0543));        
    env_set_local_raw(e, "KEY_CLOSE",   vint(0544));        
    env_set_local_raw(e, "KEY_COMMAND", vint(0545));        
    env_set_local_raw(e, "KEY_COPY",    vint(0546));        
    env_set_local_raw(e, "KEY_CREATE",  vint(0547));        
    env_set_local_raw(e, "KEY_END",     vint(0550));        
    env_set_local_raw(e, "KEY_EXIT",    vint(0551));        
    env_set_local_raw(e, "KEY_FIND",    vint(0552));        
    env_set_local_raw(e, "KEY_HELP",    vint(0553));        
    env_set_local_raw(e, "KEY_MARK",    vint(0554));        
    env_set_local_raw(e, "KEY_MESSAGE", vint(0555));        
    env_set_local_raw(e, "KEY_MOVE",    vint(0556));        
    env_set_local_raw(e, "KEY_NEXT",    vint(0557));        
    env_set_local_raw(e, "KEY_OPEN",    vint(0560));        
    env_set_local_raw(e, "KEY_OPTIONS", vint(0561));        
    env_set_local_raw(e, "KEY_PREVIOUS",    vint(0562));        
    env_set_local_raw(e, "KEY_REDO",    vint(0563));        
    env_set_local_raw(e, "KEY_REFERENCE",   vint(0564));        
    env_set_local_raw(e, "KEY_REFRESH", vint(0565));        
    env_set_local_raw(e, "KEY_REPLACE", vint(0566));        
    env_set_local_raw(e, "KEY_RESTART", vint(0567));        
    env_set_local_raw(e, "KEY_RESUME",  vint(0570));        
    env_set_local_raw(e, "KEY_SAVE",    vint(0571));        
    env_set_local_raw(e, "KEY_SBEG",    vint(0572));        
    env_set_local_raw(e, "KEY_SCANCEL", vint(0573));        
    env_set_local_raw(e, "KEY_SCOMMAND",    vint(0574));        
    env_set_local_raw(e, "KEY_SCOPY",   vint(0575));        
    env_set_local_raw(e, "KEY_SCREATE", vint(0576));        
    env_set_local_raw(e, "KEY_SDC",     vint(0577));        
    env_set_local_raw(e, "KEY_SDL",     vint(0600));        
    env_set_local_raw(e, "KEY_SELECT",  vint(0601));        
    env_set_local_raw(e, "KEY_SEND",    vint(0602));        
    env_set_local_raw(e, "KEY_SEOL",    vint(0603));        
    env_set_local_raw(e, "KEY_SEXIT",   vint(0604));        
    env_set_local_raw(e, "KEY_SFIND",   vint(0605));        
    env_set_local_raw(e, "KEY_SHELP",   vint(0606));        
    env_set_local_raw(e, "KEY_SHOME",   vint(0607));        
    env_set_local_raw(e, "KEY_SIC",     vint(0610));        
    env_set_local_raw(e, "KEY_SLEFT",   vint(0611));        
    env_set_local_raw(e, "KEY_SMESSAGE",    vint(0612));        
    env_set_local_raw(e, "KEY_SMOVE",   vint(0613));        
    env_set_local_raw(e, "KEY_SNEXT",   vint(0614));        
    env_set_local_raw(e, "KEY_SOPTIONS",    vint(0615));        
    env_set_local_raw(e, "KEY_SPREVIOUS",   vint(0616));        
    env_set_local_raw(e, "KEY_SPRINT",  vint(0617));        
    env_set_local_raw(e, "KEY_SREDO",   vint(0620));        
    env_set_local_raw(e, "KEY_SREPLACE",    vint(0621));        
    env_set_local_raw(e, "KEY_SRIGHT",  vint(0622));        
    env_set_local_raw(e, "KEY_SRSUME",  vint(0623));        
    env_set_local_raw(e, "KEY_SSAVE",   vint(0624));        
    env_set_local_raw(e, "KEY_SSUSPEND",    vint(0625));        
    env_set_local_raw(e, "KEY_SUNDO",   vint(0626));        
    env_set_local_raw(e, "KEY_SUSPEND", vint(0627));        
    env_set_local_raw(e, "KEY_UNDO",    vint(0630));        
    env_set_local_raw(e, "KEY_MOUSE",   vint(0631));        
}
