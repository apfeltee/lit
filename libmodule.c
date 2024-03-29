
#include "lit.h"

static LitValue access_private(LitVM* vm, LitMap* map, LitString* name, LitValue* val)
{
    int index;
    LitValue value;
    LitString* id;
    LitModule* module;
    id = CONST_STRING(vm->state, "_module");
    if(!lit_table_get(&map->values, id, &value) || !lit_value_ismodule(value))
    {
        return NULL_VALUE;
    }
    module = lit_value_asmodule(value);

    if(id == name)
    {
        return lit_value_objectvalue(module);
    }

    if(lit_table_get(&module->private_names->values, name, &value))
    {
        index = (int)lit_value_asnumber(value);
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
    module = lit_value_ismodule(instance) ? lit_value_asmodule(instance) : vm->fiber->module;
    map = module->private_names;
    if(map->index_fn == NULL)
    {
        map->index_fn = access_private;
        lit_table_set(vm->state, &map->values, CONST_STRING(vm->state, "_module"), lit_value_objectvalue(module));
    }
    return lit_value_objectvalue(map);
}

static LitValue objfn_module_current(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_value_objectvalue(vm->fiber->module);
}

static LitValue objfn_module_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_value_objectvalue(lit_string_format(vm->state, "Module @", lit_value_objectvalue(lit_value_asmodule(instance)->name)));
}

static LitValue objfn_module_name(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_objectvalue(lit_value_asmodule(instance)->name);
}

void lit_open_module_library(LitState* state)
{
    LitClass* klass;
    klass = lit_create_classobject(state, "Module");
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
        lit_class_bindconstructor(state, klass, util_invalid_constructor);
        lit_class_setstaticfield(state, klass, "loaded", lit_value_objectvalue(state->vm->modules));
        lit_class_bindgetset(state, klass, "privates", objfn_module_privates, NULL, true);
        lit_class_bindgetset(state, klass, "current", objfn_module_current, NULL, true);
        lit_class_bindmethod(state, klass, "toString", objfn_module_tostring);
        lit_class_bindgetset(state, klass, "name", objfn_module_name, NULL, false);
        lit_class_bindgetset(state, klass, "privates", objfn_module_privates, NULL, false);
        state->modulevalue_class = klass;
    }
    lit_state_setglobal(state, klass->name, lit_value_objectvalue(klass));
    if(klass->super == NULL)
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
    };
}

