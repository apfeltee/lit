
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


bool lit_compare_values(LitState* state, const LitValue a, const LitValue b)
{
    LitInterpretResult inret;
    LitValue args[3];
    if(lit_value_isinstance(a))
    {
        args[0] = b;
        inret = lit_instance_call_method(state, a, CONST_STRING(state, "=="), args, 1);
        if(inret.type == LITRESULT_OK)
        {
            if(lit_bool_to_value(state, inret.result) == TRUE_VALUE)
            {
                return true;
            }
            return false;
        }
    }
    return (a == b);
}
