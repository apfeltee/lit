LitInterpretResult lit_interpret_fiber(LitState* state, LitFiber* fiber)
{
    uint8_t* ip;

    LitVm* vm;
    LitCallFrame* frame;
    LitChunk* current_chunk;
    LitValue a;
    LitValue b;
    LitValue tmpval;
    LitValue result;
    LitValue* slots;
    LitValue* privates;
    LitUpvalue** upvalues;
    vm = state->vm;
    vm_pushgc(state, true);
    vm->fiber = fiber;
    fiber->abort = false;
    frame = &fiber->frames[fiber->frame_count - 1];
    current_chunk = &frame->function->chunk;
    fiber->module = frame->function->module;
    ip = frame->ip;
    slots = frame->slots;
    privates = fiber->module->privates;
    upvalues = frame->closure == NULL ? NULL : frame->closure->upvalues;

    // Has to be inside of the function in order for goto to work
    static void* dispatch_table[] =
    {
        #define OPCODE(name, effect) &&OP_##name,
        #include "opcodes.inc"
        #undef OPCODE
    };

#ifdef LIT_TRACE_EXECUTION
    TRACE_FRAME();
    uint8_t instruction;
#endif

    while(true)
    {
#ifdef LIT_TRACE_STACK
        lit_trace_vm_stack(vm);
#endif

#ifdef LIT_CHECK_STACK_SIZE
        if((fiber->stack_top - frame->slots) > fiber->stack_capacity)
        {
            vm_rterrorvarg("Fiber stack is not large enough (%i > %i)", (int)(fiber->stack_top - frame->slots),
                               fiber->stack_capacity);
        }
#endif

#ifdef LIT_TRACE_EXECUTION
        instruction = *ip++;

        lit_disassemble_instruction(current_chunk, (size_t)(ip - current_chunk->code - 1), NULL);
        goto* dispatch_table[instruction];
#else
        goto* dispatch_table[*ip++];
#endif

        op_case(POP)
        {
            vm_drop();
            continue;
        }

        op_case(RETURN)
        {
            result = vm_pop();
            close_upvalues(vm, slots);

            vm_writeframe();
            fiber->frame_count--;

            if(frame->return_to_c)
            {
                frame->return_to_c = false;
                fiber->module->return_value = result;
                fiber->stack_top = frame->slots;

                return (LitInterpretResult){ INTERPRET_OK, result };
            }

            if(fiber->frame_count == 0)
            {
                fiber->module->return_value = result;

                if(fiber->parent == NULL)
                {
                    vm_drop();

                    state->allow_gc = was_allowed;
                    return (LitInterpretResult){ INTERPRET_OK, result };
                }

                size_t arg_count = fiber->arg_count;
                LitFiber* parent = fiber->parent;
                fiber->parent = NULL;

                vm->fiber = fiber = parent;

                vm_readframe();
                TRACE_FRAME();

                fiber->stack_top -= arg_count;
                fiber->stack_top[-1] = result;

                continue;
            }

            fiber->stack_top = frame->slots;

            if(frame->result_ignored)
            {
                fiber->stack_top++;
                frame->result_ignored = false;
            }
            else
            {
                vm_push(result);
            }

            vm_readframe();
            TRACE_FRAME();

            continue;
        }

        op_case(CONSTANT)
        {
            vm_push(vm_readconstant());
            continue;
        }

        op_case(CONSTANT_LONG)
        {
            vm_push(vm_readconstantlong());
            continue;
        }

        op_case(TRUE)
        {
            vm_push(TRUE_VALUE);
            continue;
        }

        op_case(FALSE)
        {
            vm_push(FALSE_VALUE);
            continue;
        }

        op_case(NULL)
        {
            vm_push(NULL_VALUE);
            continue;
        }

        op_case(ARRAY)
        {
            vm_push(OBJECT_VALUE(lit_create_array(state)));
            continue;
        }

        op_case(OBJECT)
        {
            vm_push(OBJECT_VALUE(lit_create_instance(state, state->objectvalue_class)));
            continue;
        }

        op_case(RANGE)
        {
            a = vm_pop();
            b = vm_pop();

            if(!IS_NUMBER(a) || !IS_NUMBER(b))
            {
                vm_rterror("Range operands must be number");
            }

            vm_push(OBJECT_VALUE(lit_create_range(state, lit_value_to_number(a), lit_value_to_number(b))));
            continue;
        }

        op_case(NEGATE)
        {
            if(!IS_NUMBER(vm_peek(0)))
            {
                LitValue arg = vm_peek(0);

                // Don't even ask me why
                // This doesn't kill our performance, since it's a error anyway
                if(IS_STRING(arg) && strcmp(AS_CSTRING(arg), "muffin") == 0)
                {
                    vm_rterror("Idk, can you negate a muffin?");
                }
                else
                {
                    vm_rterror("Operand must be a number");
                }
            }
            tmpval = lit_number_to_value(-lit_value_to_number(vm_pop()));
            vm_push(tmpval);
            continue;
        }

        op_case(NOT)
        {
            if(IS_INSTANCE(vm_peek(0)))
            {
                vm_writeframe();
                vm_invoke_from_class(AS_INSTANCE(vm_peek(0))->klass, CONST_STRING(state, "!"), 0, false, methods, false);
                continue;
            }
            tmpval = BOOL_VALUE(lit_is_falsey(vm_pop()));
            vm_push(tmpval);
            continue;
        }

        op_case(BNOT)
        {
            if(!IS_NUMBER(vm_peek(0)))
            {
                vm_rterror("Operand must be a number");
            }
            tmpval = lit_number_to_value(~((int)lit_value_to_number(vm_pop())));
            vm_push(tmpval);
            continue;
        }

        op_case(ADD)
        {
            vm_binaryop(lit_number_to_value, +, "+");
            continue;
        }

        op_case(SUBTRACT)
        {
            vm_binaryop(lit_number_to_value, -, "-");
            continue;
        }

        op_case(MULTIPLY)
        {
            vm_binaryop(lit_number_to_value, *, "*");
            continue;
        }

        op_case(POWER)
        {
            a = vm_peek(1);
            b = vm_peek(0);

            if(IS_NUMBER(a) && IS_NUMBER(b))
            {
                vm_drop();
                *(fiber->stack_top - 1) = (lit_number_to_value(pow(lit_value_to_number(a), lit_value_to_number(b))));

                continue;
            }

            vm_invokemethod(a, "**", 1);
            continue;
        }
        op_case(DIVIDE)
        {
            vm_binaryop(lit_number_to_value, /, "/");
            continue;
        }
        op_case(FLOOR_DIVIDE)
        {
            a = vm_peek(1);
            b = vm_peek(0);
            if(IS_NUMBER(a) && IS_NUMBER(b))
            {
                vm_drop();
                *(fiber->stack_top - 1) = (lit_number_to_value(floor(lit_value_to_number(a) / lit_value_to_number(b))));

                continue;
            }

            vm_invokemethod(a, "#", 1);
            continue;
        }
        op_case(MOD)
        {
            a = vm_peek(1);
            b = vm_peek(0);
            if(IS_NUMBER(a) && IS_NUMBER(b))
            {
                vm_drop();
                *(fiber->stack_top - 1) = lit_number_to_value(fmod(lit_value_to_number(a), lit_value_to_number(b)));
                continue;
            }
            vm_invokemethod(a, "%", 1);
            continue;
        }
        op_case(BAND)
        {
            vm_bitwiseop(&, "&");
            continue;
        }
        op_case(BOR)
        {
            vm_bitwiseop(|, "|");
            continue;
        }
        op_case(BXOR)
        {
            vm_bitwiseop(^, "^");
            continue;
        }
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
        op_case(EQUAL)
        {
            if(IS_INSTANCE(vm_peek(1)))
            {
                vm_writeframe();
                vm_invoke_from_class(AS_INSTANCE(vm_peek(1))->klass, CONST_STRING(state, "=="), 1, false, methods, false);
                continue;
            }
            a = vm_pop();
            b = vm_pop();
            vm_push(BOOL_VALUE(a == b));
            continue;
        }

        op_case(GREATER)
        {
            vm_binaryop(BOOL_VALUE, >, ">");
            continue;
        }
        op_case(GREATER_EQUAL)
        {
            vm_binaryop(BOOL_VALUE, >=, ">=");
            continue;
        }
        op_case(LESS)
        {
            vm_binaryop(BOOL_VALUE, <, "<");
            continue;
        }
        op_case(LESS_EQUAL)
        {
            vm_binaryop(BOOL_VALUE, <=, "<=");
            continue;
        }

        op_case(SET_GLOBAL)
        {
            LitString* name = vm_readstringlong();
            lit_table_set(state, &vm->globals->values, name, vm_peek(0));

            continue;
        }

        op_case(GET_GLOBAL)
        {
            LitString* name = vm_readstringlong();
            LitValue setval;

            if(!lit_table_get(&vm->globals->values, name, &setval))
            {
                vm_push(NULL_VALUE);
            }
            else
            {
                vm_push(setval);
            }
            continue;
        }

        op_case(SET_LOCAL)
        {
            uint8_t index = vm_readbyte();
            slots[index] = vm_peek(0);

            continue;
        }

        op_case(GET_LOCAL)
        {
            vm_push(slots[vm_readbyte()]);
            continue;
        }

        op_case(SET_LOCAL_LONG)
        {
            uint8_t index = vm_readshort();
            slots[index] = vm_peek(0);

            continue;
        }

        op_case(GET_LOCAL_LONG)
        {
            vm_push(slots[vm_readshort()]);
            continue;
        }

        op_case(SET_PRIVATE)
        {
            uint8_t index = vm_readbyte();
            privates[index] = vm_peek(0);

            continue;
        }

        op_case(GET_PRIVATE)
        {
            vm_push(privates[vm_readbyte()]);
            continue;
        }

        op_case(SET_PRIVATE_LONG)
        {
            uint8_t index = vm_readshort();
            privates[index] = vm_peek(0);

            continue;
        }

        op_case(GET_PRIVATE_LONG)
        {
            vm_push(privates[vm_readshort()]);
            continue;
        }

        op_case(SET_UPVALUE)
        {
            uint8_t index = vm_readbyte();
            *upvalues[index]->location = vm_peek(0);

            continue;
        }

        op_case(GET_UPVALUE)
        {
            vm_push(*upvalues[vm_readbyte()]->location);
            continue;
        }

        op_case(JUMP_IF_FALSE)
        {
            uint16_t offset = vm_readshort();

            if(lit_is_falsey(vm_pop()))
            {
                ip += offset;
            }

            continue;
        }

        op_case(JUMP_IF_NULL)
        {
            uint16_t offset = vm_readshort();

            if(IS_NULL(vm_peek(0)))
            {
                ip += offset;
            }

            continue;
        }

        op_case(JUMP_IF_NULL_POPPING)
        {
            uint16_t offset = vm_readshort();

            if(IS_NULL(vm_pop()))
            {
                ip += offset;
            }

            continue;
        }

        op_case(JUMP)
        {
            uint16_t offset = vm_readshort();
            ip += offset;

            continue;
        }

        op_case(JUMP_BACK)
        {
            uint16_t offset = vm_readshort();
            ip -= offset;

            continue;
        }

        op_case(AND)
        {
            uint16_t offset = vm_readshort();

            if(lit_is_falsey(vm_peek(0)))
            {
                ip += offset;
            }
            else
            {
                vm_drop();
            }

            continue;
        }

        op_case(OR)
        {
            uint16_t offset = vm_readshort();
            if(lit_is_falsey(vm_peek(0)))
            {
                vm_drop();
            }
            else
            {
                ip += offset;
            }
            continue;
        }

        op_case(NULL_OR)
        {
            uint16_t offset = vm_readshort();
            if(IS_NULL(vm_peek(0)))
            {
                vm_drop();
            }
            else
            {
                ip += offset;
            }
            continue;
        }

        op_case(CALL)
        {
            uint8_t arg_count = vm_readbyte();
            vm_writeframe();
            vm_callvalue(vm_peek(arg_count), arg_count);
            continue;
        }

        op_case(CLOSURE)
        {
            LitFunction* function = AS_FUNCTION(vm_readconstantlong());
            LitClosure* closure = lit_create_closure(state, function);

            vm_push(OBJECT_VALUE(closure));

            for(size_t i = 0; i < closure->upvalue_count; i++)
            {
                uint8_t is_local = vm_readbyte();
                uint8_t index = vm_readbyte();

                if(is_local)
                {
                    closure->upvalues[i] = capture_upvalue(state, frame->slots + index);
                }
                else
                {
                    closure->upvalues[i] = upvalues[index];
                }
            }

            continue;
        }

        op_case(CLOSE_UPVALUE)
        {
            close_upvalues(vm, fiber->stack_top - 1);
            vm_drop();

            continue;
        }

        op_case(CLASS)
        {
            LitString* name = vm_readstringlong();
            LitClass* klass = lit_create_class(state, name);

            vm_push(OBJECT_VALUE(klass));

            klass->super = state->objectvalue_class;

            lit_table_add_all(state, &klass->super->methods, &klass->methods);
            lit_table_add_all(state, &klass->super->static_fields, &klass->static_fields);

            lit_table_set(state, &vm->globals->values, name, OBJECT_VALUE(klass));

            continue;
        }

        op_case(GET_FIELD)
        {
            LitValue object = vm_peek(1);

            if(IS_NULL(object))
            {
                vm_rterror("Attempt to index a null value");
            }

            LitValue getval;
            LitString* name = AS_STRING(vm_peek(0));

            if(IS_INSTANCE(object))
            {
                LitInstance* instance = AS_INSTANCE(object);

                if(!lit_table_get(&instance->fields, name, &getval))
                {
                    if(lit_table_get(&instance->klass->methods, name, &getval))
                    {
                        if(IS_FIELD(getval))
                        {
                            LitField* field = AS_FIELD(getval);

                            if(field->getter == NULL)
                            {
                                vm_rterrorvarg("Class %s does not have a getter for the field %s",
                                                   instance->klass->name->chars, name->chars);
                            }

                            vm_drop();
                            vm_writeframe();
                            vm_callvalue(OBJECT_VALUE(AS_FIELD(getval)->getter), 0);
                            vm_readframe();
                            continue;
                        }
                        else
                        {
                            getval = OBJECT_VALUE(lit_create_bound_method(state, OBJECT_VALUE(instance), getval));
                        }
                    }
                    else
                    {
                        getval = NULL_VALUE;
                    }
                }
            }
            else if(IS_CLASS(object))
            {
                LitClass* klass = AS_CLASS(object);

                if(lit_table_get(&klass->static_fields, name, &getval))
                {
                    if(IS_NATIVE_METHOD(getval) || IS_PRIMITIVE_METHOD(getval))
                    {
                        getval = OBJECT_VALUE(lit_create_bound_method(state, OBJECT_VALUE(klass), getval));
                    }
                    else if(IS_FIELD(getval))
                    {
                        LitField* field = AS_FIELD(getval);

                        if(field->getter == NULL)
                        {
                            vm_rterrorvarg("Class %s does not have a getter for the field %s", klass->name->chars,
                                               name->chars);
                        }

                        vm_drop();
                        vm_writeframe();
                        vm_callvalue(OBJECT_VALUE(field->getter), 0);
                        vm_readframe();
                        continue;
                    }
                }
                else
                {
                    getval = NULL_VALUE;
                }
            }
            else
            {
                LitClass* klass = lit_get_class_for(state, object);

                if(klass == NULL)
                {
                    vm_rterror("Only instances and classes have fields");
                }

                if(lit_table_get(&klass->methods, name, &getval))
                {
                    if(IS_FIELD(getval))
                    {
                        LitField* field = AS_FIELD(getval);

                        if(field->getter == NULL)
                        {
                            vm_rterrorvarg("Class %s does not have a getter for the field %s", klass->name->chars,
                                               name->chars);
                        }

                        vm_drop();
                        vm_writeframe();
                        vm_callvalue(OBJECT_VALUE(AS_FIELD(getval)->getter), 0);
                        vm_readframe();
                        continue;
                    }
                    else if(IS_NATIVE_METHOD(getval) || IS_PRIMITIVE_METHOD(getval))
                    {
                        getval = OBJECT_VALUE(lit_create_bound_method(state, object, getval));
                    }
                }
                else
                {
                    getval = NULL_VALUE;
                }
            }

            vm_drop();// Pop field name
            fiber->stack_top[-1] = getval;

            continue;
        }

        op_case(SET_FIELD)
        {
            LitValue instance = vm_peek(2);

            if(IS_NULL(instance))
            {
                vm_rterror("Attempt to index a null value")
            }

            LitValue value = vm_peek(1);
            LitString* field_name = AS_STRING(vm_peek(0));

            if(IS_CLASS(instance))
            {
                LitClass* klass = AS_CLASS(instance);
                LitValue setter;

                if(lit_table_get(&klass->static_fields, field_name, &setter) && IS_FIELD(setter))
                {
                    LitField* field = AS_FIELD(setter);

                    if(field->setter == NULL)
                    {
                        vm_rterrorvarg("Class %s does not have a setter for the field %s", klass->name->chars,
                                           field_name->chars);
                    }

                    vm_dropn(2);
                    vm_push(value);
                    vm_writeframe();
                    vm_callvalue(OBJECT_VALUE(field->setter), 1);
                    vm_readframe();
                    continue;
                }
                if(IS_NULL(value))
                {
                    lit_table_delete(&klass->static_fields, field_name);
                }
                else
                {
                    lit_table_set(state, &klass->static_fields, field_name, value);
                }
                vm_dropn(2);// Pop field name and the value
                fiber->stack_top[-1] = value;
            }
            else if(IS_INSTANCE(instance))
            {
                LitInstance* inst = AS_INSTANCE(instance);
                LitValue setter;
                if(lit_table_get(&inst->klass->methods, field_name, &setter) && IS_FIELD(setter))
                {
                    LitField* field = AS_FIELD(setter);
                    if(field->setter == NULL)
                    {
                        vm_rterrorvarg("Class %s does not have a setter for the field %s", inst->klass->name->chars,
                                           field_name->chars);
                    }
                    vm_dropn(2);
                    vm_push(value);
                    vm_writeframe();
                    vm_callvalue(OBJECT_VALUE(field->setter), 1);
                    vm_readframe();
                    continue;
                }
                if(IS_NULL(value))
                {
                    lit_table_delete(&inst->fields, field_name);
                }
                else
                {
                    lit_table_set(state, &inst->fields, field_name, value);
                }
                vm_dropn(2);// Pop field name and the value
                fiber->stack_top[-1] = value;
            }
            else
            {
                LitClass* klass = lit_get_class_for(state, instance);
                if(klass == NULL)
                {
                    vm_rterror("Only instances and classes have fields");
                }
                LitValue setter;
                if(lit_table_get(&klass->methods, field_name, &setter) && IS_FIELD(setter))
                {
                    LitField* field = AS_FIELD(setter);
                    if(field->setter == NULL)
                    {
                        vm_rterrorvarg("Class %s does not have a setter for the field %s", klass->name->chars,
                                           field_name->chars);
                    }
                    vm_dropn(2);
                    vm_push(value);
                    vm_writeframe();
                    vm_callvalue(OBJECT_VALUE(field->setter), 1);
                    vm_readframe();
                    continue;
                }
                else
                {
                    vm_rterrorvarg("Class %s does not contain field %s", klass->name->chars, field_name->chars);
                }
            }
            continue;
        }
        op_case(SUBSCRIPT_GET)
        {
            vm_invokemethod(vm_peek(1), "[]", 1);
            continue;
        }
        op_case(SUBSCRIPT_SET)
        {
            vm_invokemethod(vm_peek(2), "[]", 2);
            continue;
        }
        op_case(PUSH_ARRAY_ELEMENT)
        {
            LitValues* values = &AS_ARRAY(vm_peek(1))->values;
            int index = values->count;
            lit_values_ensure_size(state, values, index + 1);
            values->values[index] = vm_peek(0);
            vm_drop();
            continue;
        }
        op_case(PUSH_OBJECT_FIELD)
        {
            LitValue operand = vm_peek(2);

            if(IS_MAP(operand))
            {
                lit_table_set(state, &AS_MAP(operand)->values, AS_STRING(vm_peek(1)), vm_peek(0));
            }
            else if(IS_INSTANCE(operand))
            {
                lit_table_set(state, &AS_INSTANCE(operand)->fields, AS_STRING(vm_peek(1)), vm_peek(0));
            }
            else
            {
                vm_rterrorvarg("Expected an object or a map as the operand, got %s", lit_get_value_type(operand));
            }
            vm_dropn(2);
            continue;
        }
        op_case(STATIC_FIELD)
        {
            lit_table_set(state, &AS_CLASS(vm_peek(1))->static_fields, vm_readstringlong(), vm_peek(0));
            vm_drop();
            continue;
        }
        op_case(METHOD)
        {
            LitClass* klass = AS_CLASS(vm_peek(1));
            LitString* name = vm_readstringlong();
            if((klass->init_method == NULL || (klass->super != NULL && klass->init_method == ((LitClass*)klass->super)->init_method))
               && name->length == 11 && memcmp(name->chars, "constructor", 11) == 0)
            {
                klass->init_method = AS_OBJECT(vm_peek(0));
            }
            lit_table_set(state, &klass->methods, name, vm_peek(0));
            vm_drop();
            continue;
        }
        op_case(DEFINE_FIELD)
        {
            lit_table_set(state, &AS_CLASS(vm_peek(1))->methods, vm_readstringlong(), vm_peek(0));
            vm_drop();
            continue;
        }
        op_case(INVOKE)
        {
            vm_invokeoperation(false);
            continue;
        }
        op_case(INVOKE_IGNORING)
        {
            vm_invokeoperation(true);
            continue;
        }
        op_case(INVOKE_SUPER)
        {
            uint8_t arg_count = vm_readbyte();
            LitString* method_name = vm_readstringlong();
            LitClass* klass = AS_CLASS(vm_pop());
            vm_writeframe();
            vm_invoke_from_class(klass, method_name, arg_count, true, methods, false);
            continue;
        }
        op_case(INVOKE_SUPER_IGNORING)
        {
            uint8_t arg_count = vm_readbyte();
            LitString* method_name = vm_readstringlong();
            LitClass* klass = AS_CLASS(vm_pop());
            vm_writeframe();
            vm_invoke_from_class(klass, method_name, arg_count, true, methods, true);
            continue;
        }
        op_case(GET_SUPER_METHOD)
        {
            LitString* method_name = vm_readstringlong();
            LitClass* klass = AS_CLASS(vm_pop());
            LitValue instance = vm_pop();
            LitValue value;
            if(lit_table_get(&klass->methods, method_name, &value))
            {
                value = OBJECT_VALUE(lit_create_bound_method(state, instance, value));
            }
            else
            {
                value = NULL_VALUE;
            }
            vm_push(value);
            continue;
        }
        op_case(INHERIT)
        {
            LitValue super = vm_peek(1);
            if(!IS_CLASS(super))
            {
                vm_rterror("Superclass must be a class");
            }
            LitClass* klass = AS_CLASS(vm_peek(0));
            LitClass* super_klass = AS_CLASS(super);
            klass->super = super_klass;
            klass->init_method = super_klass->init_method;
            lit_table_add_all(state, &super_klass->methods, &klass->methods);
            lit_table_add_all(state, &klass->super->static_fields, &klass->static_fields);
            continue;
        }
        op_case(IS)
        {
            LitValue instance = vm_peek(1);
            if(IS_NULL(instance))
            {
                vm_dropn(2);
                vm_push(FALSE_VALUE);

                continue;
            }
            LitClass* instance_klass = lit_get_class_for(state, instance);
            LitValue klass = vm_peek(0);
            if(instance_klass == NULL || !IS_CLASS(klass))
            {
                vm_rterror("operands must be an instance and a class");
            }
            LitClass* type = AS_CLASS(klass);
            bool found = false;
            while(instance_klass != NULL)
            {
                if(instance_klass == type)
                {
                    found = true;
                    break;
                }
                instance_klass = (LitClass*)instance_klass->super;
            }
            vm_dropn(2);// Drop the instance and class
            vm_push(BOOL_VALUE(found));
            continue;
        }
        op_case(POP_LOCALS)
        {
            vm_dropn(vm_readshort());
            continue;
        }
        op_case(VARARG)
        {
            LitValue slot = slots[vm_readbyte()];
            if(!IS_ARRAY(slot))
            {
                continue;
            }
            LitValues* values = &AS_ARRAY(slot)->values;
            lit_ensure_fiber_stack(state, fiber, values->count + frame->function->max_slots + (int)(fiber->stack_top - fiber->stack));
            for(size_t i = 0; i < values->count; i++)
            {
                vm_push(values->values[i]);
            }
            // Hot-bytecode patching, increment the amount of arguments to OP_CALL
            ip[1] = ip[1] + values->count - 1;
            continue;
        }

        op_case(REFERENCE_GLOBAL)
        {
            LitString* name = vm_readstringlong();
            LitValue* value;
            if(lit_table_get_slot(&vm->globals->values, name, &value))
            {
                vm_push(OBJECT_VALUE(lit_create_reference(state, value)));
            }
            else
            {
                vm_rterror("Attempt to reference a null value");
            }
            continue;
        }
        op_case(REFERENCE_PRIVATE)
        {
            vm_push(OBJECT_VALUE(lit_create_reference(state, &privates[vm_readshort()])));
            continue;
        }
        op_case(REFERENCE_LOCAL)
        {
            vm_push(OBJECT_VALUE(lit_create_reference(state, &slots[vm_readshort()])));
            continue;
        }
        op_case(REFERENCE_UPVALUE)
        {
            vm_push(OBJECT_VALUE(lit_create_reference(state, upvalues[vm_readbyte()]->location)));
            continue;
        }
        op_case(REFERENCE_FIELD)
        {
            LitValue object = vm_peek(1);
            if(IS_NULL(object))
            {
                vm_rterror("Attempt to index a null value");
            }
            LitValue* value;
            LitString* name = AS_STRING(vm_peek(0));
            if(IS_INSTANCE(object))
            {
                if(!lit_table_get_slot(&AS_INSTANCE(object)->fields, name, &value))
                {
                    vm_rterror("Attempt to reference a null value");
                }
            }
            else
            {
                lit_print_value(object);
                printf("\n");
                vm_rterror("You can only reference fields of real instances");
            }
            vm_drop();// Pop field name
            fiber->stack_top[-1] = OBJECT_VALUE(lit_create_reference(state, value));
            continue;
        }
        op_case(SET_REFERENCE)
        {
            LitValue reference = vm_pop();
            if(!IS_REFERENCE(reference))
            {
                vm_rterror("Provided value is not a reference");
            }
            *AS_REFERENCE(reference)->slot = vm_peek(0);
            continue;
        }
        vm_rterrorvarg("Unknown op code '%d'", *ip);
        break;
    }

    vm_returnerror();
}
