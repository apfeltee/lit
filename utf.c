
#include <wchar.h>
#include "lit.h"

int lit_decode_num_bytes(uint8_t byte)
{
    if((byte & 0xc0) == 0x80)
    {
        return 0;
    }
    if((byte & 0xf8) == 0xf0)
    {
        return 4;
    }
    if((byte & 0xf0) == 0xe0)
    {
        return 3;
    }
    if((byte & 0xe0) == 0xc0)
    {
        return 2;
    }
    return 1;
}

int lit_ustring_length(LitString* string)
{
    int length;
    uint32_t i;
    length = 0;
    for(i = 0; i < lit_string_length(string);)
    {
        i += lit_decode_num_bytes(string->chars[i]);
        length++;
    }
    return length;
}

LitString* lit_ustring_code_point_at(LitState* state, LitString* string, uint32_t index)
{
    char bytes[2];
    int code_point;
    if(index >= lit_string_length(string))
    {
        return NULL;
    }
    code_point = lit_ustring_decode((uint8_t*)string->chars + index, lit_string_length(string) - index);
    if(code_point == -1)
    {
        bytes[0] = string->chars[index];
        bytes[1] = '\0';
        return lit_string_copy(state, bytes, 1);
    }
    return lit_ustring_from_code_point(state, code_point);
}

LitString* lit_ustring_from_code_point(LitState* state, int value)
{
    int length;
    char* bytes;
    LitString* rt;
    length = lit_encode_num_bytes(value);
    bytes = LIT_ALLOCATE(state, char, length + 1);
    lit_ustring_encode(value, (uint8_t*)bytes);
    /* this should be lit_string_take, but something prevents the memory from being free'd. */
    rt = lit_string_copy(state, bytes, length);
    LIT_FREE(state, char, bytes);
    return rt;
}

LitString* lit_ustring_from_range(LitState* state, LitString* source, int start, uint32_t count)
{
    int length;
    int index;
    int code_point;
    uint32_t i;
    uint8_t* to;
    uint8_t* from;
    char* bytes;
    from = (uint8_t*)source->chars;
    length = 0;
    for(i = 0; i < count; i++)
    {
        length += lit_decode_num_bytes(from[start + i]);
    }
    bytes = (char*)malloc(length + 1);
    to = (uint8_t*)bytes;
    for(i = 0; i < count; i++)
    {
        index = start + i;
        code_point = lit_ustring_decode(from + index, lit_string_length(source) - index);
        if(code_point != -1)
        {
            to += lit_ustring_encode(code_point, to);
        }
    }
    return lit_string_take(state, bytes, length, false);
}

int lit_encode_num_bytes(int value)
{
    if(value <= 0x7f)
    {
        return 1;
    }
    if(value <= 0x7ff)
    {
        return 2;
    }
    if(value <= 0xffff)
    {
        return 3;
    }
    if(value <= 0x10ffff)
    {
        return 4;
    }
    return 0;
}

int lit_ustring_encode(int value, uint8_t* bytes)
{
    if(value <= 0x7f)
    {
        *bytes = value & 0x7f;
        return 1;
    }
    else if(value <= 0x7ff)
    {
        *bytes = 0xc0 | ((value & 0x7c0) >> 6);
        bytes++;
        *bytes = 0x80 | (value & 0x3f);
        return 2;
    }
    else if(value <= 0xffff)
    {
        *bytes = 0xe0 | ((value & 0xf000) >> 12);
        bytes++;
        *bytes = 0x80 | ((value & 0xfc0) >> 6);
        bytes++;
        *bytes = 0x80 | (value & 0x3f);
        return 3;
    }
    else if(value <= 0x10ffff)
    {
        *bytes = 0xf0 | ((value & 0x1c0000) >> 18);
        bytes++;
        *bytes = 0x80 | ((value & 0x3f000) >> 12);
        bytes++;
        *bytes = 0x80 | ((value & 0xfc0) >> 6);
        bytes++;
        *bytes = 0x80 | (value & 0x3f);
        return 4;
    }
    UNREACHABLE
    return 0;
}

int lit_ustring_decode(const uint8_t* bytes, uint32_t length)
{
    int value;
    uint32_t remaining_bytes;
    if(*bytes <= 0x7f)
    {
        return *bytes;
    }
    if((*bytes & 0xe0) == 0xc0)
    {
        value = *bytes & 0x1f;
        remaining_bytes = 1;
    }
    else if((*bytes & 0xf0) == 0xe0)
    {
        value = *bytes & 0x0f;
        remaining_bytes = 2;
    }
    else if((*bytes & 0xf8) == 0xf0)
    {
        value = *bytes & 0x07;
        remaining_bytes = 3;
    }
    else
    {
        return -1;
    }
    if(remaining_bytes > length - 1)
    {
        return -1;
    }
    while(remaining_bytes > 0)
    {
        bytes++;
        remaining_bytes--;
        if((*bytes & 0xc0) != 0x80)
        {
            return -1;
        }
        value = value << 6 | (*bytes & 0x3f);
    }
    return value;
}

int lit_uchar_offset(char* str, int index)
{
#define is_utf(c) (((c)&0xC0) != 0x80)
    int offset;
    offset = 0;
    while(index > 0 && str[offset])
    {
        (void)(is_utf(str[++offset]) || is_utf(str[++offset]) || is_utf(str[++offset]) || ++offset);
        index--;
    }
    return offset;
#undef is_utf
}