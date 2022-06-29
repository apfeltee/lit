LitValue mthval;
if(lit_table_get(instance)->fields, mthname, &mthval)) || lit_table_get(&instance->klass->methods, mthname, &mthval))
{
    if(false)
    {
        if(call_value(vm, mthval, 0))
        {
            vm_recoverstate();
            frame->result_ignored = true;
        }
        else
        {
            fiber->stack_top[-1] = vm_peek(0);
        }
    }
    else
    {
        vm_callvalue(mthval, 0);
    }
}
else
{
    if(false)
    {
        vm_rterrorvarg("Attempt to call method '%s', that is not defined in class %s", CONST_STRING(state, "!")->chars, instance->klass->name->chars)
    }
}
if(false)
{
    continue;
};
