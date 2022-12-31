
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
    return lit_value_numbertovalue(vm->state, fabs(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_cos(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, cos(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_sin(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, sin(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_tan(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, tan(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_acos(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, acos(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_asin(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, asin(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_atan(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, atan(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_atan2(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, atan2(lit_check_number(vm, argv, argc, 0), lit_check_number(vm, argv, argc, 1)));
}

static LitValue math_floor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, floor(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_ceil(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, ceil(lit_check_number(vm, argv, argc, 0)));
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
        return lit_value_numbertovalue(vm->state, round(value * places) / places);
    }
    return lit_value_numbertovalue(vm->state, round(value));
}

static LitValue math_min(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, fmin(lit_check_number(vm, argv, argc, 0), lit_check_number(vm, argv, argc, 1)));
}

static LitValue math_max(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, fmax(lit_check_number(vm, argv, argc, 0), lit_check_number(vm, argv, argc, 1)));
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
        return lit_value_numbertovalue(vm->state, fmax(x, fmin(y, z)));
    }
    return lit_value_numbertovalue(vm->state, fmax(y, fmin(x, z)));
}

static LitValue math_toRadians(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, lit_check_number(vm, argv, argc, 0) * M_PI / 180.0);
}

static LitValue math_toDegrees(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, lit_check_number(vm, argv, argc, 0) * 180.0 / M_PI);
}

static LitValue math_sqrt(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, sqrt(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_log(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, exp(lit_check_number(vm, argv, argc, 0)));
}

static LitValue math_exp(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    return lit_value_numbertovalue(vm->state, exp(lit_check_number(vm, argv, argc, 0)));
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

    if(!lit_table_get(&lit_value_asinstance(instance)->fields, CONST_STRING(state, "_data"), &data))
    {
        return 0;
    }

    return (size_t*)lit_value_asuserdata(data)->data;
}

static LitValue random_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitUserdata* userdata = lit_create_userdata(vm->state, sizeof(size_t), false);
    lit_table_set(vm->state, &lit_value_asinstance(instance)->fields, CONST_STRING(vm->state, "_data"), lit_value_objectvalue(userdata));

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
        return lit_value_numbertovalue(vm->state, rand_r((unsigned int*)data) % bound);
    }
    else if(argc == 2)
    {
        int min = (int)lit_get_number(vm, argv, argc, 0, 0);
        int max = (int)lit_get_number(vm, argv, argc, 1, 1);

        if(max - min == 0)
        {
            return lit_value_numbertovalue(vm->state, max);
        }

        return lit_value_numbertovalue(vm->state, min + rand_r((unsigned int*)data) % (max - min));
    }

    return lit_value_numbertovalue(vm->state, rand_r((unsigned int*)data));
}

static LitValue random_float(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t* data = extract_random_data(vm->state, instance);
    double value = (double)rand_r((unsigned int*)data) / RAND_MAX;

    if(argc == 1)
    {
        int bound = (int)lit_get_number(vm, argv, argc, 0, 0);
        return lit_value_numbertovalue(vm->state, value * bound);
    }
    else if(argc == 2)
    {
        int min = (int)lit_get_number(vm, argv, argc, 0, 0);
        int max = (int)lit_get_number(vm, argv, argc, 1, 1);

        if(max - min == 0)
        {
            return lit_value_numbertovalue(vm->state, max);
        }

        return lit_value_numbertovalue(vm->state, min + value * (max - min));
    }

    return lit_value_numbertovalue(vm->state, value);
}

static LitValue random_bool(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_bool_to_value(vm->state, rand_r((unsigned int*)extract_random_data(vm->state, instance)) % 2);
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
            LitArray* array = lit_value_asarray(argv[0]);

            if(lit_vallist_count(&array->list) == 0)
            {
                return NULL_VALUE;
            }

            return lit_vallist_get(&array->list, value % lit_vallist_count(&array->list));
        }
        else if(lit_value_ismap(argv[0]))
        {
            LitMap* map = lit_value_asmap(argv[0]);
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
    LitClass* klass;
    {
        klass = lit_create_classobject(state, "Math");
        {
            lit_class_setstaticfield(state, klass, "Pi", lit_value_numbertovalue(state, M_PI));
            lit_class_setstaticfield(state, klass, "Tau", lit_value_numbertovalue(state, M_PI * 2));
            lit_class_bindstaticmethod(state, klass, "abs", math_abs);
            lit_class_bindstaticmethod(state, klass, "sin", math_sin);
            lit_class_bindstaticmethod(state, klass, "cos", math_cos);
            lit_class_bindstaticmethod(state, klass, "tan", math_tan);
            lit_class_bindstaticmethod(state, klass, "asin", math_asin);
            lit_class_bindstaticmethod(state, klass, "acos", math_acos);
            lit_class_bindstaticmethod(state, klass, "atan", math_atan);
            lit_class_bindstaticmethod(state, klass, "atan2", math_atan2);
            lit_class_bindstaticmethod(state, klass, "floor", math_floor);
            lit_class_bindstaticmethod(state, klass, "ceil", math_ceil);
            lit_class_bindstaticmethod(state, klass, "round", math_round);
            lit_class_bindstaticmethod(state, klass, "min", math_min);
            lit_class_bindstaticmethod(state, klass, "max", math_max);
            lit_class_bindstaticmethod(state, klass, "mid", math_mid);
            lit_class_bindstaticmethod(state, klass, "toRadians", math_toRadians);
            lit_class_bindstaticmethod(state, klass, "toDegrees", math_toDegrees);
            lit_class_bindstaticmethod(state, klass, "sqrt", math_sqrt);
            lit_class_bindstaticmethod(state, klass, "log", math_log);
            lit_class_bindstaticmethod(state, klass, "exp", math_exp);
        }
        lit_set_global(state, klass->name, lit_value_objectvalue(klass));
        if(klass->super == NULL)
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
        };
    }
    srand(time(NULL));
    static_random_data = time(NULL);
    {
        klass = lit_create_classobject(state, "Random");
        {
            lit_class_bindconstructor(state, klass, random_constructor);
            lit_class_bindmethod(state, klass, "setSeed", random_setSeed);
            lit_class_bindmethod(state, klass, "int", random_int);
            lit_class_bindmethod(state, klass, "float", random_float);
            lit_class_bindmethod(state, klass, "chance", random_chance);
            lit_class_bindmethod(state, klass, "pick", random_pick);
            lit_class_bindstaticmethod(state, klass, "setSeed", random_setSeed);
            lit_class_bindstaticmethod(state, klass, "int", random_int);
            lit_class_bindstaticmethod(state, klass, "float", random_float);
            lit_class_bindstaticmethod(state, klass, "bool", random_bool);
            lit_class_bindstaticmethod(state, klass, "chance", random_chance);
            lit_class_bindstaticmethod(state, klass, "pick", random_pick);
        }
        lit_set_global(state, klass->name, lit_value_objectvalue(klass));
        if(klass->super == NULL)
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
        }
    }
}
