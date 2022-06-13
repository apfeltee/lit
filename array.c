
#include "lit.h"

void lit_init_uints(LitUInts* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}
void lit_free_uints(LitState* state, LitUInts* array)
{
    LIT_FREE_ARRAY(state, size_t, array->values, array->capacity);
    lit_init_uints(array);
}
void lit_uints_write(LitState* state, LitUInts* array, size_t value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, size_t, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}
void lit_init_bytes(LitBytes* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}
void lit_free_bytes(LitState* state, LitBytes* array)
{
    LIT_FREE_ARRAY(state, uint8_t, array->values, array->capacity);
    lit_init_bytes(array);
}
void lit_bytes_write(LitState* state, LitBytes* array, uint8_t value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, uint8_t, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}
