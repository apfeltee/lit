
#include <stdlib.h>
#include <time.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "lit.h"

#if !(defined(__unix__) || defined(__linux__))
    #define rand_r(v) (*v)
#endif

static LitValue math_abs(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(fabs(LIT_CHECK_NUMBER(0)));
}

static LitValue math_cos(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(cos(LIT_CHECK_NUMBER(0)));
}

static LitValue math_sin(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(sin(LIT_CHECK_NUMBER(0)));
}

static LitValue math_tan(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(tan(LIT_CHECK_NUMBER(0)));
}

static LitValue math_acos(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(acos(LIT_CHECK_NUMBER(0)));
}

static LitValue math_asin(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(asin(LIT_CHECK_NUMBER(0)));
}

static LitValue math_atan(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(atan(LIT_CHECK_NUMBER(0)));
}

static LitValue math_atan2(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(atan2(LIT_CHECK_NUMBER(0), LIT_CHECK_NUMBER(1)));
}

static LitValue math_floor(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(floor(LIT_CHECK_NUMBER(0)));
}

static LitValue math_ceil(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(ceil(LIT_CHECK_NUMBER(0)));
}

static LitValue math_round(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    double value = LIT_CHECK_NUMBER(0);

    if(arg_count > 1)
    {
        int places = (int)pow(10, LIT_CHECK_NUMBER(1));
        return NUMBER_VALUE(round(value * places) / places);
    }

    return NUMBER_VALUE(round(value));
}

static LitValue math_min(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(fmin(LIT_CHECK_NUMBER(0), LIT_CHECK_NUMBER(1)));
}

static LitValue math_max(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(fmax(LIT_CHECK_NUMBER(0), LIT_CHECK_NUMBER(1)));
}

static LitValue math_mid(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    double x = LIT_CHECK_NUMBER(0);
    double y = LIT_CHECK_NUMBER(1);
    double z = LIT_CHECK_NUMBER(2);

    if(x > y)
    {
        return NUMBER_VALUE(fmax(x, fmin(y, z)));
    }
    else
    {
        return NUMBER_VALUE(fmax(y, fmin(x, z)));
    }
}

static LitValue math_toRadians(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(LIT_CHECK_NUMBER(0) * M_PI / 180.0);
}

static LitValue math_toDegrees(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(LIT_CHECK_NUMBER(0) * 180.0 / M_PI);
}

static LitValue math_sqrt(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(sqrt(LIT_CHECK_NUMBER(0)));
}

static LitValue math_log(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(exp(LIT_CHECK_NUMBER(0)));
}

static LitValue math_exp(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(exp(LIT_CHECK_NUMBER(0)));
}

/*
 * Random
 */

static size_t static_random_data;

static size_t* extract_random_data(LitState* state, LitValue instance)
{
    if(IS_CLASS(instance))
    {
        return &static_random_data;
    }

    LitValue data;

    if(!lit_table_get(&AS_INSTANCE(instance)->fields, CONST_STRING(state, "_data"), &data))
    {
        return 0;
    }

    return (size_t*)AS_USERDATA(data)->data;
}

static LitValue random_constructor(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    LitUserdata* userdata = lit_create_userdata(vm->state, sizeof(size_t));
    lit_table_set(vm->state, &AS_INSTANCE(instance)->fields, CONST_STRING(vm->state, "_data"), OBJECT_VALUE(userdata));

    size_t* data = (size_t*)userdata->data;

    if(arg_count == 1)
    {
        size_t number = (size_t)LIT_CHECK_NUMBER(0);
        *data = number;
    }
    else
    {
        *data = time(NULL);
    }

    return OBJECT_VALUE(instance);
}

static LitValue random_setSeed(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    size_t* data = extract_random_data(vm->state, instance);

    if(arg_count == 1)
    {
        size_t number = (size_t)LIT_CHECK_NUMBER(0);
        *data = number;
    }
    else
    {
        *data = time(NULL);
    }

    return NULL_VALUE;
}

static LitValue random_int(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    size_t* data = extract_random_data(vm->state, instance);

    if(arg_count == 1)
    {
        int bound = (int)LIT_GET_NUMBER(0, 0);
        return NUMBER_VALUE(rand_r((unsigned int*)data) % bound);
    }
    else if(arg_count == 2)
    {
        int min = (int)LIT_GET_NUMBER(0, 0);
        int max = (int)LIT_GET_NUMBER(1, 1);

        if(max - min == 0)
        {
            return NUMBER_VALUE(max);
        }

        return NUMBER_VALUE(min + rand_r((unsigned int*)data) % (max - min));
    }

    return NUMBER_VALUE(rand_r((unsigned int*)data));
}

static LitValue random_float(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    size_t* data = extract_random_data(vm->state, instance);
    double value = (double)rand_r((unsigned int*)data) / RAND_MAX;

    if(arg_count == 1)
    {
        int bound = (int)LIT_GET_NUMBER(0, 0);
        return NUMBER_VALUE(value * bound);
    }
    else if(arg_count == 2)
    {
        int min = (int)LIT_GET_NUMBER(0, 0);
        int max = (int)LIT_GET_NUMBER(1, 1);

        if(max - min == 0)
        {
            return NUMBER_VALUE(max);
        }

        return NUMBER_VALUE(min + value * (max - min));
    }

    return NUMBER_VALUE(value);
}

static LitValue random_bool(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return BOOL_VALUE(rand_r((unsigned int*)extract_random_data(vm->state, instance)) % 2);
}

static LitValue random_chance(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    float c = LIT_GET_NUMBER(0, 50);
    return BOOL_VALUE((((float)rand_r((unsigned int*)extract_random_data(vm->state, instance))) / RAND_MAX * 100) <= c);
}

static LitValue random_pick(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    int value = rand_r((unsigned int*)extract_random_data(vm->state, instance));

    if(arg_count == 1)
    {
        if(IS_ARRAY(args[0]))
        {
            LitArray* array = AS_ARRAY(args[0]);

            if(array->values.count == 0)
            {
                return NULL_VALUE;
            }

            return array->values.values[value % array->values.count];
        }
        else if(IS_MAP(args[0]))
        {
            LitMap* map = AS_MAP(args[0]);
            size_t length = map->values.count;
            size_t capacity = map->values.capacity;

            if(length == 0)
            {
                return NULL_VALUE;
            }

            size_t target = value % length;
            size_t index = 0;

            for(size_t i = 0; i < capacity; i++)
            {
                if(map->values.entries[i].key != NULL)
                {
                    if(index == target)
                    {
                        return map->values.entries[i].value;
                    }

                    index++;
                }
            }
        }
        else
        {
            lit_runtime_error_exiting(vm, "Expected map or array as the argument");
        }
    }
    else
    {
        return args[value % arg_count];
    }

    return NULL_VALUE;
}

void lit_open_math_library(LitState* state)
{
    {
        LIT_BEGIN_CLASS("Math");
        {
            LIT_SET_STATIC_FIELD("Pi", NUMBER_VALUE(M_PI));
            LIT_SET_STATIC_FIELD("Tau", NUMBER_VALUE(M_PI * 2));
            LIT_BIND_STATIC_METHOD("abs", math_abs);
            LIT_BIND_STATIC_METHOD("sin", math_sin);
            LIT_BIND_STATIC_METHOD("cos", math_cos);
            LIT_BIND_STATIC_METHOD("tan", math_tan);
            LIT_BIND_STATIC_METHOD("asin", math_asin);
            LIT_BIND_STATIC_METHOD("acos", math_acos);
            LIT_BIND_STATIC_METHOD("atan", math_atan);
            LIT_BIND_STATIC_METHOD("atan2", math_atan2);
            LIT_BIND_STATIC_METHOD("floor", math_floor);
            LIT_BIND_STATIC_METHOD("ceil", math_ceil);
            LIT_BIND_STATIC_METHOD("round", math_round);
            LIT_BIND_STATIC_METHOD("min", math_min);
            LIT_BIND_STATIC_METHOD("max", math_max);
            LIT_BIND_STATIC_METHOD("mid", math_mid);
            LIT_BIND_STATIC_METHOD("toRadians", math_toRadians);
            LIT_BIND_STATIC_METHOD("toDegrees", math_toDegrees);
            LIT_BIND_STATIC_METHOD("sqrt", math_sqrt);
            LIT_BIND_STATIC_METHOD("log", math_log);
            LIT_BIND_STATIC_METHOD("exp", math_exp);
        }
        LIT_END_CLASS();
    }
    srand(time(NULL));
    static_random_data = time(NULL);
    {
        LIT_BEGIN_CLASS("Random");
        {
            LIT_BIND_CONSTRUCTOR(random_constructor);
            LIT_BIND_METHOD("setSeed", random_setSeed);
            LIT_BIND_METHOD("int", random_int);
            LIT_BIND_METHOD("float", random_float);
            LIT_BIND_METHOD("chance", random_chance);
            LIT_BIND_METHOD("pick", random_pick);
            LIT_BIND_STATIC_METHOD("setSeed", random_setSeed);
            LIT_BIND_STATIC_METHOD("int", random_int);
            LIT_BIND_STATIC_METHOD("float", random_float);
            LIT_BIND_STATIC_METHOD("bool", random_bool);
            LIT_BIND_STATIC_METHOD("chance", random_chance);
            LIT_BIND_STATIC_METHOD("pick", random_pick);
        }
        LIT_END_CLASS()
    }
}
