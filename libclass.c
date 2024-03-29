
#include "lit.h"

LitClass* lit_create_class(LitState* state, LitString* name)
{
    LitClass* klass;
    klass = (LitClass*)lit_gcmem_allocobject(state, sizeof(LitClass), LITTYPE_CLASS, false);
    klass->name = name;
    klass->init_method = NULL;
    klass->super = NULL;
    lit_table_init(state, &klass->methods);
    lit_table_init(state, &klass->static_fields);
    return klass;
}

LitClass* lit_create_classobject(LitState* state, const char* name)
{
    LitString* nm;
    LitClass* cl;
    nm = lit_string_copy(state, name, strlen(name));
    cl = lit_create_class(state, nm);
    cl->name = nm;
    return cl;
}

LitField* lit_create_field(LitState* state, LitObject* getter, LitObject* setter)
{
    LitField* field;
    field = (LitField*)lit_gcmem_allocobject(state, sizeof(LitField), LITTYPE_FIELD, false);
    field->getter = getter;
    field->setter = setter;
    return field;
}

LitInstance* lit_create_instance(LitState* state, LitClass* klass)
{
    LitInstance* instance;
    instance = (LitInstance*)lit_gcmem_allocobject(state, sizeof(LitInstance), LITTYPE_INSTANCE, false);
    instance->klass = klass;
    lit_table_init(state, &instance->fields);
    instance->fields.count = 0;
    return instance;
}

void lit_class_bindconstructor(LitState* state, LitClass* cl, LitNativeMethodFn fn)
{
    LitNativeMethod* mth;
    mth = lit_class_bindmethod(state, cl, "constructor", fn);
    cl->init_method = (LitObject*)mth;
}

LitNativeMethod* lit_class_bindmethod(LitState* state, LitClass* cl, const char* name, LitNativeMethodFn fn)
{
    LitString* nm;
    LitNativeMethod* mth;
    nm = lit_string_copy(state, name, strlen(name));
    mth = lit_create_native_method(state, fn, nm);
    lit_table_set(state, &cl->methods, nm, lit_value_objectvalue(mth));
    return mth;
}

LitPrimitiveMethod* lit_class_bindprimitive(LitState* state, LitClass* cl, const char* name, LitPrimitiveMethodFn fn)
{
    LitString* nm;
    LitPrimitiveMethod* mth;
    nm = lit_string_copy(state, name, strlen(name));
    mth = lit_create_primitive_method(state, fn, nm);
    lit_table_set(state, &cl->methods, nm, lit_value_objectvalue(mth));
    return mth;
}

LitNativeMethod* lit_class_bindstaticmethod(LitState* state, LitClass* cl, const char* name, LitNativeMethodFn fn)
{
    LitString* nm;
    LitNativeMethod* mth;
    nm = lit_string_copy(state, name, strlen(name));
    mth = lit_create_native_method(state, fn, nm);
    lit_table_set(state, &cl->static_fields, nm, lit_value_objectvalue(mth));
    return mth;
}

LitPrimitiveMethod* lit_class_bindstaticprimitive(LitState* state, LitClass* cl, const char* name, LitPrimitiveMethodFn fn)
{
    LitString* nm;
    LitPrimitiveMethod* mth;
    nm = lit_string_copy(state, name, strlen(name));
    mth = lit_create_primitive_method(state, fn, nm);
    lit_table_set(state, &cl->static_fields, nm, lit_value_objectvalue(mth));
    return mth;
}


void lit_class_setstaticfield(LitState* state, LitClass* cl, const char* name, LitValue val)
{
    LitString* nm;
    nm = lit_string_copy(state, name, strlen(name));
    lit_table_set(state, &cl->static_fields, nm, val);
}

LitField* lit_class_bindgetset(LitState* state, LitClass* cl, const char* name, LitNativeMethodFn getfn, LitNativeMethodFn setfn, bool isstatic)
{
    LitTable* tbl;
    LitField* field;
    LitString* nm;
    LitNativeMethod* mthset;
    LitNativeMethod* mthget;
    tbl = &cl->methods;
    mthset = NULL;
    mthget = NULL;

    nm = lit_string_copy(state, name, strlen(name));
    if(getfn != NULL)
    {
        mthget = lit_create_native_method(state, getfn, nm);
    }
    if(setfn != NULL)
    {
        mthset = lit_create_native_method(state, setfn, nm);
    }
    if(isstatic)
    {
        tbl = &cl->static_fields;
    }
    field = lit_create_field(state, (LitObject*)mthget, (LitObject*)mthset);
    lit_table_set(state, tbl, nm, lit_value_objectvalue(field)); 
    return field;
}


/*

    #define LIT_INHERIT_CLASS(super_klass)                                \
        klass->super = (LitClass*)super_klass;                            \
        if(klass->init_method == NULL)                                    \
        {                                                                 \
            klass->init_method = super_klass->init_method;                \
        }                                                                 \
        lit_table_add_all(state, &super_klass->methods, &klass->methods); \
        lit_table_add_all(state, &super_klass->static_fields, &klass->static_fields);
*/

void lit_class_inheritfrom(LitState* state, LitClass* current, LitClass* other)
{
    current->super = (LitClass*)other;
    if(current->init_method == NULL)
    {
        current->init_method = other->init_method;
    }
    lit_table_add_all(state, &other->methods, &current->methods); \
    lit_table_add_all(state, &other->static_fields, &current->static_fields);
}

static LitValue objfn_class_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_value_objectvalue(lit_string_format(vm->state, "class @", lit_value_objectvalue(lit_value_asclass(instance)->name)));
}

static LitValue objfn_class_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    bool fields;
    int value;
    int index;
    int mthcap;
    LitClass* klass;
    (void)argc;
    LIT_ENSURE_ARGS(vm->state, 1);
    klass = lit_value_asclass(instance);
    index = argv[0] == NULL_VALUE ? -1 : lit_value_asnumber(argv[0]);
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
    return lit_value_numbertovalue(vm->state, fields ? value + mthcap : value);
}


static LitValue objfn_class_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    bool fields;
    size_t index;
    size_t mthcap;
    LitClass* klass;
    index = lit_value_checknumber(vm, argv, argc, 0);
    klass = lit_value_asclass(instance);
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
    if(lit_value_isinstance(instance))
    {
        super = lit_value_asinstance(instance)->klass->super;
    }
    else
    {
        super = lit_value_asclass(instance)->super;
    }
    if(super == NULL)
    {
        return NULL_VALUE;
    }
    return lit_value_objectvalue(super);
}

static LitValue objfn_class_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitClass* klass;    
    LitValue value;
    (void)argc;
    klass = lit_value_asclass(instance);
    if(argc == 2)
    {
        if(!lit_value_isstring(argv[0]))
        {
            lit_vm_raiseexitingerror(vm, "class index must be a string");
        }

        lit_table_set(vm->state, &klass->static_fields, lit_value_asstring(argv[0]), argv[1]);
        return argv[1];
    }
    if(!lit_value_isstring(argv[0]))
    {
        lit_vm_raiseexitingerror(vm, "class index must be a string");
    }
    if(lit_table_get(&klass->static_fields, lit_value_asstring(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&klass->methods, lit_value_asstring(argv[0]), &value))
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
    if(lit_value_isclass(argv[0]))
    {
        selfclass = lit_value_asclass(instance);
        otherclass = lit_value_asclass(argv[0]);
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
    return lit_value_objectvalue(lit_value_asclass(instance)->name);
}

void lit_open_class_library(LitState* state)
{
    LitClass* klass;
    klass = lit_create_classobject(state, "Class");
    {
        lit_class_bindmethod(state, klass, "[]", objfn_class_subscript);
        lit_class_bindmethod(state, klass, "==", objfn_class_compare);
        lit_class_bindmethod(state, klass, "toString", objfn_class_tostring);
        lit_class_bindstaticmethod(state, klass, "toString", objfn_class_tostring);
        lit_class_bindstaticmethod(state, klass, "iterator", objfn_class_iterator);
        lit_class_bindstaticmethod(state, klass, "iteratorValue", objfn_class_iteratorvalue);
        lit_class_bindgetset(state, klass, "super", objfn_class_super, NULL, false);
        lit_class_bindgetset(state, klass, "super", objfn_class_super, NULL, true);
        lit_class_bindgetset(state, klass, "name", objfn_class_name, NULL, true);
        state->classvalue_class = klass;
    }
    lit_state_setglobal(state, klass->name, lit_value_objectvalue(klass));
}


