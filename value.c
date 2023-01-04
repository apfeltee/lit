
#include <stdarg.h>
#include <stdio.h>
#include "lit.h"

LitValue lit_value_objectvalue_actual(uintptr_t obj)
{
    return (LitValue)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj));
}

LitObject* lit_value_asobject(LitValue v)
{
    return ((LitObject*)(uintptr_t)((v) & ~(SIGN_BIT | QNAN)));
}

LitValue lit_bool_to_value(LitState* state, bool b) 
{
    (void)state;
    return (b ? TRUE_VALUE : FALSE_VALUE);
}

bool lit_value_isbool(LitValue v)
{
    return ((v & FALSE_VALUE) == FALSE_VALUE);
}

bool lit_value_isobject(LitValue v)
{
    return ((v & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT));
}

LitObjType lit_value_type(LitValue v)
{
    LitObject* o;
    if(lit_value_isobject(v))
    {
        o = lit_value_asobject(v);
        if(o == NULL)
        {
            return LITTYPE_UNDEFINED;
        }
        return o->type;
    }
    return LITTYPE_UNDEFINED;
}

#define USE_NUMBEROBJECT 0

double lit_value_asnumber(LitValue v)
{
    #if defined(USE_NUMBEROBJECT) && (USE_NUMBEROBJECT == 1)
        return ((LitNumber*)lit_value_asobject(v))->num;
    #else
        return *((double*)&v);
    #endif
}

LitValue lit_value_numbertovalue(LitState* state, double num)
{
    (void)state;
    #if defined(USE_NUMBEROBJECT) && (USE_NUMBEROBJECT == 1)
        #if 1
            LitObject* o;
            LitNumber* nobj;
            o = lit_gcmem_allocobject(state, sizeof(LitNumber), LITTYPE_NUMBER, true);
            o->type = LITTYPE_NUMBER;
            nobj = (LitNumber*)o;
            nobj->num = num;
            return lit_value_objectvalue(nobj);
        #else
            LitNumber* n;
            LitObject o;
            n = (LitNumber*)&o;
            o.type = LITTYPE_NUMBER;
            n->object = o;
            n->num = num;
            return lit_value_objectvalue(n);
        #endif
    #else
        return *((LitValue*)&num);
    #endif
}

bool lit_value_isnumber(LitValue v)
{
    #if defined(USE_NUMBEROBJECT) && (USE_NUMBEROBJECT == 1)
        return lit_value_istype(v, LITTYPE_NUMBER);
    #else
        return (((v)&QNAN) != QNAN);
    #endif
}


bool lit_value_compare(LitState* state, const LitValue a, const LitValue b)
{
    LitObjType t1;
    LitObjType t2;
    double n1;
    double n2;
    LitInterpretResult inret;
    LitValue args[3];
    if(lit_value_isinstance(a))
    {
        args[0] = b;
        inret = lit_state_callinstancemethod(state, a, CONST_STRING(state, "=="), args, 1);
        if(inret.type == LITRESULT_OK)
        {
            if(lit_bool_to_value(state, inret.result) == TRUE_VALUE)
            {
                return true;
            }
            return false;
        }
    }
    //t1 = lit_value_type(a);
    //t2 = lit_value_type(b);
    //fprintf(stderr, "compare: t1=%d t2=%d\n", t1, t2);
    //if(t1 == t2)
    {
        return (a == b);
    }
    //return false;
}

LitString* lit_value_tostring(LitState* state, LitValue object)
{
    LitValue* slot;
    LitVM* vm;
    LitFiber* fiber;
    LitFunction* function;
    LitChunk* chunk;
    LitCallFrame* frame;
    LitInterpretResult result;
    if(lit_value_isstring(object))
    {
        return lit_value_asstring(object);
    }
    else if(!lit_value_isobject(object))
    {
        if(lit_value_isnull(object))
        {
            return CONST_STRING(state, "null");
        }
        else if(lit_value_isnumber(object))
        {
            return lit_value_asstring(lit_string_numbertostring(state, lit_value_asnumber(object)));
        }
        else if(lit_value_isbool(object))
        {
            return CONST_STRING(state, lit_value_asbool(object) ? "true" : "false");
        }
    }
    else if(lit_value_isreference(object))
    {
        slot = lit_value_asreference(object)->slot;

        if(slot == NULL)
        {
            return CONST_STRING(state, "null");
        }
        return lit_value_tostring(state, *slot);
    }
    vm = state->vm;
    fiber = vm->fiber;
    if(lit_state_ensurefiber(vm, fiber))
    {
        return CONST_STRING(state, "null");
    }
    function = state->api_function;
    if(function == NULL)
    {
        function = state->api_function = lit_create_function(state, fiber->module);
        function->chunk.has_line_info = false;
        function->name = state->api_name;
        chunk = &function->chunk;
        chunk->count = 0;
        lit_vallist_setcount(&chunk->constants, 0);
        function->max_slots = 3;
        lit_chunk_push(state, chunk, OP_INVOKE, 1);
        lit_chunk_emitbyte(state, chunk, 0);
        lit_chunk_emitshort(state, chunk, lit_chunk_addconst(state, chunk, OBJECT_CONST_STRING(state, "toString")));
        lit_chunk_emitbyte(state, chunk, OP_RETURN);
    }
    lit_ensure_fiber_stack(state, fiber, function->max_slots + (int)(fiber->stack_top - fiber->stack));
    frame = &fiber->frames[fiber->frame_count++];
    frame->ip = function->chunk.code;
    frame->closure = NULL;
    frame->function = function;
    frame->slots = fiber->stack_top;
    frame->result_ignored = false;
    frame->return_to_c = true;
    PUSH(lit_value_objectvalue(function));
    PUSH(object);
    result = lit_vm_execfiber(state, fiber);
    if(result.type != LITRESULT_OK)
    {
        return CONST_STRING(state, "null");
    }
    return lit_value_asstring(result.result);
}


double lit_value_checknumber(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isnumber(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected a number as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }
    return lit_value_asnumber(args[id]);
}

double lit_value_getnumber(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, double def)
{
    (void)vm;
    if(arg_count <= id || !lit_value_isnumber(args[id]))
    {
        return def;
    }
    return lit_value_asnumber(args[id]);
}

bool lit_value_checkbool(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isbool(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected a boolean as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }

    return lit_value_asbool(args[id]);
}

bool lit_value_getbool(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, bool def)
{
    (void)vm;
    if(arg_count <= id || !lit_value_isbool(args[id]))
    {
        return def;
    }
    return lit_value_asbool(args[id]);
}

const char* lit_value_checkstring(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isstring(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected a string as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }

    return lit_value_asstring(args[id])->chars;
}

const char* lit_value_getstring(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, const char* def)
{
    (void)vm;
    if(arg_count <= id || !lit_value_isstring(args[id]))
    {
        return def;
    }

    return lit_value_asstring(args[id])->chars;
}

LitString* lit_value_checkobjstring(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isstring(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected a string as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }

    return lit_value_asstring(args[id]);
}

LitInstance* lit_value_checkinstance(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isinstance(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected an instance as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }

    return lit_value_asinstance(args[id]);
}

LitValue* lit_value_checkreference(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isreference(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected a reference as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }

    return lit_value_asreference(args[id])->slot;
}

void lit_value_ensurebool(LitVM* vm, LitValue value, const char* lit_emitter_raiseerror)
{
    if(!lit_value_isbool(value))
    {
        lit_vm_raiseexitingerror(vm, lit_emitter_raiseerror);
    }
}

void lit_value_ensurestring(LitVM* vm, LitValue value, const char* lit_emitter_raiseerror)
{
    if(!lit_value_isstring(value))
    {
        lit_vm_raiseexitingerror(vm, lit_emitter_raiseerror);
    }
}

void lit_value_ensurenumber(LitVM* vm, LitValue value, const char* lit_emitter_raiseerror)
{
    if(!lit_value_isnumber(value))
    {
        lit_vm_raiseexitingerror(vm, lit_emitter_raiseerror);
    }
}

void lit_value_ensureobjtype(LitVM* vm, LitValue value, LitObjType type, const char* lit_emitter_raiseerror)
{
    if(!lit_value_isobject(value) || lit_value_type(value) != type)
    {
        lit_vm_raiseexitingerror(vm, lit_emitter_raiseerror);
    }
}


LitValue lit_value_callnew(LitVM* vm, const char* name, LitValue* args, size_t argc, bool ignfiber)
{
    LitValue value;
    LitClass* klass;
    if(!lit_table_get(&vm->globals->values, CONST_STRING(vm->state, name), &value))
    {
        lit_vm_raiseerror(vm, "failed to create instance of class %s: class not found", name);
        return NULL_VALUE;
    }
    klass = lit_value_asclass(value);
    if(klass->init_method == NULL)
    {
        return lit_value_objectvalue(lit_create_instance(vm->state, klass));
    }
    return lit_state_callmethod(vm->state, value, value, args, argc, ignfiber).result;
}

