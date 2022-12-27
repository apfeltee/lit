
#include "lit.h"

static LitValue access_private(LitVM* vm, LitMap* map, LitString* name, LitValue* val)
{
    int index;
    LitValue value;
    LitString* id;
    LitModule* module;
    id = CONST_STRING(vm->state, "_module");
    if(!lit_table_get(&map->values, id, &value) || !IS_MODULE(value))
    {
        return NULL_VALUE;
    }
    module = AS_MODULE(value);

    if(id == name)
    {
        return OBJECT_VALUE(module);
    }

    if(lit_table_get(&module->private_names->values, name, &value))
    {
        index = (int)lit_value_to_number(value);
        if(index > -1 && index < (int)module->private_count)
        {
            if(val != NULL)
            {
                module->privates[index] = *val;
                return *val;
            }
            return module->privates[index];
        }
    }
    return NULL_VALUE;
}


static LitValue objfn_module_privates(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitModule* module;
    LitMap* map;
    (void)argc;
    (void)argv;
    module = IS_MODULE(instance) ? AS_MODULE(instance) : vm->fiber->module;
    map = module->private_names;
    if(map->index_fn == NULL)
    {
        map->index_fn = access_private;
        lit_table_set(vm->state, &map->values, CONST_STRING(vm->state, "_module"), OBJECT_VALUE(module));
    }
    return OBJECT_VALUE(map);
}

static LitValue objfn_module_current(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(vm->fiber->module);
}

static LitValue objfn_module_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_string_format(vm->state, "Module @", OBJECT_VALUE(AS_MODULE(instance)->name)));
}

static LitValue objfn_module_name(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(AS_MODULE(instance)->name);
}

void lit_open_module_library(LitState* state)
{
    LIT_BEGIN_CLASS("Module");
    {
        LIT_INHERIT_CLASS(state->objectvalue_class);
        LIT_BIND_CONSTRUCTOR(util_invalid_constructor);
        LIT_SET_STATIC_FIELD("loaded", OBJECT_VALUE(state->vm->modules));
        LIT_BIND_STATIC_GETTER("privates", objfn_module_privates);
        LIT_BIND_STATIC_GETTER("current", objfn_module_current);
        LIT_BIND_METHOD("toString", objfn_module_tostring);
        LIT_BIND_GETTER("name", objfn_module_name);
        LIT_BIND_GETTER("privates", objfn_module_privates);
        state->modulevalue_class = klass;
    }
    LIT_END_CLASS();
}

