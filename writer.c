
#include <stdarg.h>
#include <stdio.h>
#include "priv.h"
#include "sds.h"

static void litwr_cb_writebyte(LitWriter* wr, int byte)
{
    LitString* ds;
    if(wr->stringmode)
    {
        ds = (LitString*)wr->uptr;
        lit_string_appendchar(ds, byte);        
    }
    else
    {
        fputc(byte, (FILE*)wr->uptr);
    }
}

static void litwr_cb_writestring(LitWriter* wr, const char* string, size_t len)
{
    LitString* ds;
    if(wr->stringmode)
    {
        ds = (LitString*)wr->uptr;
        lit_string_appendlen(ds, string, len);
    }
    else
    {
        fwrite(string, sizeof(char), len, (FILE*)wr->uptr);
    }
}

static void litwr_cb_writeformat(LitWriter* wr, const char* fmt, va_list va)
{
    LitString* ds;
    if(wr->stringmode)
    {
        ds = (LitString*)wr->uptr;
        ds->chars = sdscatvprintf(ds->chars, fmt, va);
    }
    else
    {
        vfprintf((FILE*)wr->uptr, fmt, va);
    }
}

static void lit_writer_init_default(LitState* state, LitWriter* wr)
{
    wr->state = state;
    wr->forceflush = false;
    wr->stringmode = false;
    wr->fnbyte = litwr_cb_writebyte;
    wr->fnstring = litwr_cb_writestring;
    wr->fnformat = litwr_cb_writeformat;
}

void lit_writer_init_file(LitState* state, LitWriter* wr, FILE* fh, bool forceflush)
{
    lit_writer_init_default(state, wr);
    wr->uptr = fh;
    wr->forceflush = forceflush;
}

void lit_writer_init_string(LitState* state, LitWriter* wr)
{
    lit_writer_init_default(state, wr);
    wr->stringmode = true;
    wr->uptr = lit_string_makeempty(state, 0, false);
}

void lit_writer_writebyte(LitWriter* wr, int byte)
{
    wr->fnbyte(wr, byte);
}

void lit_writer_writestringl(LitWriter* wr, const char* str, size_t len)
{
    wr->fnstring(wr, str, len);
}

void lit_writer_writestring(LitWriter* wr, const char* str)
{
    wr->fnstring(wr, str, strlen(str));
}

void lit_writer_writeformat(LitWriter* wr, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    wr->fnformat(wr, fmt, va);
    va_end(va);
}

LitString* lit_writer_get_string(LitWriter* wr)
{
    if(wr->stringmode)
    {
        return (LitString*)wr->uptr;
    }
    return NULL;
}

static const char* lit_object_type_names[] =
{
    "string",
    "function",
    "native_function",
    "native_primitive",
    "native_method",
    "primitive_method",
    "fiber",
    "module",
    "closure",
    "upvalue",
    "class",
    "instance",
    "bound_method",
    "array",
    "map",
    "userdata",
    "range",
    "field",
    "reference"
};



static void print_array(LitState* state, LitWriter* wr, LitArray* array, size_t size)
{
    size_t i;
    lit_writer_writeformat(wr, "(%u) [", (unsigned int)size);
    if(size > 0)
    {
        lit_writer_writestring(wr, " ");
        for(i = 0; i < size; i++)
        {
            if(lit_value_isarray(lit_vallist_get(&array->list, i)) && (array == lit_value_asarray(lit_vallist_get(&array->list,i))))
            {
                lit_writer_writestring(wr, "(recursion)");
            }
            else
            {
                lit_towriter_value(state, wr, lit_vallist_get(&array->list, i));
            }
            if(i + 1 < size)
            {
                lit_writer_writestring(wr, ", ");
            }
            else
            {
                lit_writer_writestring(wr, " ");
            }
        }
    }
    lit_writer_writestring(wr, "]");
}

static void print_map(LitState* state, LitWriter* wr, LitMap* map, size_t size)
{
    bool had_before;
    size_t i;
    LitTableEntry* entry;
    lit_writer_writeformat(wr, "(%u) {", (unsigned int)size);
    had_before = false;
    if(size > 0)
    {
        for(i = 0; i < (size_t)map->values.capacity; i++)
        {
            entry = &map->values.entries[i];
            if(entry->key != NULL)
            {
                if(had_before)
                {
                    lit_writer_writestring(wr, ", ");
                }
                else
                {
                    lit_writer_writestring(wr, " ");
                }
                lit_writer_writeformat(wr, "%s = ", entry->key->chars);
                if(lit_value_ismap(entry->value) && (map == lit_value_asmap(entry->value)))
                {
                    lit_writer_writestring(wr, "(recursion)");
                }
                else
                {
                    lit_towriter_value(state, wr, entry->value);
                }
                had_before = true;
            }
        }
    }
    if(had_before)
    {
        lit_writer_writestring(wr, " }");
    }
    else
    {
        lit_writer_writestring(wr, "}");
    }
}

void lit_towriter_object(LitState* state, LitWriter* wr, LitValue value)
{
    size_t size;
    LitMap* map;
    LitArray* array;
    LitRange* range;
    LitValue* slot;
    LitObject* obj;
    LitUpvalue* upvalue;
    obj = lit_value_asobject(value);
    if(obj != NULL)
    {
        switch(obj->type)
        {
            case LITTYPE_STRING:
                {
                    lit_writer_writeformat(wr, "%s", lit_value_ascstring(value));
                }
                break;
            case LITTYPE_FUNCTION:
                {
                    lit_writer_writeformat(wr, "function %s", lit_value_asfunction(value)->name->chars);
                }
                break;
            case LITTYPE_CLOSURE:
                {
                    lit_writer_writeformat(wr, "closure %s", lit_value_asclosure(value)->function->name->chars);
                }
                break;
            case LITTYPE_NATIVE_PRIMITIVE:
                {
                    lit_writer_writeformat(wr, "function %s", lit_value_asnativeprimitive(value)->name->chars);
                }
                break;
            case LITTYPE_NATIVE_FUNCTION:
                {
                    lit_writer_writeformat(wr, "function %s", lit_value_asnativefunction(value)->name->chars);
                }
                break;
            case LITTYPE_PRIMITIVE_METHOD:
                {
                    lit_writer_writeformat(wr, "function %s", lit_value_asprimitivemethod(value)->name->chars);
                }
                break;
            case LITTYPE_NATIVE_METHOD:
                {
                    lit_writer_writeformat(wr, "function %s", lit_value_asnativemethod(value)->name->chars);
                }
                break;
            case LITTYPE_FIBER:
                {
                    lit_writer_writeformat(wr, "fiber");
                }
                break;
            case LITTYPE_MODULE:
                {
                    lit_writer_writeformat(wr, "module %s", lit_value_asmodule(value)->name->chars);
                }
                break;

            case LITTYPE_UPVALUE:
                {
                    upvalue = lit_value_asupvalue(value);
                    if(upvalue->location == NULL)
                    {
                        lit_towriter_value(state, wr, upvalue->closed);
                    }
                    else
                    {
                        lit_towriter_object(state, wr, *upvalue->location);
                    }
                }
                break;
            case LITTYPE_CLASS:
                {
                    lit_writer_writeformat(wr, "class %s", lit_value_asclass(value)->name->chars);
                }
                break;
            case LITTYPE_INSTANCE:
                {
                    /*
                    if(lit_value_asinstance(value)->klass->object.type == LITTYPE_MAP)
                    {
                        fprintf(stderr, "instance is a map\n");
                    }
                    printf("%s instance", lit_value_asinstance(value)->klass->name->chars);
                    */
                    lit_writer_writeformat(wr, "<instance '%s' ", lit_value_asinstance(value)->klass->name->chars);
                    map = lit_value_asmap(value);
                    size = map->values.count;
                    print_map(state, wr, map, size);
                    lit_writer_writestring(wr, ">");
                }
                break;
            case LITTYPE_BOUND_METHOD:
                {
                    lit_towriter_value(state, wr, lit_value_asboundmethod(value)->method);
                    return;
                }
                break;
            case LITTYPE_ARRAY:
                {
                    #ifdef LIT_MINIMIZE_CONTAINERS
                        lit_writer_writestring(wr, "array");
                    #else
                        array = lit_value_asarray(value);
                        size = lit_vallist_count(&array->list);
                        print_array(state, wr, array, size);
                    #endif
                }
                break;
            case LITTYPE_MAP:
                {
                    #ifdef LIT_MINIMIZE_CONTAINERS
                        lit_writer_writeformat(wr, "map");
                    #else
                        map = lit_value_asmap(value);
                        size = map->values.count;
                        print_map(state, wr, map, size);
                    #endif
                }
                break;
            case LITTYPE_USERDATA:
                {
                    lit_writer_writeformat(wr, "userdata");
                }
                break;
            case LITTYPE_RANGE:
                {
                    range = lit_value_asrange(value);
                    lit_writer_writeformat(wr, "%g .. %g", range->from, range->to);
                }
                break;
            case LITTYPE_FIELD:
                {
                    lit_writer_writeformat(wr, "field");
                }
                break;
            case LITTYPE_REFERENCE:
                {
                    lit_writer_writeformat(wr, "reference => ");
                    slot = lit_value_asreference(value)->slot;
                    if(slot == NULL)
                    {
                        lit_writer_writestring(wr, "null");
                    }
                    else
                    {
                        lit_towriter_value(state, wr, *slot);
                    }
                }
                break;
            default:
                {
                }
                break;
        }
    }
    else
    {
        lit_writer_writestring(wr, "!nullpointer!");
    }
}

//LitInterpretResult lit_call_instance_method(LitState* state, LitInstance* instance, LitString* mthname, LitValue* argv, size_t argc)
//
void lit_towriter_value(LitState* state, LitWriter* wr, LitValue value)
{
    /*
    LitValue mthtostring;
    LitValue tstrval;
    LitString* tstring;
    LitString* mthname;
    LitInterpretResult inret;
    LitValue args[1] = {NULL_VALUE};
    mthname = CONST_STRING(state, "toString");
    fprintf(stderr, "lit_towriter_value: checking if toString() exists for '%s' ...\n", lit_tostring_typename(value));
    if(lit_value_asclass(value) != NULL)
    {
        mthtostring = lit_state_getinstancemethod(state, value, mthname);
        if(!lit_value_isnull(mthtostring))
        {
            fprintf(stderr, "lit_towriter_value: we got toString()! now checking if calling it works ...\n");
            inret = lit_state_callinstancemethod(state, value, mthname, args, 0);
            if(inret.type == LITRESULT_OK)
            {
                fprintf(stderr, "lit_towriter_value: calling toString() succeeded! but is it a string? ...\n");
                tstrval = inret.result;
                if(!lit_value_isnull(tstrval))
                {
                    fprintf(stderr, "lit_towriter_value: toString() returned a string! so that's what we'll use.\n");
                    tstring = lit_value_asstring(tstrval);
                    printf("%.*s", (int)lit_string_getlength(tstring), tstring->chars);
                    return;
                }
            }
        }
    }
    fprintf(stderr, "lit_towriter_value: nope, no toString(), or it didn't return a string. falling back to manual stringification\n");
    */
    if(lit_value_isbool(value))
    {
        lit_writer_writestring(wr, lit_value_asbool(value) ? "true" : "false");
    }
    else if(lit_value_isnull(value))
    {
        lit_writer_writestring(wr, "null");
    }
    else if(lit_value_isnumber(value))
    {
        lit_writer_writeformat(wr, "%g", lit_value_asnumber(value));
    }
    else if(lit_value_isobject(value))
    {
        lit_towriter_object(state, wr, value);
    }
}


const char* lit_tostring_typename(LitValue value)
{
    if(lit_value_isbool(value))
    {
        return "bool";
    }
    else if(lit_value_isnull(value))
    {
        return "null";
    }
    else if(lit_value_isnumber(value))
    {
        return "number";
    }
    else if(lit_value_isobject(value))
    {
        return lit_object_type_names[lit_value_type(value)];
    }
    return "unknown";
}

const char* lit_tostring_exprtype(LitExprType t)
{
    switch(t)
    {
        case LITEXPR_LITERAL: return "LITERAL";
        case LITEXPR_BINARY: return "BINARY";
        case LITEXPR_UNARY: return "UNARY";
        case LITEXPR_VAREXPR: return "VAREXPR";
        case LITEXPR_ASSIGN: return "ASSIGN";
        case LITEXPR_CALL: return "CALL";
        case LITEXPR_SET: return "SET";
        case LITEXPR_GET: return "GET";
        case LITEXPR_LAMBDA: return "LAMBDA";
        case LITEXPR_ARRAY: return "ARRAY";
        case LITEXPR_OBJECT: return "OBJECT";
        case LITEXPR_SUBSCRIPT: return "SUBSCRIPT";
        case LITEXPR_THIS: return "THIS";
        case LITEXPR_SUPER: return "SUPER";
        case LITEXPR_RANGE: return "RANGE";
        case LITEXPR_TERNARY: return "TERNARY";
        case LITEXPR_INTERPOLATION: return "INTERPOLATION";
        case LITEXPR_REFERENCE: return "REFERENCE";
        case LITEXPR_EXPRESSION: return "EXPRESSION";
        case LITEXPR_BLOCK: return "BLOCK";
        case LITEXPR_IFSTMT: return "IFSTMT";
        case LITEXPR_WHILE: return "WHILE";
        case LITEXPR_FOR: return "FOR";
        case LITEXPR_VARSTMT: return "VARSTMT";
        case LITEXPR_CONTINUE: return "CONTINUE";
        case LITEXPR_BREAK: return "BREAK";
        case LITEXPR_FUNCTION: return "FUNCTION";
        case LITEXPR_RETURN: return "RETURN";
        case LITEXPR_METHOD: return "METHOD";
        case LITEXPR_CLASS: return "CLASS";
        case LITEXPR_FIELD: return "FIELD";
        default:
            break;
    }
    return "unknown";
}

const char* lit_tostring_optok(LitTokType t)
{
    switch(t)
    {
        case LITTOK_NEW_LINE: return "NEW_LINE";
        case LITTOK_LEFT_PAREN: return "(";
        case LITTOK_RIGHT_PAREN: return ")";
        case LITTOK_LEFT_BRACE: return "{";
        case LITTOK_RIGHT_BRACE: return "}";
        case LITTOK_LEFT_BRACKET: return "[";
        case LITTOK_RIGHT_BRACKET: return "]";
        case LITTOK_COMMA: return ",";
        case LITTOK_SEMICOLON: return ";";
        case LITTOK_COLON: return ":";
        case LITTOK_BAR_EQUAL: return "|=";
        case LITTOK_BAR: return "|";
        case LITTOK_BAR_BAR: return "||";
        case LITTOK_AMPERSAND_EQUAL: return "&=";
        case LITTOK_AMPERSAND: return "&";
        case LITTOK_AMPERSAND_AMPERSAND: return "&&";
        case LITTOK_BANG: return "!";
        case LITTOK_BANG_EQUAL: return "!=";
        case LITTOK_EQUAL: return "=";
        case LITTOK_EQUAL_EQUAL: return "==";
        case LITTOK_GREATER: return ">";
        case LITTOK_GREATER_EQUAL: return ">=";
        case LITTOK_GREATER_GREATER: return ">>";
        case LITTOK_LESS: return "<";
        case LITTOK_LESS_EQUAL: return "<=";
        case LITTOK_LESS_LESS: return "<<";
        case LITTOK_PLUS: return "+";
        case LITTOK_PLUS_EQUAL: return "+=";
        case LITTOK_PLUS_PLUS: return "++";
        case LITTOK_MINUS: return "-";
        case LITTOK_MINUS_EQUAL: return "-=";
        case LITTOK_MINUS_MINUS: return "--";
        case LITTOK_STAR: return "*";
        case LITTOK_STAR_EQUAL: return "*=";
        case LITTOK_STAR_STAR: return "**";
        case LITTOK_SLASH: return "/";
        case LITTOK_SLASH_EQUAL: return "/=";
        case LITTOK_QUESTION: return "?";
        case LITTOK_QUESTION_QUESTION: return "??";
        case LITTOK_PERCENT: return "%";
        case LITTOK_PERCENT_EQUAL: return "%=";
        case LITTOK_ARROW: return "=>";
        case LITTOK_SMALL_ARROW: return "->";
        case LITTOK_TILDE: return "~";
        case LITTOK_CARET: return "^";
        case LITTOK_CARET_EQUAL: return "^=";
        case LITTOK_DOT: return ".";
        case LITTOK_DOT_DOT: return "..";
        case LITTOK_DOT_DOT_DOT: return "...";
        case LITTOK_SHARP: return "#";
        case LITTOK_SHARP_EQUAL: return "#=";
        case LITTOK_IDENTIFIER: return "IDENTIFIER";
        case LITTOK_STRING: return "STRING";
        case LITTOK_INTERPOLATION: return "INTERPOLATION";
        case LITTOK_NUMBER: return "NUMBER";
        case LITTOK_CLASS: return "CLASS";
        case LITTOK_ELSE: return "ELSE";
        case LITTOK_FALSE: return "FALSE";
        case LITTOK_FOR: return "FOR";
        case LITTOK_FUNCTION: return "FUNCTION";
        case LITTOK_IF: return "IF";
        case LITTOK_NULL: return "NULL";
        case LITTOK_RETURN: return "RETURN";
        case LITTOK_SUPER: return "SUPER";
        case LITTOK_THIS: return "THIS";
        case LITTOK_TRUE: return "TRUE";
        case LITTOK_VAR: return "VAR";
        case LITTOK_WHILE: return "WHILE";
        case LITTOK_CONTINUE: return "CONTINUE";
        case LITTOK_BREAK: return "BREAK";
        case LITTOK_NEW: return "NEW";
        case LITTOK_EXPORT: return "EXPORT";
        case LITTOK_IS: return "IS";
        case LITTOK_STATIC: return "STATIC";
        case LITTOK_OPERATOR: return "OPERATOR";
        case LITTOK_GET: return "GET";
        case LITTOK_SET: return "SET";
        case LITTOK_IN: return "IN";
        case LITTOK_CONST: return "CONST";
        case LITTOK_REF: return "REF";
        case LITTOK_ERROR: return "ERROR";
        case LITTOK_EOF: return "EOF";
        default:
            break;
    }
    return "unknown";
}

#define as_type(varname, fromname, tname) \
    tname* varname = (tname*)fromname

void lit_towriter_expr(LitState* state, LitWriter* wr, LitExpression* expr)
{
    size_t i;
    if(expr == NULL)
    {
        return;
    }
    //fprintf(stderr, "dumping expression type %d %s:", expr->type, lit_tostring_exprtype(expr->type));
    
    switch(expr->type)
    {
        case LITEXPR_LITERAL:
            {
                as_type(exlit, expr, LitLiteralExpr);
                lit_towriter_value(state, wr, exlit->value);
            }
            break;
        case LITEXPR_BINARY:
            {
                as_type(exbin, expr, LitBinaryExpr);
                if(exbin->ignore_left)
                {
                    lit_towriter_expr(state, wr, exbin->left);
                }
                lit_writer_writestring(wr, lit_tostring_optok(exbin->op));
                lit_towriter_expr(state, wr, exbin->right);
            }
            break;
        case LITEXPR_UNARY:
            {
                as_type(exun, expr, LitUnaryExpr);
                lit_writer_writestring(wr, lit_tostring_optok(exun->op));
                lit_towriter_expr(state, wr, exun->right);
                /*
                if(exun->op == LITTOK_SLASH_SLASH)
                {
                    lit_writer_writestring(wr, "\n");
                }
                */
            }
            break;
        case LITEXPR_VAREXPR:
            {
                as_type(exvarex, expr, LitVarExpr);
                lit_writer_writeformat(wr, "%.*s", exvarex->length, exvarex->name);
            }
            break;
        case LITEXPR_ASSIGN:
            {
                as_type(exassign, expr, LitAssignExpr);
                lit_towriter_expr(state, wr, exassign->to);
                lit_writer_writestring(wr, " = ");
                lit_towriter_expr(state, wr, exassign->value);
                lit_writer_writestring(wr, "\n");
            }
            break;
        case LITEXPR_CALL:
            {
                as_type(excall, expr, LitCallExpr);
                lit_towriter_expr(state, wr, excall->callee);
                lit_writer_writestring(wr, "(");
                for(i=0; i<excall->args.count; i++)
                {
                    lit_towriter_expr(state, wr, excall->args.values[i]);
                    if((i+1) < excall->args.count)
                    {
                        lit_writer_writestring(wr, ", ");
                    }
                }
                lit_writer_writestring(wr, ")");
            }
            break;
        case LITEXPR_SET:
            {
                as_type(exset, expr, LitSetExpr);
                lit_towriter_expr(state, wr, exset->where);
                lit_writer_writeformat(wr, ".%s = ", exset->name);
                lit_towriter_expr(state, wr, exset->value);
            }
            break;
        case LITEXPR_GET:
            {
                as_type(exget, expr, LitGetExpr);
                lit_towriter_expr(state, wr, exget->where);
                lit_writer_writeformat(wr, ".%s", exget->name);
            }
            break;
        case LITEXPR_LAMBDA:
            {
                as_type(exlam, expr, LitLambdaExpr);
                lit_writer_writeformat(wr, "(");
                for(i=0; i<exlam->parameters.count; i++)
                {
                    lit_writer_writeformat(wr, "%s", exlam->parameters.values[i].name);
                    if(exlam->parameters.values[i].default_value != NULL)
                    {
                        lit_towriter_expr(state, wr, exlam->parameters.values[i].default_value);
                    }
                    if((i+1) < exlam->parameters.count)
                    {
                        lit_writer_writeformat(wr, ", ");
                    }
                }
                lit_writer_writeformat(wr, ")");
                lit_towriter_expr(state, wr, exlam->body);

            }
            break;
        case LITEXPR_ARRAY:
            {
                as_type(exarr, expr, LitArrayExpr);
                lit_writer_writeformat(wr, "[");
                for(i=0; i<exarr->values.count; i++)
                {
                    lit_towriter_expr(state, wr, exarr->values.values[i]);
                    if((i+1) < exarr->values.count)
                    {
                        lit_writer_writeformat(wr, ", ");
                    }
                }
                lit_writer_writeformat(wr, "]");
            }
            break;
        case LITEXPR_OBJECT:
            {
                as_type(exobj, expr, LitObjectExpr);
                lit_writer_writeformat(wr, "{");
                for(i=0; i<lit_vallist_count(&exobj->keys); i++)
                {
                    lit_towriter_value(state, wr, lit_vallist_get(&exobj->keys, i));
                    lit_writer_writeformat(wr, ": ");
                    lit_towriter_expr(state, wr, exobj->values.values[i]);
                    if((i+1) < lit_vallist_count(&exobj->keys))
                    {
                        lit_writer_writeformat(wr, ", ");
                    }
                }
                lit_writer_writeformat(wr, "}");
            }
            break;
        case LITEXPR_SUBSCRIPT:
            {
                as_type(exsub, expr, LitSubscriptExpr);
                lit_towriter_expr(state, wr, exsub->array);
                lit_writer_writestring(wr, "[");
                lit_towriter_expr(state, wr, exsub->index);
                lit_writer_writestring(wr, "]");
            }
            break;
        case LITEXPR_THIS:
            {
                lit_writer_writestring(wr, "this");
            }
            break;
        case LITEXPR_SUPER:
            {
                as_type(exsuper, expr, LitSuperExpr);
                lit_writer_writeformat(wr, "super(%s)", exsuper->method->chars);
            }
            break;
        case LITEXPR_RANGE:
            {
                as_type(exrange, expr, LitRangeExpr);
                lit_writer_writestring(wr, "[");
                lit_towriter_expr(state, wr, exrange->from);
                lit_writer_writestring(wr, " .. ");
                lit_towriter_expr(state, wr, exrange->to);
                lit_writer_writestring(wr, "]");
            }
            break;
        case LITEXPR_TERNARY:
            {
                as_type(exif, expr, LitTernaryExpr);
                lit_writer_writestring(wr, "if(");
                lit_towriter_expr(state, wr, exif->condition);
                lit_writer_writestring(wr, ")");
                lit_towriter_expr(state, wr, exif->if_branch);
                if(exif->else_branch != NULL)
                {
                    lit_writer_writestring(wr, "else ");
                    lit_towriter_expr(state, wr, exif->else_branch);
                }
            }
            break;
        case LITEXPR_INTERPOLATION:
            {
                as_type(exint, expr, LitInterpolationExpr);
                lit_writer_writestring(wr, "\"\"+");
                for(i=0; i<exint->expressions.count; i++)
                {
                    lit_writer_writestring(wr, "(");
                    lit_towriter_expr(state, wr, exint->expressions.values[i]);
                    lit_writer_writestring(wr, ")");
                    if((i+1) < exint->expressions.count)
                    {
                        lit_writer_writestring(wr, "+");
                    }
                }
                lit_writer_writestring(wr, "+\"\"");
            }
            break;
        case LITEXPR_REFERENCE:
            {
            
            }
            break;
        case LITEXPR_EXPRESSION:
            {
                as_type(exexpr, expr, LitExpressionExpr);
                lit_towriter_expr(state, wr, exexpr->expression);
            }
            break;
        case LITEXPR_BLOCK:
            {
                as_type(exblock, expr, LitBlockExpr);
                lit_writer_writestring(wr, "{");
                for(i=0; i<exblock->statements.count; i++)
                {
                    lit_towriter_expr(state, wr, exblock->statements.values[i]);
                }
                lit_writer_writestring(wr, "}");
            }
            break;
        case LITEXPR_IFSTMT:
            {
            
            }
            break;
        case LITEXPR_WHILE:
            {
                as_type(wl, expr, LitWhileExpr);
                lit_writer_writeformat(wr, "while(");
                lit_towriter_expr(state, wr, wl->condition);
                lit_writer_writeformat(wr, ")");
                lit_towriter_expr(state, wr, wl->body);
            }
            break;
        case LITEXPR_FOR:
            {
            
            }
            break;
        case LITEXPR_VARSTMT:
            {
            
            }
            break;
        case LITEXPR_CONTINUE:
            {
            
            }
            break;
        case LITEXPR_BREAK:
            {
            
            }
            break;
        case LITEXPR_FUNCTION:
            {
            
            }
            break;
        case LITEXPR_RETURN:
            {
            
            }
            break;
        case LITEXPR_METHOD:
            {
            
            }
            break;
        case LITEXPR_CLASS:
            {
            
            }
            break;
        case LITEXPR_FIELD:
            {
            
            }
            break;
        default:
            {
                lit_writer_writeformat(wr, "(unhandled expression type %d %s)", expr->type, lit_tostring_exprtype(expr->type));
            }
            break;
    }
}

void lit_towriter_ast(LitState* state, LitWriter* wr, LitExprList* exlist)
{
    size_t i;
    lit_writer_writeformat(wr, "begin AST dump (list of %d expressions):\n", exlist->count);
    for(i=0; i<exlist->count; i++)
    {
        lit_towriter_expr(state, wr, exlist->values[i]);
    }
    lit_writer_writeformat(wr, "\nend AST dump\n");
}
