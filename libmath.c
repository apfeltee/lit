
#include <stdlib.h>
#include <time.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "lit.h"

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#if !(defined(__unix__) || defined(__linux__))
    #define rand_r(v) (*v)
#endif

static LitValue math_abs(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(fabs(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_cos(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(cos(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_sin(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(sin(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_tan(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(tan(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_acos(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(acos(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_asin(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(asin(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_atan(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(atan(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_atan2(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(atan2(lit_check_number(vm, argv, argc, 0), lit_check_number(vm, argv, argc, 1)));
}

static LitValue math_floor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(floor(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_ceil(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(ceil(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_round(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int places;
    double value;
    (void)instance;
    value = lit_check_number(vm, argv, argc, 0);
    if(argc > 1)
    {
        places = (int)pow(10, lit_check_number(vm, argv, argc, 1));
        return lit_number_to_value(round(value * places) / places);
    }
    return lit_number_to_value(round(value));
}

static LitValue math_min(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(fmin(lit_check_number(vm, argv, argc, 0), lit_check_number(vm, argv, argc, 1)));
}

static LitValue math_max(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(fmax(lit_check_number(vm, argv, argc, 0), lit_check_number(vm, argv, argc, 1)));
}

static LitValue math_mid(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    double x;
    double y;
    double z;
    (void)instance;
    x = lit_check_number(vm, argv, argc, 0);
    y = lit_check_number(vm, argv, argc, 1);
    z = lit_check_number(vm, argv, argc, 2);
    if(x > y)
    {
        return lit_number_to_value(fmax(x, fmin(y, z)));
    }
    return lit_number_to_value(fmax(y, fmin(x, z)));
}

static LitValue math_toRadians(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(lit_check_number(vm, argv, argc, 0) * M_PI / 180.0);
}

static LitValue math_toDegrees(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(lit_check_number(vm, argv, argc, 0) * 180.0 / M_PI);
}

static LitValue math_sqrt(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(sqrt(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_log(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(exp(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_exp(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_number_to_value(exp(lit_check_number(vm, argv, argc, 0)));
}

/*
 * Random
 */

static size_t static_random_data;

static size_t* extract_random_data(LitState* state, LitValue instance)
{
    if(lit_value_isclass(instance))
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

static LitValue random_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitUserdata* userdata = lit_create_userdata(vm->state, sizeof(size_t), false);
    lit_table_set(vm->state, &AS_INSTANCE(instance)->fields, CONST_STRING(vm->state, "_data"), lit_value_objectvalue(userdata));

    size_t* data = (size_t*)userdata->data;

    if(argc == 1)
    {
        size_t number = (size_t)lit_check_number(vm, argv, argc, 0);
        *data = number;
    }
    else
    {
        *data = time(NULL);
    }

    return lit_value_objectvalue(instance);
}

static LitValue random_setSeed(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t* data = extract_random_data(vm->state, instance);

    if(argc == 1)
    {
        size_t number = (size_t)lit_check_number(vm, argv, argc, 0);
        *data = number;
    }
    else
    {
        *data = time(NULL);
    }

    return NULL_VALUE;
}

static LitValue random_int(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t* data = extract_random_data(vm->state, instance);

    if(argc == 1)
    {
        int bound = (int)lit_get_number(vm, argv, argc, 0, 0);
        return lit_number_to_value(rand_r((unsigned int*)data) % bound);
    }
    else if(argc == 2)
    {
        int min = (int)lit_get_number(vm, argv, argc, 0, 0);
        int max = (int)lit_get_number(vm, argv, argc, 1, 1);

        if(max - min == 0)
        {
            return lit_number_to_value(max);
        }

        return lit_number_to_value(min + rand_r((unsigned int*)data) % (max - min));
    }

    return lit_number_to_value(rand_r((unsigned int*)data));
}

static LitValue random_float(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t* data = extract_random_data(vm->state, instance);
    double value = (double)rand_r((unsigned int*)data) / RAND_MAX;

    if(argc == 1)
    {
        int bound = (int)lit_get_number(vm, argv, argc, 0, 0);
        return lit_number_to_value(value * bound);
    }
    else if(argc == 2)
    {
        int min = (int)lit_get_number(vm, argv, argc, 0, 0);
        int max = (int)lit_get_number(vm, argv, argc, 1, 1);

        if(max - min == 0)
        {
            return lit_number_to_value(max);
        }

        return lit_number_to_value(min + value * (max - min));
    }

    return lit_number_to_value(value);
}

static LitValue random_bool(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_value_boolvalue(rand_r((unsigned int*)extract_random_data(vm->state, instance)) % 2);
}

static LitValue random_chance(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    float c;
    c = lit_get_number(vm, argv, argc, 0, 50);
    if((((float)rand_r((unsigned int*)extract_random_data(vm->state, instance))) / ((float)(RAND_MAX-1)) * 100) <= c)
    {
        return TRUE_VALUE;
    }
    return FALSE_VALUE;
}

static LitValue random_pick(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int value = rand_r((unsigned int*)extract_random_data(vm->state, instance));

    if(argc == 1)
    {
        if(lit_value_isarray(argv[0]))
        {
            LitArray* array = AS_ARRAY(argv[0]);

            if(lit_vallist_count(&array->list) == 0)
            {
                return NULL_VALUE;
            }

            return lit_vallist_get(&array->list, value % lit_vallist_count(&array->list));
        }
        else if(lit_value_ismap(argv[0]))
        {
            LitMap* map = AS_MAP(argv[0]);
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
        return argv[value % argc];
    }

    return NULL_VALUE;
}

void lit_open_math_library(LitState* state)
{
    {
        LIT_BEGIN_CLASS("Math");
        {
            LIT_SET_STATIC_FIELD("Pi", lit_number_to_value(M_PI));
            LIT_SET_STATIC_FIELD("Tau", lit_number_to_value(M_PI * 2));
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
