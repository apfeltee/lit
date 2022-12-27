
#include <stdio.h>
#include "lit.h"

void lit_disassemble_module(LitState* state, LitModule* module, const char* source)
{
    lit_disassemble_chunk(state, &module->main_function->chunk, module->main_function->name->chars, source);
}

void lit_disassemble_chunk(LitState* state, LitChunk* chunk, const char* name, const char* source)
{
    size_t i;
    size_t offset;
    LitValue value;
    LitValueList* values;
    LitFunction* function;
    values = &chunk->constants;

    for(i = 0; i < values->count; i++)
    {
        value = values->values[i];
        if(IS_FUNCTION(value))
        {
            function = AS_FUNCTION(value);
            lit_disassemble_chunk(state, &function->chunk, function->name->chars, source);
        }
    }
    lit_writer_writeformat(&state->debugwriter, "== %s ==\n", name);
    for(offset = 0; offset < chunk->count;)
    {
        offset = lit_disassemble_instruction(state, chunk, offset, source);
    }
}

static size_t print_simple_op(LitState* state, LitWriter* wr, const char* name, size_t offset)
{
    (void)state;
    lit_writer_writeformat(wr, "%s%s%s\n", COLOR_YELLOW, name, COLOR_RESET);
    return offset + 1;
}

static size_t print_constant_op(LitState* state, LitWriter* wr, const char* name, LitChunk* chunk, size_t offset, bool big)
{
    uint8_t constant;
    if(big)
    {
        constant = (uint16_t)(chunk->code[offset + 1] << 8);
        constant |= chunk->code[offset + 2];
    }
    else
    {
        constant = chunk->code[offset + 1];
    }
    lit_writer_writeformat(wr, "%s%-16s%s %4d '", COLOR_YELLOW, name, COLOR_RESET, constant);
    lit_print_value(state, wr, chunk->constants.values[constant]);
    lit_writer_writeformat(wr, "'\n");
    return offset + (big ? 3 : 2);
}

static size_t print_byte_op(LitState* state, LitWriter* wr, const char* name, LitChunk* chunk, size_t offset)
{
    uint8_t slot;
    (void)state;
    slot = chunk->code[offset + 1];
    lit_writer_writeformat(wr, "%s%-16s%s %4d\n", COLOR_YELLOW, name, COLOR_RESET, slot);
    return offset + 2;
}

static size_t print_short_op(LitState* state, LitWriter* wr, const char* name, LitChunk* chunk, size_t offset)
{
    uint16_t slot;
    (void)state;
    slot = (uint16_t)(chunk->code[offset + 1] << 8);
    slot |= chunk->code[offset + 2];
    lit_writer_writeformat(wr, "%s%-16s%s %4d\n", COLOR_YELLOW, name, COLOR_RESET, slot);
    return offset + 2;
}

static size_t print_jump_op(LitState* state, LitWriter* wr, const char* name, int sign, LitChunk* chunk, size_t offset)
{
    uint16_t jump;
    (void)state;
    jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    lit_writer_writeformat(wr, "%s%-16s%s %4d -> %d\n", COLOR_YELLOW, name, COLOR_RESET, (int)offset, (int)(offset + 3 + sign * jump));
    return offset + 3;
}

static size_t print_invoke_op(LitState* state, LitWriter* wr, const char* name, LitChunk* chunk, size_t offset)
{
    uint8_t arg_count;
    uint8_t constant;
    (void)state;
    arg_count = chunk->code[offset + 1];
    constant = chunk->code[offset + 2];
    constant |= chunk->code[offset + 3];
    lit_writer_writeformat(wr, "%s%-16s%s (%d args) %4d '", COLOR_YELLOW, name, COLOR_RESET, arg_count, constant);
    lit_print_value(state, wr, chunk->constants.values[constant]);
    lit_writer_writeformat(wr, "'\n");
    return offset + 4;
}

size_t lit_disassemble_instruction(LitState* state, LitChunk* chunk, size_t offset, const char* source)
{
    bool same;
    int is_local;
    size_t j;
    size_t line;
    size_t index;
    int16_t constant;
    uint8_t instruction;
    char c;
    char* next_line;
    char* prev_line;
    char* output_line;
    char* current_line;
    LitWriter* wr;
    LitFunction* function;
    wr = &state->debugwriter;
    line = lit_chunk_get_line(chunk, offset);
    same = !chunk->has_line_info || (offset > 0 && line == lit_chunk_get_line(chunk, offset - 1));
    if(!same && source != NULL)
    {
        index = 0;
        current_line = (char*)source;
        while(current_line)
        {
            next_line = strchr(current_line, '\n');
            prev_line = current_line;
            index++;
            current_line = next_line ? (next_line + 1) : NULL;
            if(index == line)
            {
                output_line = prev_line ? prev_line : next_line;
                while((c = *output_line) && (c == '\t' || c == ' '))
                {
                    output_line++;
                }
                lit_writer_writeformat(wr, "%s        %.*s%s\n", COLOR_RED, next_line ? (int)(next_line - output_line) : (int)strlen(prev_line), output_line, COLOR_RESET);
                break;
            }
        }
    }
    lit_writer_writeformat(wr, "%04d ", (int)offset);
    if(same)
    {
        lit_writer_writestring(wr, "   | ");
    }
    else
    {
        lit_writer_writeformat(wr, "%s%4d%s ", COLOR_BLUE, (int)line, COLOR_RESET);
    }
    instruction = chunk->code[offset];
    switch(instruction)
    {
        case OP_POP:
            return print_simple_op(state, wr, "OP_POP", offset);
        case OP_POP_LOCALS:
            return print_constant_op(state, wr, "OP_POP_LOCALS", chunk, offset, true);
        case OP_RETURN:
            return print_simple_op(state, wr, "OP_RETURN", offset);
        case OP_CONSTANT:
            return print_constant_op(state, wr, "OP_CONSTANT", chunk, offset, false);
        case OP_CONSTANT_LONG:
            return print_constant_op(state, wr, "OP_CONSTANT_LONG", chunk, offset, true);
        case OP_TRUE:
            return print_simple_op(state, wr, "OP_TRUE", offset);
        case OP_FALSE:
            return print_simple_op(state, wr, "OP_FALSE", offset);
        case OP_NULL:
            return print_simple_op(state, wr, "OP_NULL", offset);
        case OP_NEGATE:
            return print_simple_op(state, wr, "OP_NEGATE", offset);
        case OP_NOT:
            return print_simple_op(state, wr, "OP_NOT", offset);
        case OP_ADD:
            return print_simple_op(state, wr, "OP_ADD", offset);
        case OP_SUBTRACT:
            return print_simple_op(state, wr, "OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return print_simple_op(state, wr, "OP_MULTIPLY", offset);
        case OP_POWER:
            return print_simple_op(state, wr, "OP_POWER", offset);
        case OP_DIVIDE:
            return print_simple_op(state, wr, "OP_DIVIDE", offset);
        case OP_FLOOR_DIVIDE:
            return print_simple_op(state, wr, "OP_FLOOR_DIVIDE", offset);
        case OP_MOD:
            return print_simple_op(state, wr, "OP_MOD", offset);
        case OP_BAND:
            return print_simple_op(state, wr, "OP_BAND", offset);
        case OP_BOR:
            return print_simple_op(state, wr, "OP_BOR", offset);
        case OP_BXOR:
            return print_simple_op(state, wr, "OP_BXOR", offset);
        case OP_LSHIFT:
            return print_simple_op(state, wr, "OP_LSHIFT", offset);
        case OP_RSHIFT:
            return print_simple_op(state, wr, "OP_RSHIFT", offset);
        case OP_BNOT:
            return print_simple_op(state, wr, "OP_BNOT", offset);
        case OP_EQUAL:
            return print_simple_op(state, wr, "OP_EQUAL", offset);
        case OP_GREATER:
            return print_simple_op(state, wr, "OP_GREATER", offset);
        case OP_GREATER_EQUAL:
            return print_simple_op(state, wr, "OP_GREATER_EQUAL", offset);
        case OP_LESS:
            return print_simple_op(state, wr, "OP_LESS", offset);
        case OP_LESS_EQUAL:
            return print_simple_op(state, wr, "OP_LESS_EQUAL", offset);
        case OP_SET_GLOBAL:
            return print_constant_op(state, wr, "OP_SET_GLOBAL", chunk, offset, true);
        case OP_GET_GLOBAL:
            return print_constant_op(state, wr, "OP_GET_GLOBAL", chunk, offset, true);
        case OP_SET_LOCAL:
            return print_byte_op(state, wr, "OP_SET_LOCAL", chunk, offset);
        case OP_GET_LOCAL:
            return print_byte_op(state, wr, "OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL_LONG:
            return print_short_op(state, wr, "OP_SET_LOCAL_LONG", chunk, offset);
        case OP_GET_LOCAL_LONG:
            return print_short_op(state, wr, "OP_GET_LOCAL_LONG", chunk, offset);
        case OP_SET_PRIVATE:
            return print_byte_op(state, wr, "OP_SET_PRIVATE", chunk, offset);
        case OP_GET_PRIVATE:
            return print_byte_op(state, wr, "OP_GET_PRIVATE", chunk, offset);
        case OP_SET_PRIVATE_LONG:
            return print_short_op(state, wr, "OP_SET_PRIVATE_LONG", chunk, offset);
        case OP_GET_PRIVATE_LONG:
            return print_short_op(state, wr, "OP_GET_PRIVATE_LONG", chunk, offset);
        case OP_SET_UPVALUE:
            return print_byte_op(state, wr, "OP_SET_UPVALUE", chunk, offset);
        case OP_GET_UPVALUE:
            return print_byte_op(state, wr, "OP_GET_UPVALUE", chunk, offset);
        case OP_JUMP_IF_FALSE:
            return print_jump_op(state, wr, "OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_JUMP_IF_NULL:
            return print_jump_op(state, wr, "OP_JUMP_IF_NULL", 1, chunk, offset);
        case OP_JUMP_IF_NULL_POPPING:
            return print_jump_op(state, wr, "OP_JUMP_IF_NULL_POPPING", 1, chunk, offset);
        case OP_JUMP:
            return print_jump_op(state, wr, "OP_JUMP", 1, chunk, offset);
        case OP_JUMP_BACK:
            return print_jump_op(state, wr, "OP_JUMP_BACK", -1, chunk, offset);
        case OP_AND:
            return print_jump_op(state, wr, "OP_AND", 1, chunk, offset);
        case OP_OR:
            return print_jump_op(state, wr, "OP_OR", 1, chunk, offset);
        case OP_NULL_OR:
            return print_jump_op(state, wr, "OP_NULL_OR", 1, chunk, offset);
        case OP_CALL:
            return print_byte_op(state, wr, "OP_CALL", chunk, offset);
        case OP_CLOSURE:
            {
                offset++;
                constant = (uint16_t)(chunk->code[offset] << 8);
                offset++;
                constant |= chunk->code[offset];
                lit_writer_writeformat(wr, "%-16s %4d ", "OP_CLOSURE", constant);
                lit_print_value(state, wr, chunk->constants.values[constant]);
                lit_writer_writeformat(wr, "\n");
                function = AS_FUNCTION(chunk->constants.values[constant]);
                for(j = 0; j < function->upvalue_count; j++)
                {
                    is_local = chunk->code[offset++];
                    index = chunk->code[offset++];
                    lit_writer_writeformat(wr, "%04d      |                     %s %d\n", (int)(offset - 2), is_local ? "local" : "upvalue", (int)index);
                }
                return offset;
            }
            break;
        case OP_CLOSE_UPVALUE:
            return print_simple_op(state, wr, "OP_CLOSE_UPVALUE", offset);
        case OP_CLASS:
            return print_constant_op(state, wr, "OP_CLASS", chunk, offset, true);

        case OP_GET_FIELD:
            return print_simple_op(state, wr, "OP_GET_FIELD", offset);
        case OP_SET_FIELD:
            return print_simple_op(state, wr, "OP_SET_FIELD", offset);

        case OP_SUBSCRIPT_GET:
            return print_simple_op(state, wr, "OP_SUBSCRIPT_GET", offset);
        case OP_SUBSCRIPT_SET:
            return print_simple_op(state, wr, "OP_SUBSCRIPT_SET", offset);
        case OP_ARRAY:
            return print_simple_op(state, wr, "OP_ARRAY", offset);
        case OP_PUSH_ARRAY_ELEMENT:
            return print_simple_op(state, wr, "OP_PUSH_ARRAY_ELEMENT", offset);
        case OP_OBJECT:
            return print_simple_op(state, wr, "OP_OBJECT", offset);
        case OP_PUSH_OBJECT_FIELD:
            return print_simple_op(state, wr, "OP_PUSH_OBJECT_FIELD", offset);
        case OP_RANGE:
            return print_simple_op(state, wr, "OP_RANGE", offset);
        case OP_METHOD:
            return print_constant_op(state, wr, "OP_METHOD", chunk, offset, true);
        case OP_STATIC_FIELD:
            return print_constant_op(state, wr, "OP_STATIC_FIELD", chunk, offset, true);
        case OP_DEFINE_FIELD:
            return print_constant_op(state, wr, "OP_DEFINE_FIELD", chunk, offset, true);
        case OP_INVOKE:
            return print_invoke_op(state, wr, "OP_INVOKE", chunk, offset);
        case OP_INVOKE_SUPER:
            return print_invoke_op(state, wr, "OP_INVOKE_SUPER", chunk, offset);
        case OP_INVOKE_IGNORING:
            return print_invoke_op(state, wr, "OP_INVOKE_IGNORING", chunk, offset);
        case OP_INVOKE_SUPER_IGNORING:
            return print_invoke_op(state, wr, "OP_INVOKE_SUPER_IGNORING", chunk, offset);
        case OP_INHERIT:
            return print_simple_op(state, wr, "OP_INHERIT", offset);
        case OP_IS:
            return print_simple_op(state, wr, "OP_IS", offset);
        case OP_GET_SUPER_METHOD:
            return print_constant_op(state, wr, "OP_GET_SUPER_METHOD", chunk, offset, true);
        case OP_VARARG:
            return print_byte_op(state, wr, "OP_VARARG", chunk, offset);
        case OP_REFERENCE_FIELD:
            return print_simple_op(state, wr, "OP_REFERENCE_FIELD", offset);
        case OP_REFERENCE_UPVALUE:
            return print_byte_op(state, wr, "OP_REFERENCE_UPVALUE", chunk, offset);
        case OP_REFERENCE_PRIVATE:
            return print_short_op(state, wr, "OP_REFERENCE_PRIVATE", chunk, offset);
        case OP_REFERENCE_LOCAL:
            return print_short_op(state, wr, "OP_REFERENCE_LOCAL", chunk, offset);
        case OP_REFERENCE_GLOBAL:
            return print_constant_op(state, wr, "OP_REFERENCE_GLOBAL", chunk, offset, true);
        case OP_SET_REFERENCE:
            return print_simple_op(state, wr, "OP_SET_REFERENCE", offset);
        default:
            {
                lit_writer_writeformat(wr, "Unknown opcode %d\n", instruction);
                return offset + 1;
            }
            break;
    }
}

void lit_trace_frame(LitFiber* fiber, LitWriter* wr)
{
    LitCallFrame* frame;
    (void)fiber;
    (void)frame;
    (void)wr;
#ifdef LIT_TRACE_STACK
    if(fiber == NULL)
    {
        return;
    }
    frame = &fiber->frames[fiber->frame_count - 1];
    lit_writer_writeformat(wr, "== fiber %p f%i %s (expects %i, max %i, added %i, current %i, exits %i) ==\n", fiber,
           fiber->frame_count - 1, frame->function->name->chars, frame->function->arg_count, frame->function->max_slots,
           frame->function->max_slots + (int)(fiber->stack_top - fiber->stack), fiber->stack_capacity, frame->return_to_c);
#endif
}
