
#include "lit.h"


static LitValue objfn_class_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_string_format(vm->state, "class @", OBJECT_VALUE(AS_CLASS(instance)->name)));
}


static LitValue objfn_class_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    LIT_ENSURE_ARGS(1);
    LitClass* klass = AS_CLASS(instance);
    int index = argv[0] == NULL_VALUE ? -1 : lit_value_to_number(argv[0]);
    int methodsCapacity = (int)klass->methods.capacity;
    bool fields = index >= methodsCapacity;

    int value = util_table_iterator(fields ? &klass->static_fields : &klass->methods, fields ? index - methodsCapacity : index);

    if(value == -1)
    {
        if(fields)
        {
            return NULL_VALUE;
        }

        index++;
        fields = true;
        value = util_table_iterator(&klass->static_fields, index - methodsCapacity);
    }

    return value == -1 ? NULL_VALUE : lit_number_to_value(fields ? value + methodsCapacity : value);
}


static LitValue objfn_class_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    size_t index = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    LitClass* klass = AS_CLASS(instance);
    size_t methodsCapacity = klass->methods.capacity;
    bool fields = index >= methodsCapacity;

    return util_table_iterator_key(fields ? &klass->static_fields : &klass->methods, fields ? index - methodsCapacity : index);
}


static LitValue objfn_class_super(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitClass* super;
    (void)vm;
    (void)argc;
    (void)argv;
    super = NULL;
    if(IS_INSTANCE(instance))
    {
        super = AS_INSTANCE(instance)->klass->super;
    }
    else
    {
        super = AS_CLASS(instance)->super;
    }
    if(super == NULL)
    {
        return NULL_VALUE;
    }
    return OBJECT_VALUE(super);
}

static LitValue objfn_class_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitClass* klass;    
    LitValue value;
    (void)argc;
    klass = AS_CLASS(instance);
    if(argc == 2)
    {
        if(!IS_STRING(argv[0]))
        {
            lit_runtime_error_exiting(vm, "class index must be a string");
        }

        lit_table_set(vm->state, &klass->static_fields, AS_STRING(argv[0]), argv[1]);
        return argv[1];
    }
    if(!IS_STRING(argv[0]))
    {
        lit_runtime_error_exiting(vm, "class index must be a string");
    }
    if(lit_table_get(&klass->static_fields, AS_STRING(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&klass->methods, AS_STRING(argv[0]), &value))
    {
        return value;
    }
    return NULL_VALUE;
}

static LitValue objfn_class_name(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(AS_CLASS(instance)->name);
}

void lit_open_class_library(LitState* state)
{
    LIT_BEGIN_CLASS("Class");
    {
        LIT_BIND_METHOD("toString", objfn_class_tostring);
        LIT_BIND_METHOD("[]", objfn_class_subscript);
        LIT_BIND_STATIC_METHOD("toString", objfn_class_tostring);
        LIT_BIND_STATIC_METHOD("iterator", objfn_class_iterator);
        LIT_BIND_STATIC_METHOD("iteratorValue", objfn_class_iteratorvalue);
        LIT_BIND_GETTER("super", objfn_class_super);
        LIT_BIND_STATIC_GETTER("super", objfn_class_super);
        LIT_BIND_STATIC_GETTER("name", objfn_class_name);
        state->classvalue_class = klass;
    }
    LIT_END_CLASS_IGNORING();
}

