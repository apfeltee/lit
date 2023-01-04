
#include "lit.h"

void lit_chunk_init(LitChunk* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;

    chunk->has_line_info = true;
    chunk->line_count = 0;
    chunk->line_capacity = 0;
    chunk->lines = NULL;

    lit_vallist_init(&chunk->constants);
}

void lit_chunk_destroy(LitState* state, LitChunk* chunk)
{
    LIT_FREE_ARRAY(state, sizeof(uint8_t), chunk->code, chunk->capacity);
    LIT_FREE_ARRAY(state, sizeof(uint16_t), chunk->lines, chunk->line_capacity);

    lit_vallist_destroy(state, &chunk->constants);
    lit_chunk_init(chunk);
}

void lit_chunk_push(LitState* state, LitChunk* chunk, uint8_t byte, uint16_t line)
{
    if(chunk->capacity < chunk->count + 1)
    {
        size_t old_capacity = chunk->capacity;

        chunk->capacity = LIT_GROW_CAPACITY(old_capacity + 2);
        chunk->code = LIT_GROW_ARRAY(state, chunk->code, sizeof(uint8_t), old_capacity, chunk->capacity + 2);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

    if(!chunk->has_line_info)
    {
        return;
    }

    if(chunk->line_capacity < chunk->line_count + 2)
    {
        size_t old_capacity = chunk->line_capacity;

        chunk->line_capacity = LIT_GROW_CAPACITY(chunk->line_capacity + 2);
        chunk->lines = LIT_GROW_ARRAY(state, chunk->lines, sizeof(uint16_t), old_capacity, chunk->line_capacity + 2);

        if(old_capacity == 0)
        {
            chunk->lines[0] = 0;
            chunk->lines[1] = 0;
        }
    }

    size_t line_index = chunk->line_count;
    size_t value = chunk->lines[line_index];

    if(value != 0 && value != line)
    {
        chunk->line_count += 2;
        line_index = chunk->line_count;

        chunk->lines[line_index + 1] = 0;
    }

    chunk->lines[line_index] = line;
    chunk->lines[line_index + 1]++;
}

size_t lit_chunk_addconst(LitState* state, LitChunk* chunk, LitValue constant)
{
    LitState** cst;
    cst = &state;
    for(size_t i = 0; i < lit_vallist_count(&chunk->constants); i++)
    {
        if(lit_vallist_get(&chunk->constants, i) == constant)
        {
            return i;
        }
    }

    lit_state_pushvalueroot(state, constant);
    lit_vallist_push(*cst, &chunk->constants, constant);
    lit_state_poproot(state);

    return lit_vallist_count(&chunk->constants) - 1;
}

size_t lit_chunk_getline(LitChunk* chunk, size_t offset)
{
    if(!chunk->has_line_info)
    {
        return 0;
    }

    size_t rle = 0;
    size_t line = 0;
    size_t index = 0;

    for(size_t i = 0; i < offset + 1; i++)
    {
        if(rle > 0)
        {
            rle--;
            continue;
        }

        line = chunk->lines[index];
        rle = chunk->lines[index + 1];

        if(rle > 0)
        {
            rle--;
        }

        index += 2;
    }

    return line;
}

void lit_chunk_shrink(LitState* state, LitChunk* chunk)
{
    if(chunk->capacity > chunk->count)
    {
        size_t old_capacity = chunk->capacity;

        chunk->capacity = chunk->count;
        chunk->code = LIT_GROW_ARRAY(state, chunk->code, sizeof(uint8_t), old_capacity, chunk->capacity);
    }

    if(chunk->line_capacity > chunk->line_count)
    {
        size_t old_capacity = chunk->line_capacity;

        chunk->line_capacity = chunk->line_count + 2;
        chunk->lines = LIT_GROW_ARRAY(state, chunk->lines, sizeof(uint16_t), old_capacity, chunk->line_capacity);
    }
}

void lit_chunk_emitbyte(LitState* state, LitChunk* chunk, uint8_t byte)
{
    lit_chunk_push(state, chunk, byte, 1);
}

void lit_chunk_emit2bytes(LitState* state, LitChunk* chunk, uint8_t a, uint8_t b)
{
    lit_chunk_push(state, chunk, a, 1);
    lit_chunk_push(state, chunk, b, 1);
}

void lit_chunk_emitshort(LitState* state, LitChunk* chunk, uint16_t value)
{
    lit_chunk_emit2bytes(state, chunk, (uint8_t)((value >> 8) & 0xff), (uint8_t)(value & 0xff));
}