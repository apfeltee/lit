
enum LitOpCode
{
#define OPCODE(name, effect) OP_##name,
#include "opcodes.inc"
#undef OPCODE
};

enum LitExprType
{
    LITEXPR_LITERAL,
    LITEXPR_BINARY,
    LITEXPR_UNARY,
    LITEXPR_VAREXPR,
    LITEXPR_ASSIGN,
    LITEXPR_CALL,
    LITEXPR_SET,
    LITEXPR_GET,
    LITEXPR_LAMBDA,
    LITEXPR_ARRAY,
    LITEXPR_OBJECT,
    LITEXPR_SUBSCRIPT,
    LITEXPR_THIS,
    LITEXPR_SUPER,
    LITEXPR_RANGE,
    LITEXPR_TERNARY,
    LITEXPR_INTERPOLATION,
    LITEXPR_REFERENCE,

    LITEXPR_EXPRESSION,
    LITEXPR_BLOCK,
    LITEXPR_IFSTMT,
    LITEXPR_WHILE,
    LITEXPR_FOR,
    LITEXPR_VARSTMT,
    LITEXPR_CONTINUE,
    LITEXPR_BREAK,
    LITEXPR_FUNCTION,
    LITEXPR_RETURN,
    LITEXPR_METHOD,
    LITEXPR_CLASS,
    LITEXPR_FIELD
};

enum LitOptLevel
{
    LITOPTLEVEL_NONE,
    LITOPTLEVEL_REPL,
    LITOPTLEVEL_DEBUG,
    LITOPTLEVEL_RELEASE,
    LITOPTLEVEL_EXTREME,

    LITOPTLEVEL_TOTAL
};

enum LitOptimization
{
    LITOPTSTATE_CONSTANT_FOLDING,
    LITOPTSTATE_LITERAL_FOLDING,
    LITOPTSTATE_UNUSED_VAR,
    LITOPTSTATE_UNREACHABLE_CODE,
    LITOPTSTATE_EMPTY_BODY,
    LITOPTSTATE_LINE_INFO,
    LITOPTSTATE_PRIVATE_NAMES,
    LITOPTSTATE_C_FOR,

    LITOPTSTATE_TOTAL
};

enum LitError
{
    // Preprocessor errors
    LITERROR_UNCLOSED_MACRO,
    LITERROR_UNKNOWN_MACRO,

    // Scanner errors
    LITERROR_UNEXPECTED_CHAR,
    LITERROR_UNTERMINATED_STRING,
    LITERROR_INVALID_ESCAPE_CHAR,
    LITERROR_INTERPOLATION_NESTING_TOO_DEEP,
    LITERROR_NUMBER_IS_TOO_BIG,
    LITERROR_CHAR_EXPECTATION_UNMET,

    // Parser errors
    LITERROR_EXPECTATION_UNMET,
    LITERROR_INVALID_ASSIGMENT_TARGET,
    LITERROR_TOO_MANY_FUNCTION_ARGS,
    LITERROR_MULTIPLE_ELSE_BRANCHES,
    LITERROR_VAR_MISSING_IN_FORIN,
    LITERROR_NO_GETTER_AND_SETTER,
    LITERROR_STATIC_OPERATOR,
    LITERROR_SELF_INHERITED_CLASS,
    LITERROR_STATIC_FIELDS_AFTER_METHODS,
    LITERROR_MISSING_STATEMENT,
    LITERROR_EXPECTED_EXPRESSION,
    LITERROR_DEFAULT_ARG_CENTRED,

    // Emitter errors
    LITERROR_TOO_MANY_CONSTANTS,
    LITERROR_TOO_MANY_PRIVATES,
    LITERROR_VAR_REDEFINED,
    LITERROR_TOO_MANY_LOCALS,
    LITERROR_TOO_MANY_UPVALUES,
    LITERROR_VARIABLE_USED_IN_INIT,
    LITERROR_JUMP_TOO_BIG,
    LITERROR_NO_SUPER,
    LITERROR_THIS_MISSUSE,
    LITERROR_SUPER_MISSUSE,
    LITERROR_UNKNOWN_EXPRESSION,
    LITERROR_UNKNOWN_STATEMENT,
    LITERROR_LOOP_JUMP_MISSUSE,
    LITERROR_RETURN_FROM_CONSTRUCTOR,
    LITERROR_STATIC_CONSTRUCTOR,
    LITERROR_CONSTANT_MODIFIED,
    LITERROR_INVALID_REFERENCE_TARGET,

    LITERROR_TOTAL
};

enum LitPrecedence
{
    LITPREC_NONE,
    LITPREC_ASSIGNMENT,// =
    LITPREC_OR,// ||
    LITPREC_AND,// &&
    LITPREC_BOR,// | ^
    LITPREC_BAND,// &
    LITPREC_SHIFT,// << >>
    LITPREC_EQUALITY,// == !=
    LITPREC_COMPARISON,// < > <= >=
    LITPREC_COMPOUND,// += -= *= /= ++ --
    LITPREC_TERM,// + -
    LITPREC_FACTOR,// * /
    LITPREC_IS,// is
    LITPREC_RANGE,// ..
    LITPREC_UNARY,// ! - ~
    LITPREC_NULL,// ??
    LITPREC_CALL,// . ()
    LITPREC_PRIMARY
};

enum LitTokType
{
    LITTOK_NEW_LINE,

    // Single-character tokens.
    LITTOK_LEFT_PAREN,
    LITTOK_RIGHT_PAREN,
    LITTOK_LEFT_BRACE,
    LITTOK_RIGHT_BRACE,
    LITTOK_LEFT_BRACKET,
    LITTOK_RIGHT_BRACKET,
    LITTOK_COMMA,
    LITTOK_SEMICOLON,
    LITTOK_COLON,

    // One or two character tokens.
    LITTOK_BAR_EQUAL,
    LITTOK_BAR,
    LITTOK_BAR_BAR,
    LITTOK_AMPERSAND_EQUAL,
    LITTOK_AMPERSAND,
    LITTOK_AMPERSAND_AMPERSAND,
    LITTOK_BANG,
    LITTOK_BANG_EQUAL,
    LITTOK_EQUAL,
    LITTOK_EQUAL_EQUAL,
    LITTOK_GREATER,
    LITTOK_GREATER_EQUAL,
    LITTOK_GREATER_GREATER,
    LITTOK_LESS,
    LITTOK_LESS_EQUAL,
    LITTOK_LESS_LESS,
    LITTOK_PLUS,
    LITTOK_PLUS_EQUAL,
    LITTOK_PLUS_PLUS,
    LITTOK_MINUS,
    LITTOK_MINUS_EQUAL,
    LITTOK_MINUS_MINUS,
    LITTOK_STAR,
    LITTOK_STAR_EQUAL,
    LITTOK_STAR_STAR,
    LITTOK_SLASH,
    LITTOK_SLASH_EQUAL,
    LITTOK_QUESTION,
    LITTOK_QUESTION_QUESTION,
    LITTOK_PERCENT,
    LITTOK_PERCENT_EQUAL,
    LITTOK_ARROW,
    LITTOK_SMALL_ARROW,
    LITTOK_TILDE,
    LITTOK_CARET,
    LITTOK_CARET_EQUAL,
    LITTOK_DOT,
    LITTOK_DOT_DOT,
    LITTOK_DOT_DOT_DOT,
    LITTOK_SHARP,
    LITTOK_SHARP_EQUAL,

    // Literals.
    LITTOK_IDENTIFIER,
    LITTOK_STRING,
    LITTOK_INTERPOLATION,
    LITTOK_NUMBER,

    // Keywords.
    LITTOK_CLASS,
    LITTOK_ELSE,
    LITTOK_FALSE,
    LITTOK_FOR,
    LITTOK_FUNCTION,
    LITTOK_IF,
    LITTOK_NULL,
    LITTOK_RETURN,
    LITTOK_SUPER,
    LITTOK_THIS,
    LITTOK_TRUE,
    LITTOK_VAR,
    LITTOK_WHILE,
    LITTOK_CONTINUE,
    LITTOK_BREAK,
    LITTOK_NEW,
    LITTOK_EXPORT,
    LITTOK_IS,
    LITTOK_STATIC,
    LITTOK_OPERATOR,
    LITTOK_GET,
    LITTOK_SET,
    LITTOK_IN,
    LITTOK_CONST,
    LITTOK_REF,

    LITTOK_ERROR,
    LITTOK_EOF
};

enum LitResult
{
    LITRESULT_OK,
    LITRESULT_COMPILE_ERROR,
    LITRESULT_RUNTIME_ERROR,
    LITRESULT_INVALID
};

enum LitErrType
{
    COMPILE_ERROR,
    RUNTIME_ERROR
};

enum LitFuncType
{
    LITFUNC_REGULAR,
    LITFUNC_SCRIPT,
    LITFUNC_METHOD,
    LITFUNC_STATIC_METHOD,
    LITFUNC_CONSTRUCTOR
};

enum LitObjType
{
    LITTYPE_UNDEFINED,
    LITTYPE_NULL,
    LITTYPE_STRING,
    LITTYPE_FUNCTION,
    LITTYPE_NATIVE_FUNCTION,
    LITTYPE_NATIVE_PRIMITIVE,
    LITTYPE_NATIVE_METHOD,
    LITTYPE_PRIMITIVE_METHOD,
    LITTYPE_FIBER,
    LITTYPE_MODULE,
    LITTYPE_CLOSURE,
    LITTYPE_UPVALUE,
    LITTYPE_CLASS,
    LITTYPE_INSTANCE,
    LITTYPE_BOUND_METHOD,
    LITTYPE_ARRAY,
    LITTYPE_MAP,
    LITTYPE_USERDATA,
    LITTYPE_RANGE,
    LITTYPE_FIELD,
    LITTYPE_REFERENCE,
    LITTYPE_NUMBER,
    LITTYPE_BOOL,
};

typedef enum /**/LitOpCode LitOpCode;
typedef enum /**/LitExprType LitExprType;
typedef enum /**/LitOptLevel LitOptLevel;
typedef enum /**/LitOptimization LitOptimization;
typedef enum /**/LitError LitError;
typedef enum /**/LitPrecedence LitPrecedence;
typedef enum /**/LitTokType LitTokType;
typedef enum /**/LitResult LitResult;
typedef enum /**/LitErrType LitErrType;
typedef enum /**/LitFuncType LitFuncType;
typedef enum /**/LitObjType LitObjType;
typedef struct /**/LitScanner LitScanner;
typedef struct /**/LitPreprocessor LitPreprocessor;
typedef struct /**/LitExecState LitExecState;
typedef struct /**/LitVM LitVM;
typedef struct /**/LitParser LitParser;
typedef struct /**/LitEmitter LitEmitter;
typedef struct /**/LitOptimizer LitOptimizer;
typedef struct /**/LitState LitState;
typedef struct /**/LitInterpretResult LitInterpretResult;
typedef struct /**/LitMap LitMap;
typedef struct /**/LitNumber LitNumber;
typedef struct /**/LitString LitString;
typedef struct /**/LitModule LitModule;
typedef struct /**/LitFiber LitFiber;
typedef struct /**/LitUserdata LitUserdata;
typedef struct /**/LitChunk LitChunk;
typedef struct /**/LitTableEntry LitTableEntry;
typedef struct /**/LitTable LitTable;
typedef struct /**/LitFunction LitFunction;
typedef struct /**/LitUpvalue LitUpvalue;
typedef struct /**/LitClosure LitClosure;
typedef struct /**/LitNativeFunction LitNativeFunction;
typedef struct /**/LitNativePrimFunction LitNativePrimFunction;
typedef struct /**/LitNativeMethod LitNativeMethod;
typedef struct /**/LitPrimitiveMethod LitPrimitiveMethod;
typedef struct /**/LitCallFrame LitCallFrame;
typedef struct /**/LitClass LitClass;
typedef struct /**/LitInstance LitInstance;
typedef struct /**/LitBoundMethod LitBoundMethod;
typedef struct /**/LitArray LitArray;
typedef struct /**/LitRange LitRange;
typedef struct /**/LitField LitField;
typedef struct /**/LitReference LitReference;
typedef struct /**/LitToken LitToken;
typedef struct /**/LitAstExpression LitAstExpression;
typedef struct /**/LitCompilerUpvalue LitCompilerUpvalue;
typedef struct /**/LitCompiler LitCompiler;
typedef struct /**/LitParseRule LitParseRule;
typedef struct /**/LitEmulatedFile LitEmulatedFile;
typedef struct /**/LitVariable LitVariable;
typedef struct /**/LitWriter LitWriter;
typedef struct /**/LitLocal LitLocal;
typedef struct /**/LitConfig LitConfig;

/* ARRAYTYPES */
typedef struct /**/LitVarList LitVarList;
typedef struct /**/LitUintList LitUintList;
typedef struct /**/LitValueList LitValueList;
typedef struct /**/LitAstExprList LitAstExprList;
typedef struct /**/LitAstParamList LitAstParamList;
typedef struct /**/LitPrivList LitPrivList;
typedef struct /**/LitLocList LitLocList;
typedef struct /**/LitDataList LitDataList;
typedef struct /**/LitByteList LitByteList;

/* ast/compiler types */
typedef struct /**/LitAstLiteralExpr LitAstLiteralExpr;
typedef struct /**/LitAstBinaryExpr LitAstBinaryExpr;
typedef struct /**/LitAstUnaryExpr LitAstUnaryExpr;
typedef struct /**/LitAstVarExpr LitAstVarExpr;
typedef struct /**/LitAstAssignExpr LitAstAssignExpr;
typedef struct /**/LitAstCallExpr LitAstCallExpr;
typedef struct /**/LitAstGetExpr LitAstGetExpr;
typedef struct /**/LitAstSetExpr LitAstSetExpr;
typedef struct /**/LitAstParameter LitAstParameter;
typedef struct /**/LitAstLambdaExpr LitAstLambdaExpr;
typedef struct /**/LitAstArrayExpr LitAstArrayExpr;
typedef struct /**/LitAstObjectExpr LitAstObjectExpr;
typedef struct /**/LitAstIndexExpr LitAstIndexExpr;
typedef struct /**/LitAstThisExpr LitAstThisExpr;
typedef struct /**/LitAstSuperExpr LitAstSuperExpr;
typedef struct /**/LitAstRangeExpr LitAstRangeExpr;
typedef struct /**/LitAstTernaryExpr LitAstTernaryExpr;
typedef struct /**/LitAstStrInterExpr LitAstStrInterExpr;
typedef struct /**/LitAstRefExpr LitAstRefExpr;
typedef struct /**/LitAstExprExpr LitAstExprExpr;
typedef struct /**/LitAstBlockExpr LitAstBlockExpr;
typedef struct /**/LitAstAssignVarExpr LitAstAssignVarExpr;
typedef struct /**/LitAstIfExpr LitAstIfExpr;
typedef struct /**/LitAstWhileExpr LitAstWhileExpr;
typedef struct /**/LitAstForExpr LitAstForExpr;
typedef struct /**/LitAstContinueExpr LitAstContinueExpr;
typedef struct /**/LitAstBreakExpr LitAstBreakExpr;
typedef struct /**/LitAstFunctionExpr LitAstFunctionExpr;
typedef struct /**/LitAstReturnExpr LitAstReturnExpr;
typedef struct /**/LitAstMethodExpr LitAstMethodExpr;
typedef struct /**/LitAstClassExpr LitAstClassExpr;
typedef struct /**/LitAstFieldExpr LitAstFieldExpr;
typedef struct /**/LitPrivate LitPrivate;

/* forward decls to make prot.inc work */
typedef struct /**/LitDirReader LitDirReader;
typedef struct /**/LitDirItem LitDirItem;



typedef LitAstExpression* (*LitPrefixParseFn)(LitParser*, bool);
typedef LitAstExpression* (*LitInfixParseFn)(LitParser*, LitAstExpression*, bool);


typedef LitValue (*LitNativeFunctionFn)(LitVM*, size_t, LitValue*);
typedef bool (*LitNativePrimitiveFn)(LitVM*, size_t, LitValue*);
typedef LitValue (*LitNativeMethodFn)(LitVM*, LitValue, size_t arg_count, LitValue*);
typedef bool (*LitPrimitiveMethodFn)(LitVM*, LitValue, size_t, LitValue*);
typedef LitValue (*LitMapIndexFn)(LitVM*, LitMap*, LitString*, LitValue*);
typedef void (*LitCleanupFn)(LitState*, LitUserdata*, bool mark);
typedef void (*LitErrorFn)(LitState*, const char*);
typedef void (*LitPrintFn)(LitState*, const char*);

typedef void(*LitWriterByteFN)(LitWriter*, int);
typedef void(*LitWriterStringFN)(LitWriter*, const char*, size_t);
typedef void(*LitWriterFormatFN)(LitWriter*, const char*, va_list);

struct LitObject
{
    /* the type of this object */
    LitObjType type;
    LitObject* next;
    bool marked;
    bool mustfree;

    union
    {
        bool boolval;
    };
};



struct LitDataList
{
    /* how many values *could* this list hold? */
    size_t capacity;

    /* actual amount of values in this list */
    size_t count;

    size_t rawelemsz;
    size_t elemsz;

    /* the actual values */
    intptr_t* values;
};


struct LitVarList
{
    size_t capacity;
    size_t count;
    LitVariable* values;
};


struct LitUintList
{
    LitDataList list;
};

struct LitValueList
{
    LitDataList list;
};

/* TODO: using DataList messes with the string its supposed to collect. no clue why, though. */
struct LitByteList
{
    size_t capacity;
    size_t count;
    uint8_t* values;
};

struct LitWriter
{
    LitState* state;

    /* the main pointer, that either holds a pointer to a LitString, or a FILE */
    void* uptr;

    /* if true, then uptr is a LitString, otherwise it's a FILE */
    bool stringmode;

    /* if true, and !stringmode, then calls fflush() after each i/o operations */
    bool forceflush;

    /* the callback that emits a single character */
    LitWriterByteFN fnbyte;

    /* the callback that emits a string */
    LitWriterStringFN fnstring;

    /* the callback that emits a format string (printf-style) */
    LitWriterFormatFN fnformat;
};


struct LitAstExpression
{
    LitExprType type;
    size_t line;
};

struct LitPrivate
{
    bool initialized;
    bool constant;
};

struct LitPrivList
{
    size_t capacity;
    size_t count;
    LitPrivate* values;
};

struct LitEmitter
{
    LitState* state;
    LitChunk* chunk;
    LitCompiler* compiler;
    size_t last_line;
    size_t loop_start;
    LitPrivList privates;
    LitUintList breaks;
    LitUintList continues;
    LitModule* module;
    LitString* class_name;
    bool class_has_super;
    bool previous_was_expression_statement;
    int emit_reference;
};

struct LitParseRule
{
    LitPrefixParseFn prefix;
    LitInfixParseFn infix;
    LitPrecedence precedence;
};

/*
 * Expressions
 */
struct LitAstExprList
{
    size_t capacity;
    size_t count;
    LitAstExpression** values;
};

struct LitAstLiteralExpr
{
    LitAstExpression exobj;
    LitValue value;
};

struct LitAstBinaryExpr
{
    LitAstExpression exobj;
    LitAstExpression* left;
    LitAstExpression* right;
    LitTokType op;
    bool ignore_left;
};

struct LitAstUnaryExpr
{
    LitAstExpression exobj;
    LitAstExpression* right;
    LitTokType op;
};

struct LitAstVarExpr
{
    LitAstExpression exobj;
    const char* name;
    size_t length;
};

struct LitAstAssignExpr
{
    LitAstExpression exobj;
    LitAstExpression* to;
    LitAstExpression* value;
};

struct LitAstCallExpr
{
    LitAstExpression exobj;
    LitAstExpression* callee;
    LitAstExprList args;
    LitAstExpression* init;
};

struct LitAstGetExpr
{
    LitAstExpression exobj;
    LitAstExpression* where;
    const char* name;
    size_t length;
    int jump;
    bool ignore_emit;
    bool ignore_result;
};

struct LitAstSetExpr
{
    LitAstExpression exobj;
    LitAstExpression* where;
    const char* name;
    size_t length;
    LitAstExpression* value;
};

struct LitAstParameter
{
    const char* name;
    size_t length;
    LitAstExpression* default_value;
};

struct LitAstParamList
{
    size_t capacity;
    size_t count;
    LitAstParameter* values;
};


struct LitAstLambdaExpr
{
    LitAstExpression exobj;
    LitAstParamList parameters;
    LitAstExpression* body;
};

struct LitAstArrayExpr
{
    LitAstExpression exobj;
    LitAstExprList values;
};

struct LitAstObjectExpr
{
    LitAstExpression exobj;
    LitValueList keys;
    LitAstExprList values;
};

struct LitAstIndexExpr
{
    LitAstExpression exobj;
    LitAstExpression* array;
    LitAstExpression* index;
};

struct LitAstThisExpr
{
    LitAstExpression exobj;
};

struct LitAstSuperExpr
{
    LitAstExpression exobj;
    LitString* method;
    bool ignore_emit;
    bool ignore_result;
};

struct LitAstRangeExpr
{
    LitAstExpression exobj;
    LitAstExpression* from;
    LitAstExpression* to;
};

struct LitAstTernaryExpr
{
    LitAstExpression exobj;
    LitAstExpression* condition;
    LitAstExpression* if_branch;
    LitAstExpression* else_branch;
};

struct LitAstStrInterExpr
{
    LitAstExpression exobj;
    LitAstExprList expressions;
};

struct LitAstRefExpr
{
    LitAstExpression exobj;
    LitAstExpression* to;
};

/*
 * Statements
 */

struct LitAstExprExpr
{
    LitAstExpression exobj;
    LitAstExpression* expression;
    bool pop;
};

struct LitAstBlockExpr
{
    LitAstExpression exobj;
    LitAstExprList statements;
};

struct LitAstAssignVarExpr
{
    LitAstExpression exobj;
    const char* name;
    size_t length;
    bool constant;
    LitAstExpression* init;
};

struct LitAstIfExpr
{
    LitAstExpression exobj;
    LitAstExpression* condition;
    LitAstExpression* if_branch;
    LitAstExpression* else_branch;
    LitAstExprList* elseif_conditions;
    LitAstExprList* elseif_branches;
};

struct LitAstWhileExpr
{
    LitAstExpression exobj;
    LitAstExpression* condition;
    LitAstExpression* body;
};

struct LitAstForExpr
{
    LitAstExpression exobj;
    LitAstExpression* init;
    LitAstExpression* var;
    LitAstExpression* condition;
    LitAstExpression* increment;
    LitAstExpression* body;
    bool c_style;
};

struct LitAstContinueExpr
{
    LitAstExpression exobj;
};

struct LitAstBreakExpr
{
    LitAstExpression exobj;
};

struct LitAstFunctionExpr
{
    LitAstExpression exobj;
    const char* name;
    size_t length;
    LitAstParamList parameters;
    LitAstExpression* body;
    bool exported;
};

struct LitAstReturnExpr
{
    LitAstExpression exobj;
    LitAstExpression* expression;
};

struct LitAstMethodExpr
{
    LitAstExpression exobj;
    LitString* name;
    LitAstParamList parameters;
    LitAstExpression* body;
    bool is_static;
};

struct LitAstClassExpr
{
    LitAstExpression exobj;
    LitString* name;
    LitString* parent;
    LitAstExprList fields;
};

struct LitAstFieldExpr
{
    LitAstExpression exobj;
    LitString* name;
    LitAstExpression* getter;
    LitAstExpression* setter;
    bool is_static;
};

struct LitVariable
{
    const char* name;
    size_t length;
    int depth;
    bool constant;
    bool used;
    LitValue constant_value;
    LitAstExpression** declaration;
};


struct LitChunk
{
    /* how many items this chunk holds */
    size_t count;
    size_t capacity;
    uint8_t* code;
    bool has_line_info;
    size_t line_count;
    size_t line_capacity;
    uint16_t* lines;
    LitValueList constants;
};

struct LitTableEntry
{
    /* the key of this entry. can be NULL! */
    LitString* key;

    /* the associated value */
    LitValue value;
};

struct LitTable
{
    LitState* state;

    /* how many entries are in this table */
    int count;

    /* how many entries could be held */
    int capacity;

    /* the actual entries */
    LitTableEntry* entries;
};


struct LitNumber
{
    LitObject object;
    double num;
};

struct LitString
{
    LitObject object;
    /* the hash of this string - note that it is only unique to the context! */
    uint32_t hash;
    /* this is handled by sds - use lit_string_getlength to get the length! */
    char* chars;
};

struct LitFunction
{
    LitObject object;
    LitChunk chunk;
    LitString* name;
    uint8_t arg_count;
    uint16_t upvalue_count;
    size_t max_slots;
    bool vararg;
    LitModule* module;
};

struct LitUpvalue
{
    LitObject object;
    LitValue* location;
    LitValue closed;
    LitUpvalue* next;
};

struct LitClosure
{
    LitObject object;
    LitFunction* function;
    LitUpvalue** upvalues;
    size_t upvalue_count;
};

struct LitNativeFunction
{
    LitObject object;
    /* the native callback for this function */
    LitNativeFunctionFn function;
    /* the name of this function */
    LitString* name;
};

struct LitNativePrimFunction
{
    LitObject object;
    LitNativePrimitiveFn function;
    LitString* name;
};

struct LitNativeMethod
{
    LitObject object;
    LitNativeMethodFn method;
    LitString* name;
};

struct LitPrimitiveMethod
{
    LitObject object;
    LitPrimitiveMethodFn method;
    LitString* name;
};

struct LitCallFrame
{
    LitFunction* function;
    LitClosure* closure;
    uint8_t* ip;
    LitValue* slots;
    bool result_ignored;
    bool return_to_c;
};

struct LitMap
{
    LitObject object;
    /* the table that holds the actual entries */
    LitTable values;
    /* the index function corresponding to operator[] */
    LitMapIndexFn index_fn;
};

struct LitModule
{
    LitObject object;
    LitValue return_value;
    LitString* name;
    LitValue* privates;
    LitMap* private_names;
    size_t private_count;
    LitFunction* main_function;
    LitFiber* main_fiber;
    bool ran;
};

struct LitFiber
{
    LitObject object;
    LitFiber* parent;
    LitValue* stack;
    LitValue* stack_top;
    size_t stack_capacity;
    LitCallFrame* frames;
    size_t frame_capacity;
    size_t frame_count;
    size_t arg_count;
    LitUpvalue* open_upvalues;
    LitModule* module;
    LitValue lit_emitter_raiseerror;
    bool abort;
    bool catcher;
};

struct LitClass
{
    LitObject object;
    /* the name of this class */
    LitString* name;
    /* the constructor object */
    LitObject* init_method;
    /* runtime methods */
    LitTable methods;
    /* static fields, which include functions, and variables */
    LitTable static_fields;
    /*
    * the parent class - the topmost is always LitClass, followed by LitObject.
    * that is, eg for LitString: LitString <- LitObject <- LitClass
    */
    LitClass* super;
};

struct LitInstance
{
    LitObject object;
    /* the class that corresponds to this instance */
    LitClass* klass;
    LitTable fields;
};

struct LitBoundMethod
{
    LitObject object;
    LitValue receiver;
    LitValue method;
};

struct LitArray
{
    LitObject object;
    LitValueList list;
};

struct LitUserdata
{
    LitObject object;
    void* data;
    size_t size;
    LitCleanupFn cleanup_fn;
    bool canfree;
};

struct LitRange
{
    LitObject object;
    double from;
    double to;
};

struct LitField
{
    LitObject object;
    LitObject* getter;
    LitObject* setter;
};

struct LitReference
{
    LitObject object;
    LitValue* slot;
};

struct LitConfig
{
    bool dumpbytecode;
    bool dumpast;
    bool runafterdump;
};

struct LitState
{
    LitConfig config;
    LitWriter stdoutwriter;
    /* how much was allocated in total? */
    int64_t bytes_allocated;
    int64_t next_gc;
    bool allow_gc;
    LitValueList lightobjects;
    LitErrorFn error_fn;
    LitPrintFn print_fn;
    LitValue* roots;
    size_t root_count;
    size_t root_capacity;
    LitPreprocessor* preprocessor;
    LitScanner* scanner;
    LitParser* parser;
    LitEmitter* emitter;
    LitOptimizer* optimizer;
    /*
    * recursive pointer to the current VM instance.
    * using 'state->vm->state' will in turn mean this instance, etc.
    */
    LitVM* vm;
    bool had_error;
    LitFunction* api_function;
    LitFiber* api_fiber;
    LitString* api_name;
    /* when using debug routines, this is the writer that output is called on */
    LitWriter debugwriter;
    // class class
    // Mental note:
    // When adding another class here, DO NOT forget to mark it in lit_mem.c or it will be GC-ed
    LitClass* classvalue_class;
    LitClass* objectvalue_class;
    LitClass* numbervalue_class;
    LitClass* stringvalue_class;
    LitClass* boolvalue_class;
    LitClass* functionvalue_class;
    LitClass* fibervalue_class;
    LitClass* modulevalue_class;
    LitClass* arrayvalue_class;
    LitClass* mapvalue_class;
    LitClass* rangevalue_class;
    LitModule* last_module;
};


struct LitExecState
{
    LitValue* slots;
    LitValue* privates;
    LitUpvalue** upvalues;
    uint8_t* ip;
    LitCallFrame* frame;
    LitChunk* current_chunk;

};

struct LitVM
{
    /* the current state */
    LitState* state;
    /* currently held objects */
    LitObject* objects;
    /* currently cached strings */
    LitTable strings;
    /* currently loaded/defined modules */
    LitMap* modules;
    /* currently defined globals */
    LitMap* globals;
    LitFiber* fiber;
    // For garbage collection
    size_t gray_count;
    size_t gray_capacity;
    LitObject** gray_stack;
};

struct LitInterpretResult
{
    /* the result of this interpret/lit_vm_callcallable attempt */
    LitResult type;
    /* the value returned from this interpret/lit_vm_callcallable attempt */
    LitValue result;
};

struct LitToken
{
    const char* start;
    LitTokType type;
    size_t length;
    size_t line;
    LitValue value;
};

struct LitLocal
{
    const char* name;
    size_t length;
    int depth;
    bool captured;
    bool constant;
};

struct LitLocList
{
    size_t capacity;
    size_t count;
    LitLocal* values;
};

struct LitCompilerUpvalue
{
    uint8_t index;
    bool isLocal;
};

struct LitCompiler
{
    LitLocList locals;
    int scope_depth;
    LitFunction* function;
    LitFuncType type;
    LitCompilerUpvalue upvalues[UINT8_COUNT];
    LitCompiler* enclosing;
    bool skip_return;
    size_t loop_depth;
    int slots;
    int max_slots;
};

struct LitParser
{
    LitState* state;
    bool had_error;
    bool panic_mode;
    LitToken previous;
    LitToken current;
    LitCompiler* compiler;
    uint8_t expression_root_count;
    uint8_t statement_root_count;
};

struct LitEmulatedFile
{
    const char* source;
    size_t length;
    size_t position;
};

struct LitScanner
{
    size_t line;
    const char* start;
    const char* current;
    const char* file_name;
    LitState* state;
    size_t braces[LIT_MAX_INTERPOLATION_NESTING];
    size_t num_braces;
    bool had_error;
};

struct LitOptimizer
{
    LitState* state;
    LitVarList variables;
    int depth;
    bool mark_used;
};

struct LitPreprocessor
{
    LitState* state;
    LitTable defined;
    /*
	 * A little bit dirty hack:
	 * We need to store pointers (8 bytes in size),
	 * and so that we don't have to declare a new
	 * array type, that we will use only once,
	 * I'm using LitValueList here, because LitValue
	 * also has the same size (8 bytes)
	 */
    LitValueList open_ifs;
};


