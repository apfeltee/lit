
#define vm_bitwiseop(op, op_string) \
    LitValue a = vm_peek(fiber, 1); \
    LitValue b = vm_peek(fiber, 0); \
    if((!IS_NUMBER(a) && !IS_NULL(a)) || (!IS_NUMBER(b) && !IS_NULL(b))) \
    { \
        vm_rterrorvarg("Operands of bitwise op %s must be two numbers, got %s and %s", op_string, \
                           lit_get_value_type(a), lit_get_value_type(b)); \
    } \
    vm_drop(fiber); \
    *(fiber->stack_top - 1) = (lit_number_to_value((int)lit_value_to_number(a) op(int) lit_value_to_number(b)));


            op_case(LSHIFT)
            {
                vm_bitwiseop(<<, "<<");
                continue;
            }
            op_case(RSHIFT)
            {
                vm_bitwiseop(>>, ">>");
                continue;
            }

