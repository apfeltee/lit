
#include "lit.h"

static LitValue objfn_class_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_string_format(vm->state, "class @", OBJECT_VALUE(AS_CLASS(instance)->name)));
}

static LitValue objfn_class_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    bool fields;
    int value;
    int index;
    int mthcap;
    LitClass* klass;
    (void)argc;
    LIT_ENSURE_ARGS(1);
    klass = AS_CLASS(instance);
    index = argv[0] == NULL_VALUE ? -1 : lit_value_to_number(argv[0]);
    mthcap = (int)klass->methods.capacity;
    fields = index >= mthcap;
    value = util_table_iterator(fields ? &klass->static_fields : &klass->methods, fields ? index - mthcap : index);
    if(value == -1)
    {
        if(fields)
        {
            return NULL_VALUE;
        }
        index++;
        fields = true;
        value = util_table_iterator(&klass->static_fields, index - mthcap);
    }
    if(value == -1)
    {
        return NULL_VALUE;
    }
    return lit_number_to_value(fields ? value + mthcap : value);
}


static LitValue objfn_class_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    bool fields;
    size_t index;
    size_t mthcap;
    LitClass* klass;
    index = lit_check_number(vm, argv, argc, 0);
    klass = AS_CLASS(instance);
    mthcap = klass->methods.capacity;
    fields = index >= mthcap;
    return util_table_iterator_key(fields ? &klass->static_fields : &klass->methods, fields ? index - mthcap : index);
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

        lit_table_set(vm->state, &klass->static_fields, lit_as_string(argv[0]), argv[1]);
        return argv[1];
    }
    if(!IS_STRING(argv[0]))
    {
        lit_runtime_error_exiting(vm, "class index must be a string");
    }
    if(lit_table_get(&klass->static_fields, lit_as_string(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&klass->methods, lit_as_string(argv[0]), &value))
    {
        return value;
    }
    return NULL_VALUE;
}

static LitValue objfn_class_compare(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitClass* selfclass;
    LitClass* otherclass;
    (void)vm;
    (void)argc;
    if(IS_CLASS(argv[0]))
    {
        selfclass = AS_CLASS(instance);
        otherclass = AS_CLASS(argv[0]);
        if(lit_string_equal(vm->state, selfclass->name, otherclass->name))
        {
            if(selfclass == otherclass)
            {
                return TRUE_VALUE;
            }
        }
    }
    return FALSE_VALUE;
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
        LIT_BIND_METHOD("[]", objfn_class_subscript);
        LIT_BIND_METHOD("==", objfn_class_compare);
        LIT_BIND_METHOD("toString", objfn_class_tostring);
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

