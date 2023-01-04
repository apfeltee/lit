
#include <string.h>
#include "lit.h"

static bool lit_emitter_emitexpression(LitEmitter* emitter, LitExpression* expression);
static void lit_emitter_resolvestmt(LitEmitter* emitter, LitExpression* statement);

static inline void lit_uintlist_init(LitUintList* array)
{
    lit_datalist_init(&array->list, sizeof(size_t));
}

static inline void lit_uintlist_destroy(LitState* state, LitUintList* array)
{
    lit_datalist_destroy(state, &array->list);
}

static inline void lit_uintlist_push(LitState* state, LitUintList* array, size_t value)
{
    lit_datalist_push(state, &array->list, value);
}

static inline size_t lit_uintlist_get(LitUintList* array, size_t idx)
{
    return (size_t)lit_datalist_get(&array->list, idx);
}

static inline size_t lit_uintlist_count(LitUintList* array)
{
    return lit_datalist_count(&array->list);
}

static void resolve_statements(LitEmitter* emitter, LitExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        lit_emitter_resolvestmt(emitter, statements->values[i]);
    }
}

void lit_privlist_init(LitPrivList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_privlist_destroy(LitState* state, LitPrivList* array)
{
    LIT_FREE_ARRAY(state, sizeof(LitPrivate), array->values, array->capacity);
    lit_privlist_init(array);
}

void lit_privlist_push(LitState* state, LitPrivList* array, LitPrivate value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitPrivate), old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}
void lit_loclist_init(LitLocList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_loclist_destroy(LitState* state, LitLocList* array)
{
    LIT_FREE_ARRAY(state, sizeof(LitLocal), array->values, array->capacity);
    lit_loclist_init(array);
}

void lit_loclist_push(LitState* state, LitLocList* array, LitLocal value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitLocal), old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}


void lit_emitter_init(LitState* state, LitEmitter* emitter)
{
    emitter->state = state;
    emitter->loop_start = 0;
    emitter->emit_reference = 0;
    emitter->class_name = NULL;
    emitter->compiler = NULL;
    emitter->chunk = NULL;
    emitter->module = NULL;
    emitter->previous_was_expression_statement = false;
    emitter->class_has_super = false;

    lit_privlist_init(&emitter->privates);
    lit_uintlist_init(&emitter->breaks);
    lit_uintlist_init(&emitter->continues);
}

void lit_emitter_destroy(LitEmitter* emitter)
{
    lit_uintlist_destroy(emitter->state, &emitter->breaks);
    lit_uintlist_destroy(emitter->state, &emitter->continues);
}

static void lit_emitter_emit1byte(LitEmitter* emitter, uint16_t line, uint8_t byte)
{
    if(line < emitter->last_line)
    {
        // Egor-fail proofing
        line = emitter->last_line;
    }

    lit_write_chunk(emitter->state, emitter->chunk, byte, line);
    emitter->last_line = line;
}

static const int8_t stack_effects[] = {
#define OPCODE(_, effect) effect,
#include "opcodes.inc"
#undef OPCODE
};

static void lit_emitter_emit2bytes(LitEmitter* emitter, uint16_t line, uint8_t a, uint8_t b)
{
    if(line < emitter->last_line)
    {
        // Egor-fail proofing
        line = emitter->last_line;
    }
    lit_write_chunk(emitter->state, emitter->chunk, a, line);
    lit_write_chunk(emitter->state, emitter->chunk, b, line);
    emitter->last_line = line;
}

static void lit_emitter_emit1op(LitEmitter* emitter, uint16_t line, LitOpCode op)
{
    LitCompiler* compiler;
    compiler = emitter->compiler;
    lit_emitter_emit1byte(emitter, line, (uint8_t)op);
    compiler->slots += stack_effects[(int)op];

    if(compiler->slots > (int)compiler->function->max_slots)
    {
        compiler->function->max_slots = (size_t)compiler->slots;
    }
}

static void lit_emitter_emit2ops(LitEmitter* emitter, uint16_t line, LitOpCode a, LitOpCode b)
{
    LitCompiler* compiler;
    compiler = emitter->compiler;
    lit_emitter_emit2bytes(emitter, line, (uint8_t)a, (uint8_t)b);
    compiler->slots += stack_effects[(int)a] + stack_effects[(int)b];
    if(compiler->slots > (int)compiler->function->max_slots)
    {
        compiler->function->max_slots = (size_t)compiler->slots;
    }
}

static void lit_emitter_emitvaryingop(LitEmitter* emitter, uint16_t line, LitOpCode op, uint8_t arg)
{
    LitCompiler* compiler;
    compiler = emitter->compiler;
    lit_emitter_emit2bytes(emitter, line, (uint8_t)op, arg);
    compiler->slots -= arg;
    if(compiler->slots > (int)compiler->function->max_slots)
    {
        compiler->function->max_slots = (size_t)compiler->slots;
    }
}

static void lit_emitter_emitargedop(LitEmitter* emitter, uint16_t line, LitOpCode op, uint8_t arg)
{
    LitCompiler* compiler = emitter->compiler;

    lit_emitter_emit2bytes(emitter, line, (uint8_t)op, arg);
    compiler->slots += stack_effects[(int)op];

    if(compiler->slots > (int)compiler->function->max_slots)
    {
        compiler->function->max_slots = (size_t)compiler->slots;
    }
}

static void lit_emitter_emitshort(LitEmitter* emitter, uint16_t line, uint16_t value)
{
    lit_emitter_emit2bytes(emitter, line, (uint8_t)((value >> 8) & 0xff), (uint8_t)(value & 0xff));
}

static void lit_emitter_emitbyteorshort(LitEmitter* emitter, uint16_t line, uint8_t a, uint8_t b, uint16_t index)
{
    if(index > UINT8_MAX)
    {
        lit_emitter_emit1op(emitter, line, (LitOpCode)b);
        lit_emitter_emitshort(emitter, line, (uint16_t)index);
    }
    else
    {
        lit_emitter_emitargedop(emitter, line, (LitOpCode)a, (uint8_t)index);
    }
}

static void lit_compiler_compiler(LitEmitter* emitter, LitCompiler* compiler, LitFuncType type)
{
    lit_loclist_init(&compiler->locals);

    compiler->type = type;
    compiler->scope_depth = 0;
    compiler->enclosing = (struct LitCompiler*)emitter->compiler;
    compiler->skip_return = false;
    compiler->function = lit_create_function(emitter->state, emitter->module);
    compiler->loop_depth = 0;

    emitter->compiler = compiler;

    const char* name = emitter->state->scanner->file_name;

    if(emitter->compiler == NULL)
    {
        compiler->function->name = lit_string_copy(emitter->state, name, strlen(name));
    }

    emitter->chunk = &compiler->function->chunk;

    if(lit_astopt_isoptenabled(LITOPTSTATE_LINE_INFO))
    {
        emitter->chunk->has_line_info = false;
    }

    if(type == LITFUNC_METHOD || type == LITFUNC_STATIC_METHOD || type == LITFUNC_CONSTRUCTOR)
    {
        lit_loclist_push(emitter->state, &compiler->locals, (LitLocal){ "this", 4, -1, false, false });
    }
    else
    {
        lit_loclist_push(emitter->state, &compiler->locals, (LitLocal){ "", 0, -1, false, false });
    }

    compiler->slots = 1;
    compiler->max_slots = 1;
}

static void lit_emitter_emitreturn(LitEmitter* emitter, size_t line)
{
    if(emitter->compiler->type == LITFUNC_CONSTRUCTOR)
    {
        lit_emitter_emitargedop(emitter, line, OP_GET_LOCAL, 0);
        lit_emitter_emit1op(emitter, line, OP_RETURN);
    }
    else if(emitter->previous_was_expression_statement && emitter->chunk->count > 0)
    {
        emitter->chunk->count--;// Remove the OP_POP
        lit_emitter_emit1op(emitter, line, OP_RETURN);
    }
    else
    {
        lit_emitter_emit2ops(emitter, line, OP_NULL, OP_RETURN);
    }
}

static LitFunction* lit_compiler_end(LitEmitter* emitter, LitString* name)
{
    if(!emitter->compiler->skip_return)
    {
        lit_emitter_emitreturn(emitter, emitter->last_line);
        emitter->compiler->skip_return = true;
    }

    LitFunction* function = emitter->compiler->function;

    lit_loclist_destroy(emitter->state, &emitter->compiler->locals);

    emitter->compiler = (LitCompiler*)emitter->compiler->enclosing;
    emitter->chunk = emitter->compiler == NULL ? NULL : &emitter->compiler->function->chunk;

    if(name != NULL)
    {
        function->name = name;
    }

#ifdef LIT_TRACE_CHUNK
    lit_disassemble_chunk(&function->chunk, function->name->chars, NULL);
#endif

    return function;
}

static void lit_emitter_beginscope(LitEmitter* emitter)
{
    emitter->compiler->scope_depth++;
}

static void lit_emitter_endscope(LitEmitter* emitter, uint16_t line)
{
    emitter->compiler->scope_depth--;

    LitCompiler* compiler = emitter->compiler;
    LitLocList* locals = &compiler->locals;

    while(locals->count > 0 && locals->values[locals->count - 1].depth > compiler->scope_depth)
    {
        if(locals->values[locals->count - 1].captured)
        {
            lit_emitter_emit1op(emitter, line, OP_CLOSE_UPVALUE);
        }
        else
        {
            lit_emitter_emit1op(emitter, line, OP_POP);
        }

        locals->count--;
    }
}

static void lit_emitter_raiseerror(LitEmitter* emitter, size_t line, LitError lit_emitter_raiseerror, ...)
{
    va_list args;
    va_start(args, lit_emitter_raiseerror);
    lit_state_raiseerror(emitter->state, COMPILE_ERROR, lit_vformat_error(emitter->state, line, lit_emitter_raiseerror, args)->chars);
    va_end(args);
}

static uint16_t lit_emitter_addconstant(LitEmitter* emitter, size_t line, LitValue value)
{
    size_t constant = lit_chunk_add_constant(emitter->state, emitter->chunk, value);

    if(constant >= UINT16_MAX)
    {
        lit_emitter_raiseerror(emitter, line, LITERROR_TOO_MANY_CONSTANTS);
    }

    return constant;
}

static size_t lit_emitter_emitconstant(LitEmitter* emitter, size_t line, LitValue value)
{
    size_t constant = lit_chunk_add_constant(emitter->state, emitter->chunk, value);

    if(constant < UINT8_MAX)
    {
        lit_emitter_emitargedop(emitter, line, OP_CONSTANT, constant);
    }
    else if(constant < UINT16_MAX)
    {
        lit_emitter_emit1op(emitter, line, OP_CONSTANT_LONG);
        lit_emitter_emitshort(emitter, line, constant);
    }
    else
    {
        lit_emitter_raiseerror(emitter, line, LITERROR_TOO_MANY_CONSTANTS);
    }

    return constant;
}

static int lit_emitter_addprivate(LitEmitter* emitter, const char* name, size_t length, size_t line, bool constant)
{
    LitPrivList* privates = &emitter->privates;

    if(privates->count == UINT16_MAX)
    {
        lit_emitter_raiseerror(emitter, line, LITERROR_TOO_MANY_PRIVATES);
    }

    LitTable* private_names = &emitter->module->private_names->values;
    LitString* key = lit_table_find_string(private_names, name, length, lit_util_hashstring(name, length));

    if(key != NULL)
    {
        lit_emitter_raiseerror(emitter, line, LITERROR_VAR_REDEFINED, length, name);

        LitValue index;
        lit_table_get(private_names, key, &index);

        return lit_value_asnumber(index);
    }

    LitState* state = emitter->state;
    int index = (int)privates->count;

    lit_privlist_push(state, privates, (LitPrivate){ false, constant });

    lit_table_set(state, private_names, lit_string_copy(state, name, length), lit_value_numbertovalue(state, index));
    emitter->module->private_count++;

    return index;
}

static int lit_emitter_resolveprivate(LitEmitter* emitter, const char* name, size_t length, size_t line)
{
    LitTable* private_names = &emitter->module->private_names->values;
    LitString* key = lit_table_find_string(private_names, name, length, lit_util_hashstring(name, length));

    if(key != NULL)
    {
        LitValue index;
        lit_table_get(private_names, key, &index);

        int number_index = lit_value_asnumber(index);

        if(!emitter->privates.values[number_index].initialized)
        {
            lit_emitter_raiseerror(emitter, line, LITERROR_VARIABLE_USED_IN_INIT, length, name);
        }

        return number_index;
    }

    return -1;
}

static int lit_emitter_addlocal(LitEmitter* emitter, const char* name, size_t length, size_t line, bool constant)
{
    LitCompiler* compiler = emitter->compiler;
    LitLocList* locals = &compiler->locals;

    if(locals->count == UINT16_MAX)
    {
        lit_emitter_raiseerror(emitter, line, LITERROR_TOO_MANY_LOCALS);
    }

    for(int i = (int)locals->count - 1; i >= 0; i--)
    {
        LitLocal* local = &locals->values[i];

        if(local->depth != UINT16_MAX && local->depth < compiler->scope_depth)
        {
            break;
        }

        if(length == local->length && memcmp(local->name, name, length) == 0)
        {
            lit_emitter_raiseerror(emitter, line, LITERROR_VAR_REDEFINED, length, name);
        }
    }

    lit_loclist_push(emitter->state, locals, (LitLocal){ name, length, UINT16_MAX, false, constant });

    return (int)locals->count - 1;
}

static int lit_emitter_resolvelocal(LitEmitter* emitter, LitCompiler* compiler, const char* name, size_t length, size_t line)
{
    LitLocList* locals = &compiler->locals;

    for(int i = (int)locals->count - 1; i >= 0; i--)
    {
        LitLocal* local = &locals->values[i];

        if(local->length == length && memcmp(local->name, name, length) == 0)
        {
            if(local->depth == UINT16_MAX)
            {
                lit_emitter_raiseerror(emitter, line, LITERROR_VARIABLE_USED_IN_INIT, length, name);
            }

            return i;
        }
    }

    return -1;
}

static int lit_emitter_addupvalue(LitEmitter* emitter, LitCompiler* compiler, uint8_t index, size_t line, bool is_local)
{
    size_t upvalue_count = compiler->function->upvalue_count;

    for(size_t i = 0; i < upvalue_count; i++)
    {
        LitCompilerUpvalue* upvalue = &compiler->upvalues[i];

        if(upvalue->index == index && upvalue->isLocal == is_local)
        {
            return i;
        }
    }

    if(upvalue_count == UINT16_COUNT)
    {
        lit_emitter_raiseerror(emitter, line, LITERROR_TOO_MANY_UPVALUES);
        return 0;
    }

    compiler->upvalues[upvalue_count].isLocal = is_local;
    compiler->upvalues[upvalue_count].index = index;

    return compiler->function->upvalue_count++;
}

static int lit_emitter_resolveupvalue(LitEmitter* emitter, LitCompiler* compiler, const char* name, size_t length, size_t line)
{
    if(compiler->enclosing == NULL)
    {
        return -1;
    }

    int local = lit_emitter_resolvelocal(emitter, (LitCompiler*)compiler->enclosing, name, length, line);

    if(local != -1)
    {
        ((LitCompiler*)compiler->enclosing)->locals.values[local].captured = true;
        return lit_emitter_addupvalue(emitter, compiler, (uint8_t)local, line, true);
    }

    int upvalue = lit_emitter_resolveupvalue(emitter, (LitCompiler*)compiler->enclosing, name, length, line);

    if(upvalue != -1)
    {
        return lit_emitter_addupvalue(emitter, compiler, (uint8_t)upvalue, line, false);
    }

    return -1;
}

static void lit_emitter_marklocalinit(LitEmitter* emitter, size_t index)
{
    emitter->compiler->locals.values[index].depth = emitter->compiler->scope_depth;
}

static void lit_emitter_markprivateinit(LitEmitter* emitter, size_t index)
{
    emitter->privates.values[index].initialized = true;
}

static size_t lit_emitter_emitjump(LitEmitter* emitter, LitOpCode code, size_t line)
{
    lit_emitter_emit1op(emitter, line, code);
    lit_emitter_emit2bytes(emitter, line, 0xff, 0xff);

    return emitter->chunk->count - 2;
}

static void lit_emitter_patchjump(LitEmitter* emitter, size_t offset, size_t line)
{
    size_t jump = emitter->chunk->count - offset - 2;

    if(jump > UINT16_MAX)
    {
        lit_emitter_raiseerror(emitter, line, LITERROR_JUMP_TOO_BIG);
    }

    emitter->chunk->code[offset] = (jump >> 8) & 0xff;
    emitter->chunk->code[offset + 1] = jump & 0xff;
}

static void lit_emitter_emitloop(LitEmitter* emitter, size_t start, size_t line)
{
    lit_emitter_emit1op(emitter, line, OP_JUMP_BACK);
    size_t offset = emitter->chunk->count - start + 2;

    if(offset > UINT16_MAX)
    {
        lit_emitter_raiseerror(emitter, line, LITERROR_JUMP_TOO_BIG);
    }

    lit_emitter_emitshort(emitter, line, offset);
}

static void lit_emitter_patchloopjumps(LitEmitter* emitter, LitUintList* breaks, size_t line)
{
    for(size_t i = 0; i < lit_uintlist_count(breaks); i++)
    {
        lit_emitter_patchjump(emitter, lit_uintlist_get(breaks, i), line);
    }

    lit_uintlist_destroy(emitter->state, breaks);
}

static bool lit_emitter_emitparamlist(LitEmitter* emitter, LitParamList* parameters, size_t line)
{
    for(size_t i = 0; i < parameters->count; i++)
    {
        LitParameter* parameter = &parameters->values[i];

        int index = lit_emitter_addlocal(emitter, parameter->name, parameter->length, line, false);
        lit_emitter_marklocalinit(emitter, index);

        // Vararg ...
        if(parameter->length == 3 && memcmp(parameter->name, "...", 3) == 0)
        {
            return true;
        }

        if(parameter->default_value != NULL)
        {
            lit_emitter_emitbyteorshort(emitter, line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, index);
            size_t jump = lit_emitter_emitjump(emitter, OP_NULL_OR, line);

            lit_emitter_emitexpression(emitter, parameter->default_value);
            lit_emitter_patchjump(emitter, jump, line);
            lit_emitter_emitbyteorshort(emitter, line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, index);
            lit_emitter_emit1op(emitter, line, OP_POP);
        }
    }

    return false;
}

static void lit_emitter_resolvestmt(LitEmitter* emitter, LitExpression* statement)
{
    switch(statement->type)
    {
        case LITEXPR_VARSTMT:
            {
                LitAssignVarExpr* varstmt = (LitAssignVarExpr*)statement;
                lit_emitter_markprivateinit(emitter, lit_emitter_addprivate(emitter, varstmt->name, varstmt->length, statement->line, varstmt->constant));
            }
            break;
        case LITEXPR_FUNCTION:
            {
                LitFunctionExpr* funcstmt = (LitFunctionExpr*)statement;
                if(!funcstmt->exported)
                {
                    lit_emitter_markprivateinit(emitter, lit_emitter_addprivate(emitter, funcstmt->name, funcstmt->length, statement->line, false));
                }
            }
            break;
        case LITEXPR_CLASS:
        case LITEXPR_BLOCK:
        case LITEXPR_FOR:
        case LITEXPR_WHILE:
        case LITEXPR_IFSTMT:
        case LITEXPR_CONTINUE:
        case LITEXPR_BREAK:
        case LITEXPR_RETURN:
        case LITEXPR_METHOD:
        case LITEXPR_FIELD:
        case LITEXPR_EXPRESSION:
            {
            }
            break;
    }
}

static bool lit_emitter_emitexpression(LitEmitter* emitter, LitExpression* expr)
{
    LitClassExpr* clstmt;
    LitCompiler compiler;
    LitExpression* expression;
    LitExpression* e;
    LitExpressionExpr* stmtexpr;
    LitField* field;
    LitFieldExpr* fieldstmt;
    LitFunction* function;
    LitFunction* getter;
    LitFunction* setter;
    LitFunctionExpr* funcstmt;
    LitLocal* local;
    LitLocList* locals;
    LitMethodExpr* mthstmt;
    LitExpression* blockstmt;
    LitExpression* s;
    LitExprList* statements;
    LitString* name;
    LitAssignVarExpr* var;
    LitAssignVarExpr* varstmt;
    LitForExpr* forstmt;
    LitWhileExpr* whilestmt;
    LitIfExpr* ifstmt;
    bool constructor;
    bool isexport;
    bool isprivate;
    bool islocal;
    bool vararg;
    int depth;
    int ii;
    int index;
    size_t start;
    size_t else_jump;
    size_t exit_jump;
    size_t body_jump;
    size_t end_jump;
    size_t i;
    size_t increment_start;
    size_t localcnt;
    size_t iterator;
    size_t line;
    size_t sequence;
    uint16_t local_count;
    uint8_t super;
    uint64_t* end_jumps;

    
    if(expr == NULL)
    {
        return false;
    }

    switch(expr->type)
    {
        case LITEXPR_LITERAL:
            {
                LitValue value = ((LitLiteralExpr*)expr)->value;
                if(lit_value_isnumber(value) || lit_value_isstring(value))
                {
                    lit_emitter_emitconstant(emitter, expr->line, value);
                }
                else if(lit_value_isbool(value))
                {
                    lit_emitter_emit1op(emitter, expr->line, lit_value_asbool(value) ? OP_TRUE : OP_FALSE);
                }
                else if(lit_value_isnull(value))
                {
                    lit_emitter_emit1op(emitter, expr->line, OP_NULL);
                }
                else
                {
                    UNREACHABLE;
                }
            }
            break;
        case LITEXPR_BINARY:
            {
                LitBinaryExpr* binexpr = (LitBinaryExpr*)expr;
                lit_emitter_emitexpression(emitter, binexpr->left);
                if(binexpr->right == NULL)
                {
                    break;
                }
                LitTokType op = binexpr->op;
                if(op == LITTOK_AMPERSAND_AMPERSAND || op == LITTOK_BAR_BAR || op == LITTOK_QUESTION_QUESTION)
                {
                    size_t jump = lit_emitter_emitjump(emitter, op == LITTOK_BAR_BAR ? OP_OR : (op == LITTOK_QUESTION_QUESTION ? OP_NULL_OR : OP_AND),
                                          emitter->last_line);
                    lit_emitter_emitexpression(emitter, binexpr->right);
                    lit_emitter_patchjump(emitter, jump, emitter->last_line);
                    break;
                }
                lit_emitter_emitexpression(emitter, binexpr->right);
                switch(op)
                {
                    case LITTOK_PLUS:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_ADD);
                        }
                        break;
                    case LITTOK_MINUS:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_SUBTRACT);
                        }
                        break;
                    case LITTOK_STAR:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_MULTIPLY);
                        }
                        break;
                    case LITTOK_STAR_STAR:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_POWER);
                        }
                        break;
                    case LITTOK_SLASH:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_DIVIDE);
                        }
                        break;
                    case LITTOK_SHARP:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_FLOOR_DIVIDE);
                        }
                        break;
                    case LITTOK_PERCENT:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_MOD);
                        }
                        break;
                    case LITTOK_IS:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_IS);
                        }
                        break;
                    case LITTOK_EQUAL_EQUAL:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_EQUAL);
                        }
                        break;
                    case LITTOK_BANG_EQUAL:
                        {
                            lit_emitter_emit2ops(emitter, expr->line, OP_EQUAL, OP_NOT);
                        }
                        break;
                    case LITTOK_GREATER:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_GREATER);
                        }
                        break;
                    case LITTOK_GREATER_EQUAL:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_GREATER_EQUAL);
                        }
                        break;
                    case LITTOK_LESS:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_LESS);
                        }
                        break;
                    case LITTOK_LESS_EQUAL:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_LESS_EQUAL);
                        }
                        break;
                    case LITTOK_LESS_LESS:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_LSHIFT);
                        }
                        break;
                    case LITTOK_GREATER_GREATER:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_RSHIFT);
                        }
                        break;
                    case LITTOK_BAR:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_BOR);
                        }
                        break;
                    case LITTOK_AMPERSAND:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_BAND);
                        }
                        break;
                    case LITTOK_CARET:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_BXOR);
                        }
                        break;
                    default:
                        {
                            fprintf(stderr, "in lit_emitter_emitexpression: binary expression #2 is NULL! might be a bug\n");
                            //return;
                            //UNREACHABLE;
                        }
                    break;
                }
            }
            break;
        case LITEXPR_UNARY:
            {
                LitUnaryExpr* unexpr = (LitUnaryExpr*)expr;
                lit_emitter_emitexpression(emitter, unexpr->right);
                switch(unexpr->op)
                {
                    case LITTOK_MINUS:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_NEGATE);
                        }
                        break;
                    case LITTOK_BANG:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_NOT);
                        }
                        break;
                    case LITTOK_TILDE:
                        {
                            lit_emitter_emit1op(emitter, expr->line, OP_BNOT);
                        }
                        break;
                    default:
                        {
                            fprintf(stderr, "in lit_emitter_emitexpression: unary expr is NULL! might be an internal bug\n");
                            //return;
                            //UNREACHABLE;
                        }
                        break;
                }
            }
            break;
        case LITEXPR_VAREXPR:
            {
                LitVarExpr* varexpr = (LitVarExpr*)expr;
                bool ref = emitter->emit_reference > 0;
                if(ref)
                {
                    emitter->emit_reference--;
                }
                int index = lit_emitter_resolvelocal(emitter, emitter->compiler, varexpr->name, varexpr->length, expr->line);
                if(index == -1)
                {
                    index = lit_emitter_resolveupvalue(emitter, emitter->compiler, varexpr->name, varexpr->length, expr->line);
                    if(index == -1)
                    {
                        index = lit_emitter_resolveprivate(emitter, varexpr->name, varexpr->length, expr->line);
                        if(index == -1)
                        {
                            lit_emitter_emit1op(emitter, expr->line, ref ? OP_REFERENCE_GLOBAL : OP_GET_GLOBAL);
                            lit_emitter_emitshort(emitter, expr->line,
                                       lit_emitter_addconstant(emitter, expr->line,
                                                    lit_value_objectvalue(lit_string_copy(emitter->state, varexpr->name, varexpr->length))));
                        }
                        else
                        {
                            if(ref)
                            {
                                lit_emitter_emit1op(emitter, expr->line, OP_REFERENCE_PRIVATE);
                                lit_emitter_emitshort(emitter, expr->line, index);
                            }
                            else
                            {
                                lit_emitter_emitbyteorshort(emitter, expr->line, OP_GET_PRIVATE, OP_GET_PRIVATE_LONG, index);
                            }
                        }
                    }
                    else
                    {
                        lit_emitter_emitargedop(emitter, expr->line, ref ? OP_REFERENCE_UPVALUE : OP_GET_UPVALUE, (uint8_t)index);
                    }
                }
                else
                {
                    if(ref)
                    {
                        lit_emitter_emit1op(emitter, expr->line, OP_REFERENCE_LOCAL);
                        lit_emitter_emitshort(emitter, expr->line, index);
                    }
                    else
                    {
                        lit_emitter_emitbyteorshort(emitter, expr->line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, index);
                    }
                }
            }
            break;
        case LITEXPR_ASSIGN:
            {
                LitAssignExpr* assignexpr = (LitAssignExpr*)expr;
                if(assignexpr->to->type == LITEXPR_VAREXPR)
                {
                    lit_emitter_emitexpression(emitter, assignexpr->value);
                    LitVarExpr* e = (LitVarExpr*)assignexpr->to;
                    int index = lit_emitter_resolvelocal(emitter, emitter->compiler, e->name, e->length, assignexpr->to->line);
                    if(index == -1)
                    {
                        index = lit_emitter_resolveupvalue(emitter, emitter->compiler, e->name, e->length, assignexpr->to->line);
                        if(index == -1)
                        {
                            index = lit_emitter_resolveprivate(emitter, e->name, e->length, assignexpr->to->line);
                            if(index == -1)
                            {
                                lit_emitter_emit1op(emitter, expr->line, OP_SET_GLOBAL);
                                lit_emitter_emitshort(emitter, expr->line,
                                           lit_emitter_addconstant(emitter, expr->line,
                                                        lit_value_objectvalue(lit_string_copy(emitter->state, e->name, e->length))));
                            }
                            else
                            {
                                if(emitter->privates.values[index].constant)
                                {
                                    lit_emitter_raiseerror(emitter, expr->line, LITERROR_CONSTANT_MODIFIED, e->length, e->name);
                                }
                                lit_emitter_emitbyteorshort(emitter, expr->line, OP_SET_PRIVATE, OP_SET_PRIVATE_LONG, index);
                            }
                        }
                        else
                        {
                            lit_emitter_emitargedop(emitter, expr->line, OP_SET_UPVALUE, (uint8_t)index);
                        }
                        break;
                    }
                    else
                    {
                        if(emitter->compiler->locals.values[index].constant)
                        {
                            lit_emitter_raiseerror(emitter, expr->line, LITERROR_CONSTANT_MODIFIED, e->length, e->name);
                        }

                        lit_emitter_emitbyteorshort(emitter, expr->line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, index);
                    }
                }
                else if(assignexpr->to->type == LITEXPR_GET)
                {
                    lit_emitter_emitexpression(emitter, assignexpr->value);
                    LitGetExpr* e = (LitGetExpr*)assignexpr->to;
                    lit_emitter_emitexpression(emitter, e->where);
                    lit_emitter_emitexpression(emitter, assignexpr->value);
                    lit_emitter_emitconstant(emitter, emitter->last_line, lit_value_objectvalue(lit_string_copy(emitter->state, e->name, e->length)));
                    lit_emitter_emit2ops(emitter, emitter->last_line, OP_SET_FIELD, OP_POP);
                }
                else if(assignexpr->to->type == LITEXPR_SUBSCRIPT)
                {
                    LitSubscriptExpr* e = (LitSubscriptExpr*)assignexpr->to;
                    lit_emitter_emitexpression(emitter, e->array);
                    lit_emitter_emitexpression(emitter, e->index);
                    lit_emitter_emitexpression(emitter, assignexpr->value);
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_SUBSCRIPT_SET);
                }
                else if(assignexpr->to->type == LITEXPR_REFERENCE)
                {
                    lit_emitter_emitexpression(emitter, assignexpr->value);
                    lit_emitter_emitexpression(emitter, ((LitReferenceExpr*)assignexpr->to)->to);
                    lit_emitter_emit1op(emitter, expr->line, OP_SET_REFERENCE);
                }
                else
                {
                    lit_emitter_raiseerror(emitter, expr->line, LITERROR_INVALID_ASSIGMENT_TARGET);
                }
            }
            break;
        case LITEXPR_CALL:
            {
                LitCallExpr* callexpr = (LitCallExpr*)expr;
                bool method = callexpr->callee->type == LITEXPR_GET;
                bool super = callexpr->callee->type == LITEXPR_SUPER;
                if(method)
                {
                    ((LitGetExpr*)callexpr->callee)->ignore_emit = true;
                }
                else if(super)
                {
                    ((LitSuperExpr*)callexpr->callee)->ignore_emit = true;
                }
                lit_emitter_emitexpression(emitter, callexpr->callee);
                if(super)
                {
                    lit_emitter_emitargedop(emitter, expr->line, OP_GET_LOCAL, 0);
                }
                for(size_t i = 0; i < callexpr->args.count; i++)
                {
                    LitExpression* e = callexpr->args.values[i];
                    if(e->type == LITEXPR_VAREXPR)
                    {
                        LitVarExpr* ee = (LitVarExpr*)e;
                        // Vararg ...
                        if(ee->length == 3 && memcmp(ee->name, "...", 3) == 0)
                        {
                            lit_emitter_emitargedop(emitter, e->line, OP_VARARG,
                                          lit_emitter_resolvelocal(emitter, emitter->compiler, "...", 3, expr->line));
                            break;
                        }
                    }
                    lit_emitter_emitexpression(emitter, e);
                }
                if(method || super)
                {
                    if(method)
                    {
                        LitGetExpr* e = (LitGetExpr*)callexpr->callee;

                        lit_emitter_emitvaryingop(emitter, expr->line,
                                        ((LitGetExpr*)callexpr->callee)->ignore_result ? OP_INVOKE_IGNORING : OP_INVOKE,
                                        (uint8_t)callexpr->args.count);
                        lit_emitter_emitshort(emitter, emitter->last_line,
                                   lit_emitter_addconstant(emitter, emitter->last_line,
                                                lit_value_objectvalue(lit_string_copy(emitter->state, e->name, e->length))));
                    }
                    else
                    {
                        LitSuperExpr* e = (LitSuperExpr*)callexpr->callee;
                        uint8_t index = lit_emitter_resolveupvalue(emitter, emitter->compiler, "super", 5, emitter->last_line);
                        lit_emitter_emitargedop(emitter, expr->line, OP_GET_UPVALUE, index);
                        lit_emitter_emitvaryingop(emitter, emitter->last_line,
                                        ((LitSuperExpr*)callexpr->callee)->ignore_result ? OP_INVOKE_SUPER_IGNORING : OP_INVOKE_SUPER,
                                        (uint8_t)callexpr->args.count);
                        lit_emitter_emitshort(emitter, emitter->last_line, lit_emitter_addconstant(emitter, emitter->last_line, lit_value_objectvalue(e->method)));
                    }
                }
                else
                {
                    lit_emitter_emitvaryingop(emitter, expr->line, OP_CALL, (uint8_t)callexpr->args.count);
                }
                if(method)
                {
                    LitExpression* get = callexpr->callee;
                    while(get != NULL)
                    {
                        if(get->type == LITEXPR_GET)
                        {
                            LitGetExpr* getter = (LitGetExpr*)get;
                            if(getter->jump > 0)
                            {
                                lit_emitter_patchjump(emitter, getter->jump, emitter->last_line);
                            }
                            get = getter->where;
                        }
                        else if(get->type == LITEXPR_SUBSCRIPT)
                        {
                            get = ((LitSubscriptExpr*)get)->array;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                if(callexpr->init == NULL)
                {
                    return false;
                }
                LitObjectExpr* init = (LitObjectExpr*)callexpr->init;
                for(size_t i = 0; i < init->values.count; i++)
                {
                    LitExpression* e = init->values.values[i];
                    emitter->last_line = e->line;
                    lit_emitter_emitconstant(emitter, emitter->last_line, lit_vallist_get(&init->keys, i));
                    lit_emitter_emitexpression(emitter, e);
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_PUSH_OBJECT_FIELD);
                }
            }
            break;
        case LITEXPR_GET:
            {
                LitGetExpr* getexpr = (LitGetExpr*)expr;
                bool ref = emitter->emit_reference > 0;
                if(ref)
                {
                    emitter->emit_reference--;
                }
                lit_emitter_emitexpression(emitter, getexpr->where);
                if(getexpr->jump == 0)
                {
                    getexpr->jump = lit_emitter_emitjump(emitter, OP_JUMP_IF_NULL, emitter->last_line);
                    if(!getexpr->ignore_emit)
                    {
                        lit_emitter_emitconstant(emitter, emitter->last_line,
                                      lit_value_objectvalue(lit_string_copy(emitter->state, getexpr->name, getexpr->length)));
                        lit_emitter_emit1op(emitter, emitter->last_line, ref ? OP_REFERENCE_FIELD : OP_GET_FIELD);
                    }
                    lit_emitter_patchjump(emitter, getexpr->jump, emitter->last_line);
                }
                else if(!getexpr->ignore_emit)
                {
                    lit_emitter_emitconstant(emitter, emitter->last_line, lit_value_objectvalue(lit_string_copy(emitter->state, getexpr->name, getexpr->length)));
                    lit_emitter_emit1op(emitter, emitter->last_line, ref ? OP_REFERENCE_FIELD : OP_GET_FIELD);
                }
            }
            break;
        case LITEXPR_SET:
            {
                LitSetExpr* setexpr = (LitSetExpr*)expr;
                lit_emitter_emitexpression(emitter, setexpr->where);
                lit_emitter_emitexpression(emitter, setexpr->value);
                lit_emitter_emitconstant(emitter, emitter->last_line, lit_value_objectvalue(lit_string_copy(emitter->state, setexpr->name, setexpr->length)));
                lit_emitter_emit1op(emitter, emitter->last_line, OP_SET_FIELD);
            }
            break;
        case LITEXPR_LAMBDA:
            {
                LitLambdaExpr* lambdaexpr = (LitLambdaExpr*)expr;
                LitString* name = lit_value_asstring(lit_string_format(emitter->state, "lambda @:@", lit_value_objectvalue(emitter->module->name),
                                                              lit_string_numbertostring(emitter->state, expr->line)));
                LitCompiler compiler;
                lit_compiler_compiler(emitter, &compiler, LITFUNC_REGULAR);
                lit_emitter_beginscope(emitter);
                bool vararg = lit_emitter_emitparamlist(emitter, &lambdaexpr->parameters, expr->line);
                if(lambdaexpr->body != NULL)
                {
                    bool single_expression = lambdaexpr->body->type == LITEXPR_EXPRESSION;
                    if(single_expression)
                    {
                        compiler.skip_return = true;
                        ((LitExpressionExpr*)lambdaexpr->body)->pop = false;
                    }
                    lit_emitter_emitexpression(emitter, lambdaexpr->body);
                    if(single_expression)
                    {
                        lit_emitter_emit1op(emitter, emitter->last_line, OP_RETURN);
                    }
                }
                lit_emitter_endscope(emitter, emitter->last_line);
                LitFunction* function = lit_compiler_end(emitter, name);
                function->arg_count = lambdaexpr->parameters.count;
                function->max_slots += function->arg_count;
                function->vararg = vararg;
                if(function->upvalue_count > 0)
                {
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_CLOSURE);
                    lit_emitter_emitshort(emitter, emitter->last_line, lit_emitter_addconstant(emitter, emitter->last_line, lit_value_objectvalue(function)));
                    for(size_t i = 0; i < function->upvalue_count; i++)
                    {
                        lit_emitter_emit2bytes(emitter, emitter->last_line, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
                    }
                }
                else
                {
                    lit_emitter_emitconstant(emitter, emitter->last_line, lit_value_objectvalue(function));
                }
            }
            break;
        case LITEXPR_ARRAY:
            {
                LitArrayExpr* arrexpr = (LitArrayExpr*)expr;
                lit_emitter_emit1op(emitter, expr->line, OP_ARRAY);
                for(size_t i = 0; i < arrexpr->values.count; i++)
                {
                    lit_emitter_emitexpression(emitter, arrexpr->values.values[i]);
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_PUSH_ARRAY_ELEMENT);
                }
            }
            break;
        case LITEXPR_OBJECT:
            {
                LitObjectExpr* objexpr = (LitObjectExpr*)expr;
                lit_emitter_emit1op(emitter, expr->line, OP_OBJECT);
                for(size_t i = 0; i < objexpr->values.count; i++)
                {
                    lit_emitter_emitconstant(emitter, emitter->last_line, lit_vallist_get(&objexpr->keys, i));
                    lit_emitter_emitexpression(emitter, objexpr->values.values[i]);
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_PUSH_OBJECT_FIELD);
                }
            }
            break;
        case LITEXPR_SUBSCRIPT:
            {
                LitSubscriptExpr* subscrexpr = (LitSubscriptExpr*)expr;
                lit_emitter_emitexpression(emitter, subscrexpr->array);
                lit_emitter_emitexpression(emitter, subscrexpr->index);
                lit_emitter_emit1op(emitter, expr->line, OP_SUBSCRIPT_GET);
            }
            break;
        case LITEXPR_THIS:
            {
                LitFuncType type = emitter->compiler->type;
                if(type == LITFUNC_STATIC_METHOD)
                {
                    lit_emitter_raiseerror(emitter, expr->line, LITERROR_THIS_MISSUSE, "in static methods");
                }
                if(type == LITFUNC_CONSTRUCTOR || type == LITFUNC_METHOD)
                {
                    lit_emitter_emitargedop(emitter, expr->line, OP_GET_LOCAL, 0);
                }
                else
                {
                    if(emitter->compiler->enclosing == NULL)
                    {
                        lit_emitter_raiseerror(emitter, expr->line, LITERROR_THIS_MISSUSE, "in functions outside of any class");
                    }
                    else
                    {
                        int local = lit_emitter_resolvelocal(emitter, (LitCompiler*)emitter->compiler->enclosing, "this", 4, expr->line);
                        lit_emitter_emitargedop(emitter, expr->line, OP_GET_UPVALUE,
                                      lit_emitter_addupvalue(emitter, emitter->compiler, local, expr->line, true));
                    }
                }
            }
            break;
        case LITEXPR_SUPER:
            {
                if(emitter->compiler->type == LITFUNC_STATIC_METHOD)
                {
                    lit_emitter_raiseerror(emitter, expr->line, LITERROR_SUPER_MISSUSE, "in static methods");
                }
                else if(!emitter->class_has_super)
                {
                    lit_emitter_raiseerror(emitter, expr->line, LITERROR_NO_SUPER, emitter->class_name->chars);
                }
                LitSuperExpr* superexpr = (LitSuperExpr*)expr;
                if(!superexpr->ignore_emit)
                {
                    uint8_t index = lit_emitter_resolveupvalue(emitter, emitter->compiler, "super", 5, emitter->last_line);
                    lit_emitter_emitargedop(emitter, expr->line, OP_GET_LOCAL, 0);
                    lit_emitter_emitargedop(emitter, expr->line, OP_GET_UPVALUE, index);
                    lit_emitter_emit1op(emitter, expr->line, OP_GET_SUPER_METHOD);
                    lit_emitter_emitshort(emitter, expr->line, lit_emitter_addconstant(emitter, expr->line, lit_value_objectvalue(superexpr->method)));
                }
            }
            break;
        case LITEXPR_RANGE:
            {
                LitRangeExpr* rangeexpr = (LitRangeExpr*)expr;
                lit_emitter_emitexpression(emitter, rangeexpr->to);
                lit_emitter_emitexpression(emitter, rangeexpr->from);
                lit_emitter_emit1op(emitter, expr->line, OP_RANGE);
            }
            break;
        case LITEXPR_TERNARY:
            {
                LitTernaryExpr* ifexpr = (LitTernaryExpr*)expr;
                lit_emitter_emitexpression(emitter, ifexpr->condition);
                uint64_t else_jump = lit_emitter_emitjump(emitter, OP_JUMP_IF_FALSE, expr->line);
                lit_emitter_emitexpression(emitter, ifexpr->if_branch);
                uint64_t end_jump = lit_emitter_emitjump(emitter, OP_JUMP, emitter->last_line);
                lit_emitter_patchjump(emitter, else_jump, ifexpr->else_branch->line);
                lit_emitter_emitexpression(emitter, ifexpr->else_branch);

                lit_emitter_patchjump(emitter, end_jump, emitter->last_line);
            }
            break;
        case LITEXPR_INTERPOLATION:
            {
                LitInterpolationExpr* ifexpr = (LitInterpolationExpr*)expr;
                lit_emitter_emit1op(emitter, expr->line, OP_ARRAY);
                for(size_t i = 0; i < ifexpr->expressions.count; i++)
                {
                    lit_emitter_emitexpression(emitter, ifexpr->expressions.values[i]);
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_PUSH_ARRAY_ELEMENT);
                }
                lit_emitter_emitvaryingop(emitter, emitter->last_line, OP_INVOKE, 0);
                lit_emitter_emitshort(emitter, emitter->last_line,
                           lit_emitter_addconstant(emitter, emitter->last_line, OBJECT_CONST_STRING(emitter->state, "join")));
            }
            break;
        case LITEXPR_REFERENCE:
            {
                LitExpression* to = ((LitReferenceExpr*)expr)->to;
                if(to->type != LITEXPR_VAREXPR && to->type != LITEXPR_GET && to->type != LITEXPR_THIS && to->type != LITEXPR_SUPER)
                {
                    lit_emitter_raiseerror(emitter, expr->line, LITERROR_INVALID_REFERENCE_TARGET);
                    break;
                }
                int old = emitter->emit_reference;
                emitter->emit_reference++;
                lit_emitter_emitexpression(emitter, to);
                emitter->emit_reference = old;
            }
            break;
        case LITEXPR_EXPRESSION:
            {
                stmtexpr = (LitExpressionExpr*)expr;
                lit_emitter_emitexpression(emitter, stmtexpr->expression);
                if(stmtexpr->pop)
                {
                    lit_emitter_emit1op(emitter, expr->line, OP_POP);
                }
            }
            break;
        case LITEXPR_BLOCK:
            {
                statements = &((LitBlockExpr*)expr)->statements;
                lit_emitter_beginscope(emitter);
                {
                    for(i = 0; i < statements->count; i++)
                    {
                        blockstmt = statements->values[i];

                        if(lit_emitter_emitexpression(emitter, blockstmt))
                        {
                            break;
                        }
                    }
                }
                lit_emitter_endscope(emitter, emitter->last_line);
            }
            break;
        case LITEXPR_VARSTMT:
            {
                varstmt = (LitAssignVarExpr*)expr;
                line = expr->line;
                isprivate = emitter->compiler->enclosing == NULL && emitter->compiler->scope_depth == 0;
                index = isprivate ? lit_emitter_resolveprivate(emitter, varstmt->name, varstmt->length, expr->line) :
                                      lit_emitter_addlocal(emitter, varstmt->name, varstmt->length, expr->line, varstmt->constant);
                if(varstmt->init == NULL)
                {
                    lit_emitter_emit1op(emitter, line, OP_NULL);
                }
                else
                {
                    lit_emitter_emitexpression(emitter, varstmt->init);
                }
                if(isprivate)
                {
                    lit_emitter_markprivateinit(emitter, index);
                }
                else
                {
                    lit_emitter_marklocalinit(emitter, index);
                }
                lit_emitter_emitbyteorshort(emitter, expr->line, isprivate ? OP_SET_PRIVATE : OP_SET_LOCAL,
                                   isprivate ? OP_SET_PRIVATE_LONG : OP_SET_LOCAL_LONG, index);
                if(isprivate)
                {
                    // Privates don't live on stack, so we need to pop them manually
                    lit_emitter_emit1op(emitter, expr->line, OP_POP);
                }
            }
            break;
        case LITEXPR_IFSTMT:
            {
                ifstmt = (LitIfExpr*)expr;
                else_jump = 0;
                end_jump = 0;
                if(ifstmt->condition == NULL)
                {
                    else_jump = lit_emitter_emitjump(emitter, OP_JUMP, expr->line);
                }
                else
                {
                    lit_emitter_emitexpression(emitter, ifstmt->condition);
                    else_jump = lit_emitter_emitjump(emitter, OP_JUMP_IF_FALSE, expr->line);
                    lit_emitter_emitexpression(emitter, ifstmt->if_branch);
                    end_jump = lit_emitter_emitjump(emitter, OP_JUMP, emitter->last_line);
                }
                /* important: end_jumps must be N*sizeof(uint64_t) - merely allocating N isn't enough! */
                //uint64_t end_jumps[ifstmt->elseif_branches == NULL ? 1 : ifstmt->elseif_branches->count];
                end_jumps = (uint64_t*)malloc(sizeof(uint64_t) * (ifstmt->elseif_branches == NULL ? 1 : ifstmt->elseif_branches->count));
                if(ifstmt->elseif_branches != NULL)
                {
                    for(i = 0; i < ifstmt->elseif_branches->count; i++)
                    {
                        e = ifstmt->elseif_conditions->values[i];
                        if(e == NULL)
                        {
                            continue;
                        }
                        lit_emitter_patchjump(emitter, else_jump, e->line);
                        lit_emitter_emitexpression(emitter, e);
                        else_jump = lit_emitter_emitjump(emitter, OP_JUMP_IF_FALSE, emitter->last_line);
                        lit_emitter_emitexpression(emitter, ifstmt->elseif_branches->values[i]);

                        end_jumps[i] = lit_emitter_emitjump(emitter, OP_JUMP, emitter->last_line);
                    }
                }
                if(ifstmt->else_branch != NULL)
                {
                    lit_emitter_patchjump(emitter, else_jump, ifstmt->else_branch->line);
                    lit_emitter_emitexpression(emitter, ifstmt->else_branch);
                }
                else
                {
                    lit_emitter_patchjump(emitter, else_jump, emitter->last_line);
                }
                if(end_jump != 0)
                {
                    lit_emitter_patchjump(emitter, end_jump, emitter->last_line);
                }
                if(ifstmt->elseif_branches != NULL)
                {
                    for(i = 0; i < ifstmt->elseif_branches->count; i++)
                    {
                        if(ifstmt->elseif_branches->values[i] == NULL)
                        {
                            continue;
                        }
                        lit_emitter_patchjump(emitter, end_jumps[i], ifstmt->elseif_branches->values[i]->line);
                    }
                }
                free(end_jumps);
            }
            break;
        case LITEXPR_WHILE:
            {
                whilestmt = (LitWhileExpr*)expr;
                start = emitter->chunk->count;
                emitter->loop_start = start;
                emitter->compiler->loop_depth++;
                lit_emitter_emitexpression(emitter, whilestmt->condition);
                exit_jump = lit_emitter_emitjump(emitter, OP_JUMP_IF_FALSE, expr->line);
                lit_emitter_emitexpression(emitter, whilestmt->body);
                lit_emitter_patchloopjumps(emitter, &emitter->continues, emitter->last_line);
                lit_emitter_emitloop(emitter, start, emitter->last_line);
                lit_emitter_patchjump(emitter, exit_jump, emitter->last_line);
                lit_emitter_patchloopjumps(emitter, &emitter->breaks, emitter->last_line);
                emitter->compiler->loop_depth--;
            }
            break;
        case LITEXPR_FOR:
            {
                forstmt = (LitForExpr*)expr;
                lit_emitter_beginscope(emitter);
                emitter->compiler->loop_depth++;
                if(forstmt->c_style)
                {
                    if(forstmt->var != NULL)
                    {
                        lit_emitter_emitexpression(emitter, forstmt->var);
                    }
                    else if(forstmt->init != NULL)
                    {
                        lit_emitter_emitexpression(emitter, forstmt->init);
                    }
                    start = emitter->chunk->count;
                    exit_jump = 0;
                    if(forstmt->condition != NULL)
                    {
                        lit_emitter_emitexpression(emitter, forstmt->condition);
                        exit_jump = lit_emitter_emitjump(emitter, OP_JUMP_IF_FALSE, emitter->last_line);
                    }
                    if(forstmt->increment != NULL)
                    {
                        body_jump = lit_emitter_emitjump(emitter, OP_JUMP, emitter->last_line);
                        increment_start = emitter->chunk->count;
                        lit_emitter_emitexpression(emitter, forstmt->increment);
                        lit_emitter_emit1op(emitter, emitter->last_line, OP_POP);
                        lit_emitter_emitloop(emitter, start, emitter->last_line);
                        start = increment_start;
                        lit_emitter_patchjump(emitter, body_jump, emitter->last_line);
                    }
                    emitter->loop_start = start;
                    lit_emitter_beginscope(emitter);
                    if(forstmt->body != NULL)
                    {
                        if(forstmt->body->type == LITEXPR_BLOCK)
                        {
                            statements = &((LitBlockExpr*)forstmt->body)->statements;
                            for(i = 0; i < statements->count; i++)
                            {
                                lit_emitter_emitexpression(emitter, statements->values[i]);
                            }
                        }
                        else
                        {
                            lit_emitter_emitexpression(emitter, forstmt->body);
                        }
                    }
                    lit_emitter_patchloopjumps(emitter, &emitter->continues, emitter->last_line);
                    lit_emitter_endscope(emitter, emitter->last_line);
                    lit_emitter_emitloop(emitter, start, emitter->last_line);
                    if(forstmt->condition != NULL)
                    {
                        lit_emitter_patchjump(emitter, exit_jump, emitter->last_line);
                    }
                }
                else
                {
                    sequence = lit_emitter_addlocal(emitter, "seq ", 4, expr->line, false);
                    lit_emitter_marklocalinit(emitter, sequence);
                    lit_emitter_emitexpression(emitter, forstmt->condition);
                    lit_emitter_emitbyteorshort(emitter, emitter->last_line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, sequence);
                    iterator = lit_emitter_addlocal(emitter, "iter ", 5, expr->line, false);
                    lit_emitter_marklocalinit(emitter, iterator);
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_NULL);
                    lit_emitter_emitbyteorshort(emitter, emitter->last_line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, iterator);
                    start = emitter->chunk->count;
                    emitter->loop_start = emitter->chunk->count;
                    // iter = seq.iterator(iter)
                    lit_emitter_emitbyteorshort(emitter, emitter->last_line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, sequence);
                    lit_emitter_emitbyteorshort(emitter, emitter->last_line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, iterator);
                    lit_emitter_emitvaryingop(emitter, emitter->last_line, OP_INVOKE, 1);
                    lit_emitter_emitshort(emitter, emitter->last_line,
                               lit_emitter_addconstant(emitter, emitter->last_line, OBJECT_CONST_STRING(emitter->state, "iterator")));
                    lit_emitter_emitbyteorshort(emitter, emitter->last_line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, iterator);
                    // If iter is null, just get out of the loop
                    exit_jump = lit_emitter_emitjump(emitter, OP_JUMP_IF_NULL_POPPING, emitter->last_line);
                    lit_emitter_beginscope(emitter);
                    // var i = seq.iteratorValue(iter)
                    var = (LitAssignVarExpr*)forstmt->var;
                    localcnt = lit_emitter_addlocal(emitter, var->name, var->length, expr->line, false);
                    lit_emitter_marklocalinit(emitter, localcnt);
                    lit_emitter_emitbyteorshort(emitter, emitter->last_line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, sequence);
                    lit_emitter_emitbyteorshort(emitter, emitter->last_line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, iterator);
                    lit_emitter_emitvaryingop(emitter, emitter->last_line, OP_INVOKE, 1);
                    lit_emitter_emitshort(emitter, emitter->last_line,
                               lit_emitter_addconstant(emitter, emitter->last_line, OBJECT_CONST_STRING(emitter->state, "iteratorValue")));
                    lit_emitter_emitbyteorshort(emitter, emitter->last_line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, localcnt);
                    if(forstmt->body != NULL)
                    {
                        if(forstmt->body->type == LITEXPR_BLOCK)
                        {
                            statements = &((LitBlockExpr*)forstmt->body)->statements;
                            for(i = 0; i < statements->count; i++)
                            {
                                lit_emitter_emitexpression(emitter, statements->values[i]);
                            }
                        }
                        else
                        {
                            lit_emitter_emitexpression(emitter, forstmt->body);
                        }
                    }
                    lit_emitter_patchloopjumps(emitter, &emitter->continues, emitter->last_line);
                    lit_emitter_endscope(emitter, emitter->last_line);
                    lit_emitter_emitloop(emitter, start, emitter->last_line);
                    lit_emitter_patchjump(emitter, exit_jump, emitter->last_line);
                }
                lit_emitter_patchloopjumps(emitter, &emitter->breaks, emitter->last_line);
                lit_emitter_endscope(emitter, emitter->last_line);
                emitter->compiler->loop_depth--;
            }
            break;

        case LITEXPR_CONTINUE:
        {
            if(emitter->compiler->loop_depth == 0)
            {
                lit_emitter_raiseerror(emitter, expr->line, LITERROR_LOOP_JUMP_MISSUSE, "continue");
            }
            lit_uintlist_push(emitter->state, &emitter->continues, lit_emitter_emitjump(emitter, OP_JUMP, expr->line));
            break;
        }

        case LITEXPR_BREAK:
            {
                if(emitter->compiler->loop_depth == 0)
                {
                    lit_emitter_raiseerror(emitter, expr->line, LITERROR_LOOP_JUMP_MISSUSE, "break");
                }
                lit_emitter_emit1op(emitter, expr->line, OP_POP_LOCALS);
                depth = emitter->compiler->scope_depth;
                local_count = 0;
                locals = &emitter->compiler->locals;
                for(ii = locals->count - 1; ii >= 0; ii--)
                {
                    local = &locals->values[ii];
                    if(local->depth < depth)
                    {
                        break;
                    }

                    if(!local->captured)
                    {
                        local_count++;
                    }
                }
                lit_emitter_emitshort(emitter, expr->line, local_count);
                lit_uintlist_push(emitter->state, &emitter->breaks, lit_emitter_emitjump(emitter, OP_JUMP, expr->line));
            }
            break;
        case LITEXPR_FUNCTION:
            {
                funcstmt = (LitFunctionExpr*)expr;
                isexport = funcstmt->exported;
                isprivate = !isexport && emitter->compiler->enclosing == NULL && emitter->compiler->scope_depth == 0;
                islocal = !(isexport || isprivate);
                index = 0;
                if(!isexport)
                {
                    index = isprivate ? lit_emitter_resolveprivate(emitter, funcstmt->name, funcstmt->length, expr->line) :
                                      lit_emitter_addlocal(emitter, funcstmt->name, funcstmt->length, expr->line, false);
                }
                name = lit_string_copy(emitter->state, funcstmt->name, funcstmt->length);
                if(islocal)
                {
                    lit_emitter_marklocalinit(emitter, index);
                }
                else if(isprivate)
                {
                    lit_emitter_markprivateinit(emitter, index);
                }
                lit_compiler_compiler(emitter, &compiler, LITFUNC_REGULAR);
                lit_emitter_beginscope(emitter);
                vararg = lit_emitter_emitparamlist(emitter, &funcstmt->parameters, expr->line);
                lit_emitter_emitexpression(emitter, funcstmt->body);
                lit_emitter_endscope(emitter, emitter->last_line);
                function = lit_compiler_end(emitter, name);
                function->arg_count = funcstmt->parameters.count;
                function->max_slots += function->arg_count;
                function->vararg = vararg;
                if(function->upvalue_count > 0)
                {
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_CLOSURE);
                    lit_emitter_emitshort(emitter, emitter->last_line, lit_emitter_addconstant(emitter, emitter->last_line, lit_value_objectvalue(function)));
                    for(i = 0; i < function->upvalue_count; i++)
                    {
                        lit_emitter_emit2bytes(emitter, emitter->last_line, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
                    }
                }
                else
                {
                    lit_emitter_emitconstant(emitter, emitter->last_line, lit_value_objectvalue(function));
                }
                if(isexport)
                {
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_SET_GLOBAL);
                    lit_emitter_emitshort(emitter, emitter->last_line, lit_emitter_addconstant(emitter, emitter->last_line, lit_value_objectvalue(name)));
                }
                else if(isprivate)
                {
                    lit_emitter_emitbyteorshort(emitter, emitter->last_line, OP_SET_PRIVATE, OP_SET_PRIVATE_LONG, index);
                }
                else
                {
                    lit_emitter_emitbyteorshort(emitter, emitter->last_line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, index);
                }
                lit_emitter_emit1op(emitter, emitter->last_line, OP_POP);
            }
            break;
        case LITEXPR_RETURN:
            {
                if(emitter->compiler->type == LITFUNC_CONSTRUCTOR)
                {
                    lit_emitter_raiseerror(emitter, expr->line, LITERROR_RETURN_FROM_CONSTRUCTOR);
                }
                expression = ((LitReturnExpr*)expr)->expression;
                if(expression == NULL)
                {
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_NULL);
                }
                else
                {
                    lit_emitter_emitexpression(emitter, expression);
                }
                lit_emitter_emit1op(emitter, emitter->last_line, OP_RETURN);
                if(emitter->compiler->scope_depth == 0)
                {
                    emitter->compiler->skip_return = true;
                }
                return true;
            }
            break;
        case LITEXPR_METHOD:
            {
                mthstmt = (LitMethodExpr*)expr;
                constructor = lit_string_getlength(mthstmt->name) == 11 && memcmp(mthstmt->name->chars, "constructor", 11) == 0;
                if(constructor && mthstmt->is_static)
                {
                    lit_emitter_raiseerror(emitter, expr->line, LITERROR_STATIC_CONSTRUCTOR);
                }
                lit_compiler_compiler(emitter, &compiler,
                              constructor ? LITFUNC_CONSTRUCTOR : (mthstmt->is_static ? LITFUNC_STATIC_METHOD : LITFUNC_METHOD));
                lit_emitter_beginscope(emitter);
                vararg = lit_emitter_emitparamlist(emitter, &mthstmt->parameters, expr->line);
                lit_emitter_emitexpression(emitter, mthstmt->body);
                lit_emitter_endscope(emitter, emitter->last_line);
                function = lit_compiler_end(emitter, lit_value_asstring(lit_string_format(emitter->state, "@:@", lit_value_objectvalue(emitter->class_name), lit_value_objectvalue(mthstmt->name))));
                function->arg_count = mthstmt->parameters.count;
                function->max_slots += function->arg_count;
                function->vararg = vararg;
                if(function->upvalue_count > 0)
                {
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_CLOSURE);
                    lit_emitter_emitshort(emitter, emitter->last_line, lit_emitter_addconstant(emitter, emitter->last_line, lit_value_objectvalue(function)));
                    for(i = 0; i < function->upvalue_count; i++)
                    {
                        lit_emitter_emit2bytes(emitter, emitter->last_line, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
                    }
                }
                else
                {
                    lit_emitter_emitconstant(emitter, emitter->last_line, lit_value_objectvalue(function));
                }
                lit_emitter_emit1op(emitter, emitter->last_line, mthstmt->is_static ? OP_STATIC_FIELD : OP_METHOD);
                lit_emitter_emitshort(emitter, emitter->last_line, lit_emitter_addconstant(emitter, expr->line, lit_value_objectvalue(mthstmt->name)));

            }
            break;
        case LITEXPR_CLASS:
            {
                clstmt = (LitClassExpr*)expr;
                emitter->class_name = clstmt->name;
                if(clstmt->parent != NULL)
                {
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_GET_GLOBAL);
                    lit_emitter_emitshort(emitter, emitter->last_line, lit_emitter_addconstant(emitter, emitter->last_line, lit_value_objectvalue(clstmt->parent)));
                }
                lit_emitter_emit1op(emitter, expr->line, OP_CLASS);
                lit_emitter_emitshort(emitter, emitter->last_line, lit_emitter_addconstant(emitter, emitter->last_line, lit_value_objectvalue(clstmt->name)));
                if(clstmt->parent != NULL)
                {
                    lit_emitter_emit1op(emitter, emitter->last_line, OP_INHERIT);
                    emitter->class_has_super = true;
                    lit_emitter_beginscope(emitter);
                    super = lit_emitter_addlocal(emitter, "super", 5, emitter->last_line, false);
                    
                    lit_emitter_marklocalinit(emitter, super);
                }
                for(i = 0; i < clstmt->fields.count; i++)
                {
                    s = clstmt->fields.values[i];
                    if(s->type == LITEXPR_VARSTMT)
                    {
                        var = (LitAssignVarExpr*)s;
                        lit_emitter_emitexpression(emitter, var->init);
                        lit_emitter_emit1op(emitter, expr->line, OP_STATIC_FIELD);
                        lit_emitter_emitshort(emitter, expr->line,
                                   lit_emitter_addconstant(emitter, expr->line,
                                                lit_value_objectvalue(lit_string_copy(emitter->state, var->name, var->length))));
                    }
                    else
                    {
                        lit_emitter_emitexpression(emitter, s);
                    }
                }
                lit_emitter_emit1op(emitter, emitter->last_line, OP_POP);
                if(clstmt->parent != NULL)
                {
                    lit_emitter_endscope(emitter, emitter->last_line);
                }
                emitter->class_name = NULL;
                emitter->class_has_super = false;
            }
            break;
        case LITEXPR_FIELD:
            {
                fieldstmt = (LitFieldExpr*)expr;
                getter = NULL;
                setter = NULL;
                if(fieldstmt->getter != NULL)
                {
                    lit_compiler_compiler(emitter, &compiler, fieldstmt->is_static ? LITFUNC_STATIC_METHOD : LITFUNC_METHOD);
                    lit_emitter_beginscope(emitter);
                    lit_emitter_emitexpression(emitter, fieldstmt->getter);
                    lit_emitter_endscope(emitter, emitter->last_line);
                    getter = lit_compiler_end(emitter,
                        lit_value_asstring(lit_string_format(emitter->state, "@:get @", lit_value_objectvalue(emitter->class_name), fieldstmt->name)));
                }
                if(fieldstmt->setter != NULL)
                {
                    lit_compiler_compiler(emitter, &compiler, fieldstmt->is_static ? LITFUNC_STATIC_METHOD : LITFUNC_METHOD);
                    lit_emitter_marklocalinit(emitter, lit_emitter_addlocal(emitter, "value", 5, expr->line, false));
                    lit_emitter_beginscope(emitter);
                    lit_emitter_emitexpression(emitter, fieldstmt->setter);
                    lit_emitter_endscope(emitter, emitter->last_line);
                    setter = lit_compiler_end(emitter,
                        lit_value_asstring(lit_string_format(emitter->state, "@:set @", lit_value_objectvalue(emitter->class_name), fieldstmt->name)));
                    setter->arg_count = 1;
                    setter->max_slots++;
                }
                field = lit_create_field(emitter->state, (LitObject*)getter, (LitObject*)setter);
                lit_emitter_emitconstant(emitter, expr->line, lit_value_objectvalue(field));
                lit_emitter_emit1op(emitter, expr->line, fieldstmt->is_static ? OP_STATIC_FIELD : OP_DEFINE_FIELD);
                lit_emitter_emitshort(emitter, expr->line, lit_emitter_addconstant(emitter, expr->line, lit_value_objectvalue(fieldstmt->name)));
            }
            break;
        default:
            {
                lit_emitter_raiseerror(emitter, expr->line, LITERROR_UNKNOWN_EXPRESSION, (int)expr->type);
            }
            break;
    }
    emitter->previous_was_expression_statement = expr->type == LITEXPR_EXPRESSION;
    return false;
}

LitModule* lit_emitter_modemit(LitEmitter* emitter, LitExprList* statements, LitString* module_name)
{
    size_t i;
    size_t total;
    size_t old_privates_count;
    bool isnew;
    LitState* state;        
    LitValue module_value;
    LitModule* module;
    LitPrivList* privates;
    LitCompiler compiler;
    LitExpression* exstmt;
    emitter->last_line = 1;
    emitter->emit_reference = 0;
    state = emitter->state;
    isnew = false;
    if(lit_table_get(&emitter->state->vm->modules->values, module_name, &module_value))
    {
        module = lit_value_asmodule(module_value);
    }
    else
    {
        module = lit_create_module(emitter->state, module_name);
        isnew = true;
    }
    emitter->module = module;
    old_privates_count = module->private_count;
    if(old_privates_count > 0)
    {
        privates = &emitter->privates;
        privates->count = old_privates_count - 1;
        lit_privlist_push(state, privates, (LitPrivate){ true, false });
        for(i = 0; i < old_privates_count; i++)
        {
            privates->values[i].initialized = true;
        }
    }
    lit_compiler_compiler(emitter, &compiler, LITFUNC_SCRIPT);
    emitter->chunk = &compiler.function->chunk;
    resolve_statements(emitter, statements);
    for(i = 0; i < statements->count; i++)
    {
        exstmt = statements->values[i];
        if(lit_emitter_emitexpression(emitter, exstmt))
        {
            break;
        }
    }
    lit_emitter_endscope(emitter, emitter->last_line);
    module->main_function = lit_compiler_end(emitter, module_name);
    if(isnew)
    {
        total = emitter->privates.count;
        module->privates = LIT_ALLOCATE(emitter->state, sizeof(LitValue), total);
        for(i = 0; i < total; i++)
        {
            module->privates[i] = NULL_VALUE;
        }
    }
    else
    {
        module->privates = LIT_GROW_ARRAY(emitter->state, module->privates, sizeof(LitValue), old_privates_count, module->private_count);
        for(i = old_privates_count; i < module->private_count; i++)
        {
            module->privates[i] = NULL_VALUE;
        }
    }
    lit_privlist_destroy(emitter->state, &emitter->privates);
    if(lit_astopt_isoptenabled(LITOPTSTATE_PRIVATE_NAMES))
    {
        lit_table_destroy(emitter->state, &emitter->module->private_names->values);
    }
    if(isnew && !state->had_error)
    {
        lit_table_set(state, &state->vm->modules->values, module_name, lit_value_objectvalue(module));
    }
    module->ran = true;
    return module;
}
