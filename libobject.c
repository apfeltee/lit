
#include "lit.h"

static LitValue objfn_object_class(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_get_class_for(vm->state, instance));
}


static LitValue objfn_object_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_string_format(vm->state, "@ instance", OBJECT_VALUE(lit_get_class_for(vm->state, instance)->name)));
}

static LitValue objfn_object_tomap(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(AS_MAP(instance));
}

static LitValue objfn_object_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    if(!IS_INSTANCE(instance))
    {
        lit_runtime_error_exiting(vm, "cannot modify built-in types");
    }
    LitInstance* inst = AS_INSTANCE(instance);
    if(argc == 2)
    {
        if(!IS_STRING(argv[0]))
        {
            lit_runtime_error_exiting(vm, "object index must be a string");
        }

        lit_table_set(vm->state, &inst->fields, AS_STRING(argv[0]), argv[1]);
        return argv[1];
    }
    if(!IS_STRING(argv[0]))
    {
        lit_runtime_error_exiting(vm, "object index must be a string");
    }
    LitValue value;
    if(lit_table_get(&inst->fields, AS_STRING(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&inst->klass->static_fields, AS_STRING(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&inst->klass->methods, AS_STRING(argv[0]), &value))
    {
        return value;
    }
    return NULL_VALUE;
}

static LitValue objfn_object_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    LIT_ENSURE_ARGS(1)
    LitInstance* self = AS_INSTANCE(instance);
    int index = argv[0] == NULL_VALUE ? -1 : lit_value_to_number(argv[0]);
    int value = util_table_iterator(&self->fields, index);
    return value == -1 ? NULL_VALUE : lit_number_to_value(value);
}


static LitValue objfn_object_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index = lit_check_number(vm, argv, argc, 0);
    LitInstance* self = AS_INSTANCE(instance);

    return util_table_iterator_key(&self->fields, index);
}

void lit_open_object_library(LitState* state)
{
    LIT_BEGIN_CLASS("Object");
    {
        LIT_INHERIT_CLASS(state->classvalue_class);
        LIT_BIND_METHOD("toString", objfn_object_tostring);
        LIT_BIND_METHOD("toMap", objfn_object_tomap);
        LIT_BIND_METHOD("[]", objfn_object_subscript);
        LIT_BIND_METHOD("iterator", objfn_object_iterator);
        LIT_BIND_METHOD("iteratorValue", objfn_object_iteratorvalue);
        LIT_BIND_GETTER("class", objfn_object_class);
        state->objectvalue_class = klass;
        state->objectvalue_class->super = state->classvalue_class;
    }
    LIT_END_CLASS();
}
