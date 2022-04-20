//
// cspl.c - simple programming language (spl)
//
// original c implementation
//

#include <stdio.h>  // puts, printf
#include <string.h> // strcmp
#include <stdlib.h> // malloc, fread
#include <unistd.h> // write
#include <sys/time.h> // gettimeofday
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

typedef float f32;
typedef double f64;
typedef int64_t i64;
typedef uint64_t u64;
typedef int32_t i32;
typedef uint32_t u32;
typedef int16_t i16;
typedef uint16_t u16;
typedef int8_t i8;
typedef uint8_t u8;

#define ARR_SIZE(ARR) (sizeof(ARR) / sizeof(ARR[0]))
#define KB(n) (n * 1024)
#define MB(n) (KB(n * 1024))
#define GB(n) (MB(n * 1024))

#define REAL_TIMER_START(...) \
  struct timeval _end = {0}; \
  struct timeval _start = {0}; \
  gettimeofday(&_start, NULL); \
  __VA_ARGS__

#define REAL_TIMER_END(...) { \
  gettimeofday(&_end, NULL); \
  f64 _dt = ((((_end.tv_sec - _start.tv_sec) * 1000000.0f) + _end.tv_usec) - (_start.tv_usec)) / 1000000.0f; \
  __VA_ARGS__ \
}

#define list_push(list, count, value) do { \
  if (list == NULL) { list = calloc(sizeof(value), 1); list[0] = value; count = 1; break; } \
  void* tmp = realloc(list, (1 + count) * (sizeof(value))); \
  if (tmp) { \
    list = tmp; \
    list[(count)++] = value; \
  } \
} while (0); \

#define list_realloc(list, count, new_size) { \
  assert(list != NULL); \
  void* new_list = realloc(list, (new_size) * (sizeof(*list))); \
  if (new_list != NULL) { \
    list = new_list; \
    count = new_size; \
  } \
} \

#define list_shrink(list, count, num) { \
  assert((count - num) >= 0); \
  list_realloc(list, count, count - num); \
}
 
#define list_assign(list, count, index, value) { \
  assert(list != NULL); \
  if (index < count) { \
    list[index] = value; \
  } \
} \

#define list_free(list, count) { \
  if ((list) != NULL) { \
    free(list); \
    count = 0; \
    list = NULL; \
  }\
}

#define MAX_ERR_SIZE 512
#define MAX_PATH_SIZE 512
#define USE_EXTENDED_ASCII 1
#define NUM_LINES_TO_PRINT 2
#define NO_TYPECHECKING 0
#define VERBOSE_ASSEMBLY 1
#define MAX_SYSCALL_FUNCTION 7

#define HERE printf("%s:%d: HERE\n", __FUNCTION__, __LINE__)

#define VERBOSE 1

#if VERBOSE
  #define v_printf(...) fprintf(stdout, __VA_ARGS__)
#else
  #define v_printf(...)
#endif

#define Error (-1)
#define NoError (0)

typedef enum Token_type {
  T_EOF,

  T_IDENTIFIER,
  T_NUMBER,
  T_CSTRING,
  T_ASSIGN,
  T_COMMA,
  T_AT,
  T_DEREF,
  T_ADD,
  T_SUB,
  T_MUL,
  T_DIV,
  T_DIVMOD,
  T_LSHIFT,
  T_RSHIFT,
  T_LT,
  T_GT,
  T_AND,
  T_LOGICAL_NOT,
  T_OR,
  T_EQ,
  T_NEQ,
  T_COLON,
  T_SEMICOLON,
  T_POP,
  T_CONST,
  T_LET,
  T_MEMORY,
  T_PRINT,
  T_INCLUDE,
  T_FN,
  T_ARROW,
  T_WHILE,
  T_IF,
  T_ELSE,
  T_LEFT_P,
  T_RIGHT_P,
  T_LEFT_BRACKET,
  T_RIGHT_BRACKET,
  T_LEFT_CURLY,
  T_RIGHT_CURLY,
  T_STORE64,
  T_STORE32,
  T_STORE16,
  T_STORE8,
  T_LOAD64,
  T_LOAD32,
  T_LOAD16,
  T_LOAD8,
  T_SIZEOF,
  T_ENUM,
  T_ALIAS,
  T_CAST,

  // built-in types
  T_NONE,
  T_ANY,
  T_UNSIGNED64,
  T_UNSIGNED32,
  T_CSTR,

  MAX_TOKEN_TYPE,
} Token_type;

static const char* token_type_str[] = {
  "T_EOF",

  "T_IDENTIFIER",
  "T_NUMBER",
  "T_CSTRING",
  "T_ASSIGN",
  "T_COMMA",
  "T_AT",
  "T_DEREF",
  "T_ADD",
  "T_SUB",
  "T_MUL",
  "T_DIV",
  "T_DIVMOD",
  "T_LSHIFT",
  "T_RSHIFT",
  "T_LT",
  "T_GT",
  "T_AND",
  "T_LOGICAL_NOT",
  "T_OR",
  "T_EQ",
  "T_NEQ",
  "T_COLON",
  "T_SEMICOLON",
  "T_POP",
  "T_CONST",
  "T_LET",
  "T_MEMORY",
  "T_PRINT",
  "T_INCLUDE",
  "T_FN",
  "T_ARROW",
  "T_WHILE",
  "T_IF",
  "T_ELSE",
  "T_LEFT_P",
  "T_RIGHT_P",
  "T_LEFT_BRACKET",
  "T_RIGHT_BRACKET",
  "T_LEFT_CURLY",
  "T_RIGHT_CURLY",
  "T_STORE64",
  "T_STORE32",
  "T_STORE16",
  "T_STORE8",
  "T_LOAD64",
  "T_LOAD32",
  "T_LOAD16",
  "T_LOAD8",
  "T_SIZEOF",
  "T_ENUM",
  "T_ALIAS",
  "T_CAST",

  "T_NONE",
  "T_ANY",
  "T_UNSIGNED64",
  "T_UNSIGNED32",
  "T_CSTR",
};

typedef union Tvalue {
  u64 num;
  i64 i;
} Tvalue;

typedef struct Token {
  char* buffer;
  i32 length;
  Token_type type;

  Tvalue v;

  char* filename;
  char* source;

  i32 line;
  i32 column;
} Token;

typedef struct Lexer {
  Token token;
  char* filename;
  char* source; // null terminated input string
  char* index;  // pointer alias of source
  i32 line;
  i32 column;
  i32 status;
} Lexer;

typedef enum Ast_type {
  AstNone = 0,
  AstRoot,
  AstValue,
  AstExpression,
  AstExprList,
  AstStatement,
  AstStatementList,
  AstBlockStatement,
  AstBinopExpression,
  AstUopExpression,
  AstConstStatement,
  AstLetStatement,
  AstFuncDefinition,
  AstFuncCall,
  AstParamList,
  AstMemoryStatement,
  AstAssignment,
  AstWhileStatement,
  AstIfStatement,
  AstType,
  AstSizeof,
  AstEnum,
  AstCastExpression,

  MAX_AST_TYPE,
} Ast_type;

static const char* ast_type_str[] = {
  "None",
  "Root",
  "Value",
  "Expression",
  "ExprList",
  "Statement",
  "StatementList",
  "BlockStatement",
  "BinopExpression",
  "UopExpression",
  "ConstStatement",
  "LetStatement",
  "FuncDefinition",
  "FuncCall",
  "ParamList",
  "MemoryStatement",
  "Assignment",
  "WhileStatement",
  "IfStatement",
  "Type",
  "Sizeof",
  "Enum",
  "CastExpression",
};

typedef struct Ast {
  struct Ast** node;
  u32 count;
  Ast_type type;
  Token token;
  u32 konst; // if this is true, this node contains the evaluation of a constant expression
  u32 pointer; // does this node refer to another node?
} Ast;

#define MAX_SOURCE 32
#define MAX_ALIAS 256

typedef struct Alias {
  Token token;
  Token content;
} Alias;

typedef struct Parser {
  Lexer l;
  Ast* ast;
  i32 status;
  char* path[MAX_SOURCE];
  char* source[MAX_SOURCE];
  u32 source_count;
  Alias aliases[MAX_ALIAS];
  u32 alias_count;
  u32 total_lines;
} Parser;

typedef enum Ir_code {
  I_NOP = 0,
  I_POP,
  I_MOVE, // <type, id, offset>
  I_MOVE_LOCAL, // <type, local_id, x>
  I_STORE64,
  I_STORE32,
  I_STORE16,
  I_STORE8,
  I_LOAD64,
  I_LOAD32,
  I_LOAD16,
  I_LOAD8,
  I_PUSH_ADDR_OF, // <type, id, x>
  I_PUSH_LOCAL_ADDR_OF, // <type, local_id, x>
  I_PUSH, // <type, id, x>
  I_PUSH_LOCAL, // <type, local_id, x>
  I_PUSH_IMM, // <type, imm, x>
  I_ADD,
  I_SUB,
  I_MUL,
  I_LSHIFT,
  I_RSHIFT,
  I_DIV,
  I_DIVMOD,
  I_LT,
  I_GT,
  I_AND,
  I_LOGICAL_NOT,
  I_OR,
  I_EQ,
  I_NEQ,
  I_RET, // <x, frame_size, x>
  I_NORET, // <x, frame_size, x>
  I_PRINT,
  I_LABEL,
  I_CALL, // <label, argc, rtype>
  I_ADDR_CALL, // <x, argc, rtype>
  I_JMP,
  I_JZ, // <label, offset, x>
  I_BEGIN_FUNC, // <x, argc, frame_size>
  I_LOOP_LABEL,

  I_SYSCALL0,
  I_SYSCALL1,
  I_SYSCALL2,
  I_SYSCALL3,
  I_SYSCALL4,
  I_SYSCALL5,
  I_SYSCALL6,

  MAX_IR_CODE,
} Ir_code;

static const char* ir_code_str[] = {
  "I_NOP",
  "I_POP",
  "I_MOVE",
  "I_MOVE_LOCAL",
  "I_STORE64",
  "I_STORE32",
  "I_STORE16",
  "I_STORE8",
  "I_LOAD64",
  "I_LOAD32",
  "I_LOAD16",
  "I_LOAD8",
  "I_PUSH_ADDR_OF",
  "I_PUSH_LOCAL_ADDR_OF",
  "I_PUSH",
  "I_PUSH_LOCAL",
  "I_PUSH_IMM",
  "I_ADD",
  "I_SUB",
  "I_MUL",
  "I_LSHIFT",
  "I_RSHIFT",
  "I_DIV",
  "I_DIVMOD",
  "I_LT",
  "I_GT",
  "I_AND",
  "I_LOGICAL_NOT",
  "I_OR",
  "I_EQ",
  "I_NEQ",
  "I_RET",
  "I_NORET",
  "I_PRINT",
  "I_LABEL",
  "I_CALL",
  "I_ADDR_CALL",
  "I_JMP",
  "I_JZ",
  "I_BEGIN_FUNC",
  "I_LOOP_LABEL",

  "I_SYSCALL0",
  "I_SYSCALL1",
  "I_SYSCALL2",
  "I_SYSCALL3",
  "I_SYSCALL4",
  "I_SYSCALL5",
  "I_SYSCALL6",
};

// https://www.cs.uaf.edu/2017/fall/cs301/reference/x86_64.html
static const char* func_call_regs_x86_64[] = {
  "rdi",
  "rsi",
  "rdx",
  "rcx",
  "r8",
  "r9",
};

typedef enum Compile_type {
  TypeNone = 0,
  TypeAny,
  TypeUnsigned64,
  TypeUnsigned32,
  TypeCString,
  TypeFunc,
  TypeSyscallFunc,

  MAX_COMPILE_TYPE,
} Compile_type;

static const char* compile_type_str[] = {
  "None",
  "Any",
  "Unsigned64",
  "Unsigned32",
  "CString",
  "Func",
  "SyscallFunc",
};

static u64 compile_type_size[] = {
  0,
  sizeof(u64),
  sizeof(u64),
  sizeof(u32),
  sizeof(u64),
  sizeof(u64),
  sizeof(u64),
};

// intermidiate representation of the instructions which are to be generated or interpreted
typedef struct Op {
  Ir_code i;
  i32 dest;
  i32 src0;
  i32 src1;
} Op;

#define OP(_i) ((Op) {.i = _i, .dest = -1, .src0 = -1, .src1 = -1, })

#define MAX_STATIC_DATA (KB(32)) // temp

#define MAX_NAME_SIZE 64
#define MAX_SYMBOL 1024
#define MAX_FUNC 1024
#define MAX_FUNC_ARGC 6 // temp

typedef enum Compile_target {
  TARGET_LINUX_NASM_X86_64,
  TARGET_WIN_NASM_X86_64,
  MAX_COMPILE_TARGET,
} Compile_target;

// NOTE(lucas): annoying workaround to be able for macro if-statements to properly work (e.g. #if MACHINE == MACHINE_WIN64)
#define MACHINE_LINUX      0
#define MACHINE_MACOS      1
#define MACHINE_WIN64      2
#define MAX_TARGET_MACHINE 3

static const char* compile_target_str[MAX_COMPILE_TARGET] = {
  "compile_linux_nasm_x86_64",
  "compile_win_nasm_x86_64",
};

static const char* compile_target_compile_str[MAX_COMPILE_TARGET] = {
  "nasm",
  "nasm",
};

static const char* compile_target_link_str[MAX_COMPILE_TARGET] = {
  "ld",
  "ld",
};

// 2 * MAX_COMPILE_TARGET for compile and link options
// TODO(lucas): reimplement for multiple targets and machines
static const char* target_machine_option_str[MAX_TARGET_MACHINE][2 * MAX_COMPILE_TARGET] = {
  {
    "-f elf64",
    "-arch x86_64",
  },
  {
    "-f macho64",
    "-arch x86_64 -e _start -no_pie -lc -lSystem",
  },
  {
    NULL,
    NULL,
    "-f elf64",
    "-arch x86_64",
  },
};

#if __APPLE__
  #define MACHINE MACHINE_MACOS
#elif __LINUX__ || __unix__
  #define MACHINE MACHINE_LINUX
#elif _WIN64
  #define MACHINE MACHINE_WIN64
#else
  // #warning "the compiler does not support this machine, defaulting to linux"
  #define MACHINE MACHINE_LINUX
#endif

typedef struct Function {
  i32 ir_address;
  i32 label;
  u32 argc;
  u32 locals_offset_counter; // some variables are bigger than 8 bytes, so this will act as a sort of offset counter. will have to add something similar to function arguments in the future in case i want to be able to push structs and other things which uses more than 8 bytes.
  Compile_type rtype;
  u32 args[MAX_FUNC_ARGC]; // arguments pointing to some symbol in the function block/symbol table
} Function;

typedef struct Value {
  union {
    Function func;
    u64 num;
  };
  u32 konst;
} Value;

typedef enum Symbol_type {
  SYM_FUNC_ARG = 0,
  SYM_FUNC,
  SYM_LOCAL_VAR,
  SYM_GLOBAL_VAR,
} Symbol_type;

typedef struct Symbol {
  char name[MAX_NAME_SIZE];
  i32 imm; // address to immediate value
  i32 size;
  u32 num_elemements_init;
  i32 konst;
  i32 local_id;
  Symbol_type sym_type;
  Compile_type type;
  Token token;
  Value value;
  Ast* node;
  u32 ref_count;
} Symbol;

typedef struct Block {
  u32 symbols[MAX_SYMBOL]; // index into compile state symbols
  u32 symbol_count;
  struct Block* parent;
} Block;

#define MAX_TYPE_STACK 256
#define MAX_VALUE_STACK 256 // temporary
#define MAX_CSTRING 512

// compile state
typedef struct Compile {
  Op* ins;
  u32 ins_count;

  u8 imm[MAX_STATIC_DATA];
  u32 imm_index;

  Symbol symbols[MAX_SYMBOL];
  u32 symbol_count;

  u32 cstrings[MAX_CSTRING];
  u32 cstring_count;

  Block global;

  i32 label_count;  // labels that are used for branching

  i32 status;
  i32 entry_point;
  u32 entry_point_address;

  Compile_type ts[MAX_TYPE_STACK]; // type stack
  i32 ts_count; // signed integer to be able to detect stack underflows

  Value vs[MAX_VALUE_STACK];  // sometimes we need to grab values in the type checking phase, therefore we also have a value stack. this will probably be changed later
  i32 vs_count;
} Compile;

typedef struct Options {
  i32 compile;
  i32 run;
  i32 debug;
  char* filename;
} Options;

static i32 enable_warnings = 0;
static i32 disable_dce = 0; // disable dead-code-elimination?
static i32 dce_all = 0; // enable dce for everything?

static i32 spl_start(Options* options);

static char* read_entire_file(char* path);
static char* string_insert(char* dest, char* source, u32 dest_length, u32 source_length, u32 index);
static char* string_shrink_at(char* dest, u32 dest_length, u32 index, u32 count);

static void symbol_init(Symbol* s);
static void symbol_print(Symbol* s, FILE* fp);
static void block_init(Block* block, Block* parent);

static i32 compile_state_init(Compile* c);
static void compile_state_free(Compile* c);
static void compile_error(Compile* c, const char* fmt, ...);
static void compile_error_at(Compile* c, Token token, const char* fmt, ...);
static i32 compile_declare_value(Compile* c, Block* block, Function* func, Token token, Symbol** symbol, Ast* node, i32* symbol_index);
static i32 compile_create_syscall(Compile* c, const char* name, u32 argc);
static i32 compile_lookup_value(Compile* c, Block* block, Function* func, Token token, Symbol** symbol, i32* symbol_index, u32* levels_descent);
static void compile_print_symbol_info(Compile* c, FILE* fp);

static i32 typecheck_program(Compile* c, Ast* ast);
static void typecheck_print_unused(Compile* c);
static void typecheck_error(Compile* c, const char* fmt, ...);
static void typecheck_error_at(Compile* c, Token token, const char* fmt, ...);
static Compile_type typecheck(Compile* c, Block* block, Function* fs, Ast* ast);
static Compile_type typecheck_node_list(Compile* c, Block* block, Function* fs, Ast* ast);
static Compile_type typecheck_let_statement(Compile* c, Block* block, Function* fs, Ast* ast);
static u32 typecheck_is_numerical(Compile* c, Compile_type type);
static Compile_type ts_push(Compile* c, Compile_type type);
static Compile_type ts_pop(Compile* c);
static Compile_type ts_top(Compile* c);
static i32 vs_push(Compile* c, Value v);
static i32 vs_pop(Compile* c, Value* v);
static Value vs_top(Compile* c);
static i32 check_func_signatures(Compile* c, Function* a, Function* b);
static u32 is_branch_konst_eval(Ast* ast);

static void ir_compile_warning(Compile* c, const char* fmt, ...);
static void ir_compile_warning_at(Compile* c, Token token, const char* fmt, ...);
static void ir_print(Compile* c, FILE* fp);
static void ir_binary_output(Compile* c, FILE* fp);
static i32 ir_start_compile(Compile* c, Ast* ast);
static i32 ir_push_ins(Compile* c, Op ins, u32* ins_count);
static i32 ir_pop_ins(Compile* c, Op* ins, u32* ins_count);
static i32 ir_push_value(Compile* c, void* value, u32 size);
static i32 ir_push_cstring(Compile* c, char* buffer, u32 length, u32* cstring_index);
static i32 ir_push_symbol(Compile* c, Function* fs, Symbol* symbol, i32 id, u32* ins_count);
static i32 ir_compile(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count);
static i32 ir_compile_stmts(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count);
static i32 ir_compile_func_args(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count);
static i32 ir_compile_binop(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count);
static i32 ir_compile_uop(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count);
static i32 ir_compile_func(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count);

static i32 compile(Compile* c, Compile_target target, FILE* fp);
static i32 compile_linux_nasm_x86_64(Compile* c, FILE* fp);
static i32 compile_win_nasm_x86_64(Compile* c, FILE* fp);

typedef i32 (*compile_target)(Compile*, FILE*);

static compile_target compile_targets[MAX_COMPILE_TARGET] = {
  compile_linux_nasm_x86_64,
  compile_win_nasm_x86_64,
};

static i32 alias_store(Parser* p, Alias* alias);
static i32 alias_lookup(Parser* p, Alias* alias);

static i32 preprocess(Parser* p);

static i32 parser_init(Parser* p, char* filename, char* source);
static void parser_free(Parser* p);
static void parser_error(Parser* p, const char* fmt, ...);
static Ast* parse(Parser* p);
static Ast* parse_statements(Parser* p);
static Ast* parse_statement(Parser* p);
static Ast* parse_expr(Parser* p);
static Ast* parse_func_def(Parser* p);
static Ast* parse_expr_list(Parser* p);
static Ast* parse_ident_list(Parser* p);
static Ast* parse_param_list(Parser* p);
static Ast* parse_type(Parser* p);
static i32 parse_alias(Parser* p, u32 skip_store_of_alias);
static void lexer_init(Lexer* l, char* filename, char* source);
static void lexer_error(Lexer* l, const char* fmt, ...);
static void next(Lexer* l);
inline i32 compare(Token t, const char* b);
inline i32 expect(Token t, Token_type type);
inline Token lexer_read_symbol(Lexer* l);
inline Token lexer_read_number(Lexer* l);
inline i32 is_digit(char ch);
inline i32 is_hex(char ch);
inline i32 is_alpha(char ch);
inline char to_lower(char ch);
inline i32 is_extended_ascii(u8 ch);
static Token lexer_next(Lexer* l);
static Token lexer_peek(Lexer* l);

static Compile_type token_to_compile_type(Token t);

static void printline(FILE* fp, char* source, char* index, i32 token_length, i32 print_arrow, u32 num_lines_to_print);
static void print_info(const char* fmt, ...);
static i32 str_to_int(char* str, u32 length, u64* out);

static Ast* ast_create(Ast_type type);
static void ast_init_node(Ast* node);
static Ast* ast_alloc_node();
static Ast* ast_push_node(Ast* ast, Ast_type type, Token token);
static Ast* ast_push(Ast* ast, Ast* node);
static u32 ast_count(Ast* ast);
static void ast_print(const Ast* ast, i32 level, FILE* fp);
static void ast_free(Ast* ast);

i32 exec_command(const char* fmt, ...);
u32 first_dot(const char* s);

i32 main(i32 argc, char** argv) {
  assert(ARR_SIZE(token_type_str) == MAX_TOKEN_TYPE);
  assert(ARR_SIZE(ast_type_str) == MAX_AST_TYPE);
  assert(ARR_SIZE(ir_code_str) == MAX_IR_CODE);
  assert(ARR_SIZE(compile_target_str) == MAX_COMPILE_TARGET);
  assert(ARR_SIZE(compile_target_compile_str) == MAX_COMPILE_TARGET);
  assert(ARR_SIZE(compile_target_link_str) == MAX_COMPILE_TARGET);
  assert(ARR_SIZE(target_machine_option_str) == MAX_TARGET_MACHINE);
  assert(ARR_SIZE(compile_targets) == MAX_COMPILE_TARGET);
  assert(ARR_SIZE(token_type_str) == MAX_TOKEN_TYPE);
  assert(ARR_SIZE(ir_code_str) == MAX_IR_CODE);
  assert(ARR_SIZE(ast_type_str) == MAX_AST_TYPE);
  assert(ARR_SIZE(compile_type_str) == MAX_COMPILE_TYPE);
  assert(ARR_SIZE(compile_type_size) == MAX_COMPILE_TYPE);

  (void)symbol_print; // unused
  (void)ir_pop_ins; // unused
  i32 exit_status = EXIT_SUCCESS;
  Options options = (Options) {
    .compile = 1,
    .run = 0,
    .debug = 1,
    .filename = NULL,
  };

  if (argc < 2) {
    fprintf(stdout, "Usage; %s [OPTIONS]\n\n", *argv);
    fprintf(stdout,
      "OPTIONS:\n"
      "  <filename>      - path to file\n"
      "  run             - run program directly\n"
      "  no-com          - do not compile target executable\n"
      "  no-debug        - do not write debug information\n"
      "  enable-warnings - enable basic warnings\n"
      "  disable-dce     - disable dead code elimination\n"
      "  dce-all         - eliminate all dead code, including all symbols\n"
    );
    return EXIT_SUCCESS;
  }
  ++argv;
  while (*argv != NULL) {
    if (!strncmp(*argv, "run", MAX_PATH_SIZE)) {
      options.run = 1;
    }
    else if (!strncmp(*argv, "no-com", MAX_PATH_SIZE)) {
      options.compile = 0;
    }
    else if (!strncmp(*argv, "no-debug", MAX_PATH_SIZE)) {
      options.debug = 0;
    }
    else if (!strncmp(*argv, "enable-warnings", MAX_PATH_SIZE)) {
      enable_warnings = 1;
    }
    else if (!strncmp(*argv, "disable-dce", MAX_PATH_SIZE)) {
      disable_dce = 1;
    }
    else if (!strncmp(*argv, "dce-all", MAX_PATH_SIZE)) {
      dce_all = 1;
    }
    else {
      options.filename = *argv;
    }
    ++argv;
  }
  i32 result = spl_start(&options);
  if (result != NoError) {
    exit_status = EXIT_FAILURE;
  }
  return exit_status;
}

i32 spl_start(Options* options) {
  (void)ir_binary_output;
  i32 result = NoError;
  // read entry source file
  char* source = read_entire_file(options->filename);
  if (!source) {
    return Error;
  }
  // begin compiling source file
  REAL_TIMER_START();
  Parser p;
  Compile c;
  if (parser_init(&p, options->filename, source) == NoError) {
    p.ast = ast_create(AstRoot);
    ast_push(p.ast, parse(&p));
    if (p.ast && p.status == NoError && p.l.status == NoError) {
      if (compile_state_init(&c) == NoError) {
        if (typecheck_program(&c, p.ast) == NoError) {
          if (ir_start_compile(&c, p.ast) == NoError) {
            if (options->debug) {
              char debug_path[MAX_PATH_SIZE] = {0};
              snprintf(debug_path, MAX_PATH_SIZE, "%s.debug", options->filename);
              FILE* debug = fopen(debug_path, "w");
              if (debug) {
                ast_print(p.ast, 0, debug);
                ir_print(&c, debug);
                compile_print_symbol_info(&c, debug);
                fclose(debug);
              }
            }
            if (options->compile) {
              char path[MAX_PATH_SIZE] = {0};
              snprintf(path, MAX_PATH_SIZE, "%s.asm", options->filename);
              FILE* fp = fopen(path, "w");
              Compile_target target = TARGET_LINUX_NASM_X86_64;
#if MACHINE == MACHINE_WIN64
              target = TARGET_WIN_NASM_X86_64;
#endif
              if (fp) {
                compile(&c, target, fp);
                fclose(fp);
              }
              REAL_TIMER_END(
                print_info("compilation took %lf seconds (%d loc, %d file(s))\n", _dt, p.total_lines, p.source_count);
                (void)_dt;
              );
              char exec_path[MAX_PATH_SIZE] = {0};
              char o_path[MAX_PATH_SIZE] = {0};
              snprintf(exec_path, MAX_PATH_SIZE, "%.*s", first_dot(options->filename), options->filename);
              snprintf(o_path, MAX_PATH_SIZE, "%s.o", exec_path);
              const char** target_options = target_machine_option_str[MACHINE];
              if (exec_command(
                "%s %s %s -o %s && "
                "%s %s %s -o %s"
                ,
                compile_target_compile_str[target],
                target_options[target * 2],
                path,
                o_path,
                compile_target_link_str[target],
                target_options[target * 2 + 1],
                o_path,
                exec_path
              ) == NoError && options->run) {
                exec_command("./%s", exec_path);
              }
            }
          }
          else {
            result = Error;
          }
        }
        else {
          result = Error;
        }
      }
      compile_state_free(&c);
    }
    else {
      result = Error;
    }
    parser_free(&p);
  }
  return result;
}

char* read_entire_file(char* path) {
  FILE* fp = fopen(path, "rb");
  if (!fp) {
    fprintf(stderr, "[error]: failed to open file `%s`\n", path);
    return NULL;
  }
  char* source = NULL;
  fseek(fp, 0, SEEK_END);
  i32 size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  source = (char*)malloc(size + 1);
  if (!source) {
    fprintf(stderr, "[error]: failed to allocate memory for file `%s`\n", path);
    fclose(fp);
    return NULL;
  }
  i32 read_size = fread(source, 1, size, fp);
  (void)read_size;  // ignore warning
  source[read_size] = '\0';
  fclose(fp);
  return source;
}

char* string_insert(char* dest, char* source, u32 dest_length, u32 source_length, u32 index) {
  u32 new_size = dest_length + source_length;
  char* string = realloc(dest, new_size);
  if (!string) {
    return NULL;
  }
  memmove(&string[index + source_length], &string[index], dest_length - index);
  memmove(&string[index], source, source_length);
  return string;
}

char* string_shrink_at(char* dest, u32 dest_length, u32 index, u32 count) {
  i32 new_size = (i32)dest_length - count;
  char* string = NULL;
  if (new_size < 0) {
    return string;
  }

  if ((i32)(index - count) < new_size) {
    memmove(&dest[index], &dest[index + count], dest_length - (index + count));
  }
  string = realloc(dest, new_size);
  string[new_size] = '\0';

  return string;
}

void symbol_init(Symbol* s) {
  memset(s->name, 0, ARR_SIZE(s->name));
  s->imm = -1;
  s->size = 0;
  s->num_elemements_init = 0;
  s->konst = 0;
  s->local_id = -1;
  s->sym_type = SYM_LOCAL_VAR;
  s->type = TypeNone;
  s->token = (Token) {0};
  s->value = (Value) { .num = 0, .konst = 0, };
  s->node = NULL;
  s->ref_count = 0;
}

void symbol_print(Symbol* s, FILE* fp) {
  fprintf(fp,
    "Symbol {\n"
    "  name = %s,\n"
    "  imm = %d,\n"
    "  size = %d,\n"
    "  sym_type = %d,\n"
    "  type = %s,\n"
    "  token = { %.*s, %d, },\n"
    "  value = {\n"
    "    func = {\n"
    "      ir_address = %d,\n"
    "      label = %d,\n"
    "      argc = %d,\n"
    "      rtype = %s,\n"
    "    },\n"
    "    num = %ld,\n"
    "  },\n"
    "}\n"
    ,
    s->name,
    s->imm,
    s->size,
    s->sym_type,
    compile_type_str[s->type],
    s->token.length, s->token.buffer, s->token.type,
    s->value.func.ir_address,
    s->value.func.label,
    s->value.func.argc,
    compile_type_str[s->value.func.rtype],
    s->value.num
  );
}

void block_init(Block* block, Block* parent) {
  block->symbol_count = 0;
  block->parent = parent;
}

i32 compile_state_init(Compile* c) {
  c->ins = NULL;
  c->ins_count = 0;
  c->imm_index = 0;
  c->symbol_count = 0;
  c->cstring_count = 0;
  block_init(&c->global, NULL);
  c->label_count = 0;
  c->status = NoError;
  c->entry_point = 0;
  c->entry_point_address = 0;
  c->ts_count = 0;
  c->vs_count = 0;

  compile_create_syscall(c, "syscall0", 0);
  compile_create_syscall(c, "syscall1", 1);
  compile_create_syscall(c, "syscall2", 2);
  compile_create_syscall(c, "syscall3", 3);
  compile_create_syscall(c, "syscall4", 4);
  compile_create_syscall(c, "syscall5", 5);
  compile_create_syscall(c, "syscall6", 6);
  return NoError;
}

void compile_state_free(Compile* c) {
  free(c->ins);
  c->ins_count = 0;
}

void compile_error(Compile* c, const char* fmt, ...) {
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stderr;
  fprintf(fp, "[compile-error]: %s", buffer);
  c->status = Error;
}

void compile_error_at(Compile* c, Token token, const char* fmt, ...) {
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stderr;
  fprintf(fp, "[compile-error]: %s:%d:%d: %s", token.filename, token.line, token.column, buffer);
  printline(fp, token.source, token.buffer + token.length, token.length, 1, NUM_LINES_TO_PRINT);
  c->status = Error;
}

i32 compile_declare_value(Compile* c, Block* block, Function* fs, Token token, Symbol** symbol, Ast* node, i32* symbol_index) {
  if (MAX_NAME_SIZE < token.length) {
    return Error;
  }
  if (block->symbol_count < MAX_SYMBOL) {
    u32 levels_descent = 0; // how many levels did we descent before we found a symbol
    i32 lookup_result = compile_lookup_value(c, block, fs, token, symbol, NULL, &levels_descent);
    if (lookup_result == NoError && levels_descent == 0) { // we are only allowed to create a new symbol if none was found in the current block
      return c->status = Error;
    }
    i32 index = c->symbol_count;
    if (symbol_index) {
      *symbol_index = index;
    }
    block->symbols[block->symbol_count] = index;
    *symbol = &c->symbols[index];

    c->symbol_count++;
    block->symbol_count++;

    Symbol* s = *symbol;
    symbol_init(s);
    s->token = token;
    strncpy(s->name, token.buffer, token.length);
    s->node = node;
    return NoError;
  }
  assert("symbol capacity exceeded" && 0);
  return Error;
}

i32 compile_create_syscall(Compile* c, const char* name, u32 argc) {
  Block* block = &c->global;
  Token token = (Token) {
    .buffer = (char*)name,
    .length = strnlen(name, MAX_NAME_SIZE),
    .type = T_IDENTIFIER,

    .filename = NULL,
    .source = NULL,
    .line = 0,
    .column = 0,
  };

  Symbol* symbol = NULL;
  i32 symbol_index = -1;
  compile_declare_value(c, block, NULL, token, &symbol, NULL, &symbol_index);
  assert(symbol_index != -1);
  symbol->imm = -1;
  symbol->size = compile_type_size[TypeSyscallFunc];
  symbol->sym_type = SYM_FUNC;
  symbol->type = TypeSyscallFunc;
  symbol->token.v.i = symbol_index;
  symbol->ref_count = 1;
  Function* func = &symbol->value.func;
  func->ir_address = -1;
  func->label = -1;
  func->argc = argc + 1;
  func->locals_offset_counter = 0;
  func->rtype = TypeUnsigned64;
  return NoError;
}

i32 compile_lookup_value(Compile* c, Block* block, Function* func, Token token, Symbol** symbol, i32* symbol_index, u32* levels_descent) {
  assert("must pass reference to a symbol to store the return value in" && symbol != NULL);
  if (MAX_NAME_SIZE < token.length) {
    return Error;
  }
  if (!block) {
    return Error;
  }
  char copy[MAX_NAME_SIZE] = {0};
  strncpy(copy, token.buffer, token.length);
  for (u32 i = 0; i < block->symbol_count; ++i) {
    i32 index = block->symbols[i];
    Symbol* sym = &c->symbols[index];
    if (strncmp(copy, sym->name, MAX_NAME_SIZE) == 0) {
      *symbol = sym;
      if (symbol_index) {
        *symbol_index = index;
      }
      return NoError;
    }
  }
  // descend into lower level, and increment counter, because we did not find the symbol in this block
  if (levels_descent) {
    (*levels_descent)++;
  }
  // do lookup in the parent block, until global block is reached
  return compile_lookup_value(c, block->parent, func, token, symbol, symbol_index, levels_descent);
}

void compile_print_symbol_info(Compile* c, FILE* fp) {
  fprintf(fp, "%s:\n", __FUNCTION__);
  for (u32 i = 0; i < c->symbol_count; ++i) {
    Symbol* symbol = &c->symbols[i];
    if (symbol->type == TypeSyscallFunc) {
      continue;
    }
    if (symbol->type == TypeFunc) {
      Function* func = &symbol->value.func;
      fprintf(fp, "%3u: `%s` (", i, symbol->name);
      for (u32 arg_index = 0; arg_index < func->argc; ++arg_index) {
        Symbol* arg = &c->symbols[func->args[arg_index]];
        u32 count = arg->size / compile_type_size[arg->type];
        fprintf(fp, "%s", compile_type_str[arg->type]);
        if (count > 1) {
          fprintf(fp, " : %d", count);
        }
        if (arg_index != func->argc - 1) {
          fprintf(fp, ", ");
        }
      }
      fprintf(fp, ") -> %s (konst = %d, value.konst = %d)", compile_type_str[func->rtype], symbol->konst, symbol->value.konst);
      fprintf(fp, " - %s:%d:%d\n", symbol->token.filename, symbol->token.line, symbol->token.column);
      continue;
    }
    fprintf(fp, "%3u: `%s` (type = %s, size = %d, konst = %d, value.konst = %d) - %s:%d:%d\n", i, symbol->name, compile_type_str[symbol->type], symbol->size, symbol->konst, symbol->value.konst, symbol->token.filename, symbol->token.line, symbol->token.column);
  }
}

i32 typecheck_program(Compile* c, Ast* ast) {
#if NO_TYPECHECKING // NOTE(lucas): this is only temporary
  return c->status;
#endif
  REAL_TIMER_START();
  assert("something went very wrong" && ast->type == AstRoot && ast);
  for (u32 i = 0; i < ast->count && c->status == NoError; ++i) {
    typecheck(c, &c->global, NULL /* function state */, ast->node[i]);
  }
  if (c->vs_count != 0) {
    typecheck_error(c, "unhandled data on the value stack (%d)\n", c->vs_count);
  }
  if (c->ts_count != 0 && c->status != NoError) {
    // only print this error if nothing else failed
    typecheck_error(c, "unhandled data on the stack (%d)\n", c->ts_count);
  }
  if (c->status == NoError) {
    typecheck_print_unused(c);
  }
  REAL_TIMER_END(
    print_info("type checking took %lf seconds\n", _dt);
    (void)_dt;
  );
  return c->status;
}

void typecheck_print_unused(Compile* c) {
  if (disable_dce) {
    // may be redundant because why would this function be called if dce is disabled?
    return;
  }
  for (u32 i = MAX_SYSCALL_FUNCTION; i < c->symbol_count; ++i) {
    Symbol* s = &c->symbols[i];
    if (s->ref_count == 0 && (s->sym_type == SYM_FUNC || dce_all)) {
      ir_compile_warning_at(c, s->token, "`%s` defined but not used\n", s->name);
    }
  }
}

void typecheck_error(Compile* c, const char* fmt, ...) {
  if (c->status == Error) {
    return;
  }
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stderr;
  fprintf(fp, "[type-check-error]: %s", buffer);
  c->status = Error;
}

void typecheck_error_at(Compile* c, Token token, const char* fmt, ...) {
  if (c->status == Error) {
    return;
  }
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stderr;
  fprintf(fp, "[type-check-error]: %s:%d:%d: %s", token.filename, token.line, token.column, buffer);
  printline(fp, token.source, token.buffer + token.length, token.length, 1, NUM_LINES_TO_PRINT);
  c->status = Error;
}

Compile_type typecheck(Compile* c, Block* block, Function* fs, Ast* ast) {
  // early out if error was detected
  if (c->status == Error) {
    return TypeNone;
  }
  switch (ast->type) {
    case AstValue: {
      switch (ast->token.type) {
        case T_NUMBER: {
          vs_push(c, (Value) { .num = ast->token.v.num, .konst = 1, });
          ast->konst = 1;
          return ts_push(c, TypeUnsigned64);
        }
        case T_CSTRING: {
          vs_push(c, (Value) { .num = 0, });
          return ts_push(c, TypeCString);
        }
        case T_POP: {
          vs_pop(c, NULL);
          return ts_pop(c);
        }
        case T_IDENTIFIER: {
          Symbol* symbol = NULL;
          i32 symbol_index = -1;
          if (compile_lookup_value(c, block, fs, ast->token, &symbol, &symbol_index, NULL) == NoError) {
            symbol->ref_count++;
            ast->token.v.i = symbol_index;
            vs_push(c, symbol->value);
            if (symbol->value.konst) {
              ast->konst = 1;
            }
            return ts_push(c, symbol->type);
          }
          compile_error_at(c, ast->token, "symbol `%.*s` not defined\n", ast->token.length, ast->token.buffer);
          return TypeNone;
        }
        case T_AT: {
          Symbol* symbol = NULL;
          i32 symbol_index = -1;
          if (compile_lookup_value(c, block, fs, ast->token, &symbol, &symbol_index, NULL) == NoError) {
            symbol->ref_count++;
            if (symbol->type == TypeNone) {
              typecheck_error_at(c, ast->token, "can not take the address of the type `%s`\n", compile_type_str[symbol->type]);
              return TypeNone;
            }
            ast->token.v.i = symbol_index;
            vs_push(c, symbol->value);
            return ts_push(c, TypeUnsigned64); // pointers are handled as 64-bit unsigned integers for now
          }
          compile_error_at(c, ast->token, "symbol `%.*s` not defined\n", ast->token.length, ast->token.buffer);
          return TypeNone;
        }
        default: {
          assert(0); // TODO: handle
          break;
        }
      }
      return TypeNone;
    }
    case AstExpression: {
      Compile_type result = typecheck(c, block, fs, ast->node[0]);
      ast->konst = is_branch_konst_eval(ast);
      return result;
    }
    case AstExprList:
    case AstStatement:
    case AstStatementList: {
      Compile_type result = typecheck_node_list(c, block, fs, ast);
      ast->konst = is_branch_konst_eval(ast);
      return result;
    }
    case AstBinopExpression: {
      typecheck_node_list(c, block, fs, ast);
      Compile_type b = ts_pop(c);
      Compile_type a = ts_pop(c);
      if (
          (typecheck_is_numerical(c, a) || a == TypeAny || a == TypeCString) &&
          (typecheck_is_numerical(c, b) || b == TypeAny || b == TypeCString)
        ) {
        Value va;
        Value vb;
        vs_pop(c, &vb);
        vs_pop(c, &va);
        if (va.konst && vb.konst) {
          ast->konst = 1;
        }
        u64 num = 0;
        switch (ast->token.type) {
          case T_ADD:
            num = va.num + vb.num;
            break;
          case T_SUB:
            num = va.num - vb.num;
            break;
          case T_MUL:
            num = va.num * vb.num;
            break;
          case T_DIV: {
            u64 nozero[] = {vb.num, 1};
            num = va.num / nozero[vb.num == 0];
            if (vb.num == 0) {
              typecheck_error_at(c, ast->token, "divide by zero arithmetic error\n");
              return TypeNone;
            }
            break;
          }
          case T_DIVMOD: {
            u64 nozero[] = {vb.num, 1};
            num = va.num % nozero[vb.num == 0];
            if (vb.num == 0) {
              typecheck_error_at(c, ast->token, "modulo by zero arithmetic error\n");
              return TypeNone;
            }
            break;
          }
          case T_LSHIFT:
            num = va.num << vb.num;
            break;
          case T_RSHIFT:
            num = va.num >> vb.num;
            break;
          case T_LT:
            num = va.num < vb.num;
            break;
          case T_GT:
            num = va.num > vb.num;
            break;
          case T_AND:
            num = va.num && vb.num;
            break;
          case T_OR:
            num = va.num || vb.num;
            break;
          case T_EQ:
            num = va.num == vb.num;
            break;
          case T_NEQ:
            num = va.num != vb.num;
            break;
          default:
            assert("unhandled operation" && 0);
            break;
        }
        vs_push(c, (Value) { .num = num, .konst = ast->konst, });
        // return ts_push(c, TypeUnsigned64);
        return ts_push(c, a);
      }
      typecheck_error_at(c, ast->token, "type mismatch in binary operator expression\n");
      return TypeNone;
    }
    case AstUopExpression: {
      i32 ts_count = c->ts_count;
      typecheck_node_list(c, block, fs, ast);
      ast->konst = is_branch_konst_eval(ast);
      i32 ts_delta = c->ts_count - ts_count;
      if (ts_delta == 0) {
        typecheck_error_at(c, ast->token, "no value was produced in the rhs of the unary operator expression\n");
        return TypeNone;
      }
      if (ast->token.type == T_PRINT) {
        Value value;
        vs_pop(c, &value);
        ts_pop(c);
        return TypeNone;
      }
      else if (ast->token.type == T_DEREF || ast->token.type == T_LOAD64 || ast->token.type == T_LOAD32 || ast->token.type == T_LOAD16 || ast->token.type == T_LOAD8) {
        ts_pop(c);
        ts_push(c, TypeUnsigned64);
      }
      else if (ast->token.type == T_LOGICAL_NOT) {
        Value value;
        vs_pop(c, &value);
        value.num = !value.num;
        vs_push(c, value);
      }
      return ts_top(c);
    }
    case AstConstStatement:
    case AstLetStatement: {
      return typecheck_let_statement(c, block, fs, ast);
    }
    case AstBlockStatement: {
      Block local_block;
      block_init(&local_block, block);
      Compile_type result = typecheck_node_list(c, &local_block, fs, ast);
      ast->konst = is_branch_konst_eval(ast);
      return result;
    }
    case AstFuncDefinition: {
      Ast* params = ast->node[0];
      Ast* body = ast->node[1];
      Ast* rtype_node = NULL;
      if (ast->count == 3) {
        rtype_node = ast->node[2];
      }

      if (params->count > MAX_FUNC_ARGC) {
        compile_error_at(c, ast->token, "reached function parameter count limit of %d\n", MAX_FUNC_ARGC);
        return TypeNone;
      }

      Symbol* symbol = NULL;
      i32 symbol_index = -1;
      if (compile_declare_value(c, block, fs, ast->token, &symbol, ast, &symbol_index) == NoError) {
        Block local_block;
        block_init(&local_block, block);

        symbol->imm = -1;
        symbol->size = compile_type_size[TypeFunc];
        symbol->konst = 0;
        symbol->sym_type = SYM_FUNC;
        symbol->type = TypeFunc;
        ast->token.v.i = symbol_index;
        Function* func = &symbol->value.func;
        func->ir_address = -1;
        func->label = symbol_index;
        func->argc = params->count;
        func->locals_offset_counter = 0;
        func->rtype = TypeNone;

        if (rtype_node != NULL) {
          func->rtype = token_to_compile_type(rtype_node->token);
        }

        for (u32 i = 0; i < func->argc; ++i) {
          Symbol* arg_symbol = NULL;
          Ast* arg_node = params->node[i];
          Token arg_type = arg_node->token;
          Token arg = arg_node->node[0]->token;
          i32 arg_symbol_index = -1;
          if (compile_declare_value(c, &local_block, func, arg, &arg_symbol, arg_node, &arg_symbol_index) == NoError) {
            u32* arg_id = &func->args[i];
            *arg_id = arg_symbol_index;
            Compile_type arg_compile_type = token_to_compile_type(arg_type);

            arg_symbol->imm = -1;
            arg_symbol->size = compile_type_size[arg_compile_type];
            arg_symbol->konst = 0;
            arg_symbol->sym_type = SYM_FUNC_ARG;
            arg_symbol->type = arg_compile_type;
            arg_symbol->token = arg;
            arg_symbol->token.v.i = i; // TODO(lucas): change where we store the argument id (don't think this is used anymore, investigate)
          }
          else {
            compile_error_at(c, arg, "duplicate argument `%.*s`\n", arg.length, arg.buffer);
            return TypeNone;
          }
        }

        i32 ts_count = c->ts_count;
        Block func_body_block;
        block_init(&func_body_block, &local_block); // to allow for shadowing of function arguments
        typecheck_node_list(c, &func_body_block, fs, body);
        body->konst = ast->konst = symbol->value.konst = is_branch_konst_eval(body);
        Compile_type rtype = TypeNone;
        i32 ts_delta = c->ts_count - ts_count;
        if (ts_delta > 1) {
          typecheck_error_at(c, ast->token, "too many values produced by function `%s`\n", symbol->name);
          return TypeNone;
        }
        if (ts_delta != 0) {
          vs_pop(c, NULL);
          rtype = ts_pop(c);
        }

        if (rtype_node != NULL) {
          Compile_type explicit_rtype = token_to_compile_type(rtype_node->token);
          if (explicit_rtype != rtype && explicit_rtype != TypeAny) {
            compile_error_at(c, rtype_node->token, "function returns a value that does not match the return type\n");
            return TypeNone;
          }
        }
        else {
          func->rtype = rtype;
        }

        if (!strncmp(symbol->name, "main", MAX_NAME_SIZE)) {
          symbol->ref_count++;
          ++c->entry_point;
        }
        return TypeFunc;
      }
      compile_error_at(c, ast->token, "symbol `%.*s` has already been declared\n", ast->token.length, ast->token.buffer);
      return TypeNone;
    }
    case AstFuncCall: {
      Symbol* symbol = NULL;
      i32 symbol_index = -1;
      if (compile_lookup_value(c, block, fs, ast->token, &symbol, &symbol_index, NULL) == NoError) {
        symbol->ref_count++;
        Ast* arg_list = ast->node[0];
        Function* func = &symbol->value.func;
        if (symbol->type == TypeAny) {
          typecheck_error_at(c, ast->token, "function call of type `%s` is not allowed\n", compile_type_str[symbol->type]);
          return TypeNone;
        }

        if (func->argc != arg_list->count) {
          compile_error_at(c, ast->token, "function `%s` takes %d argument(s), but %d was given\n", symbol->name, func->argc, arg_list->count);
          return TypeNone;
        }
        ast->token.v.i = symbol_index;
        for (i32 i = arg_list->count - 1; i >= 0; --i) {
          typecheck(c, block, fs, arg_list->node[i]);
          Compile_type arg_type = ts_top(c);
          if (symbol->type == TypeSyscallFunc) {
            // any type is allowed here
            // we still want to type check the arguments of the call,
            // which is why this is here instead of outside the loop
          }
          else {
            Symbol* arg = &c->symbols[func->args[i]];
            if (arg->type != arg_type && arg->type != TypeAny) {
              typecheck_error_at(c, ast->token, "type mismatch in function call, got `%s`, but `%s` was expected\n", compile_type_str[arg_type], compile_type_str[arg->type]);
              c->status = NoError; // to print additional error message
              typecheck_error_at(c, arg->token, "from function `%s`\n", symbol->name);
              return TypeNone;
            }
          }
          vs_pop(c, NULL);
          ts_pop(c);
        }
        if (func->rtype == TypeNone) {
          return TypeNone;
        }
        symbol->value.konst = 0; // NOTE(lucas): temporary
        vs_push(c, symbol->value);
        ast->konst = symbol->value.konst;
        return ts_push(c, func->rtype);
      }
      compile_error_at(c, ast->token, "symbol `%.*s` not defined\n", ast->token.length, ast->token.buffer);
      return TypeNone;
    }
    case AstWhileStatement: {
      Ast* cond = ast->node[0];
      Ast* body = ast->node[1];
      Compile_type type = typecheck(c, block, fs, cond);
      ts_pop(c); // pop condition result
      vs_pop(c, NULL);
      if (typecheck_is_numerical(c, type)) {
        Block local_block;
        block_init(&local_block, block);
        typecheck(c, &local_block, fs, body);
        ast->konst = is_branch_konst_eval(ast);
        return TypeNone;
      }
      typecheck_error(c, "invalid type in while statement condition\n");
      return TypeNone;
    }
    case AstIfStatement: {
      Ast* cond = ast->node[0];
      Ast* body = ast->node[1];
      Compile_type type = typecheck(c, block, fs, cond);
      ts_pop(c); // pop condition result
      vs_pop(c, NULL);
      if (typecheck_is_numerical(c, type)) {
        Block local_block;
        block_init(&local_block, block);
        typecheck(c, &local_block, fs, body);
        if (ast->count == 3) {
          Ast* else_body = ast->node[2];
          Block local_block;
          block_init(&local_block, block);
          typecheck(c, &local_block, fs, else_body);
        }
        ast->konst = is_branch_konst_eval(ast);
        return TypeNone;
      }
      typecheck_error_at(c, cond->token, "invalid type in if statement condition\n");
      return TypeNone;
    }
    case AstMemoryStatement: {
      Symbol* symbol = NULL;
      i32 symbol_index = -1;
      Ast* node = ast->node[0];
      Compile_type type = typecheck(c, block, fs, node);
      if (typecheck_is_numerical(c, type)) {
        ts_pop(c);
        Value value;
        vs_pop(c, &value);
        if (compile_declare_value(c, block, fs, ast->token, &symbol, ast, &symbol_index) == NoError) {
          symbol->imm = -1;
          symbol->size = value.num;
          symbol->konst = 0;
          symbol->sym_type = SYM_GLOBAL_VAR;
          symbol->type = type;
          symbol->value = value;
          ast->token.v.i = symbol_index;
          ast->konst = is_branch_konst_eval(ast);
          return type;
        }
        compile_error_at(c, ast->token, "symbol `%.*s` has already been declared\n", ast->token.length, ast->token.buffer);
        return TypeNone;
      }
      typecheck_error_at(c, ast->token, "invalid type in memory statement\n");
      return TypeNone;
    }
    case AstAssignment: {
      Compile_type a = typecheck(c, block, fs, ast->node[0]);
      typecheck(c, block, fs, ast->node[1]);
      if (a == TypeUnsigned64 || a == TypeAny) {
        vs_pop(c, NULL);
        vs_pop(c, NULL);
        ts_pop(c);
        return ts_pop(c);
      }
      typecheck_error_at(c, ast->node[0]->token, "type mismatch in assignment expression\n");
      return TypeNone;
    }
    case AstSizeof: {
      Token* t = &ast->token;
      u64 size = 0;
      switch (t->type) {
        case T_IDENTIFIER: {
          Symbol* symbol = NULL;
          i32 symbol_index = -1;
          if (compile_lookup_value(c, block, fs, *t, &symbol, &symbol_index, NULL) == NoError) {
            size = symbol->size;
          }
          else {
            typecheck_error_at(c, *t, "symbol `%.*s` not defined\n", t->length, t->buffer);
            return TypeNone;
          }
          break;
        }
        case T_UNSIGNED64:
        case T_NUMBER: {
          size = sizeof(u64);
          break;
        }
        case T_UNSIGNED32: {
          size = sizeof(u32);
          break;
        }
        case T_CSTRING:
        case T_CSTR:
        case T_ANY: {
          size = sizeof(void*);
          break;
        }
        default: {
          break;
        }
      }
      Value v = { .num = size, .konst = 1, };
      vs_push(c, v);
      t->v.num = size;
      ast->konst = 1;
      return ts_push(c, TypeUnsigned64);
    }
    case AstEnum: {
      Ast* enum_node = ast->node[0];
      Value value = { .num = 0, };
      for (u32 i = 0; i < enum_node->count; ++i, ++value.num) {
        Ast* node = enum_node->node[i];
        Symbol* symbol = NULL;
        i32 symbol_index = -1;
        Compile_type type = TypeUnsigned64;
        i32 imm = ir_push_value(c, &value.num, sizeof(value.num));
        if (compile_declare_value(c, block, fs, node->token, &symbol, node, &symbol_index) == NoError) {
          symbol->imm = imm;
          symbol->size = compile_type_size[type];
          symbol->konst = 1;
          symbol->sym_type = (const Symbol_type[]){SYM_LOCAL_VAR, SYM_GLOBAL_VAR}[block == &c->global];
          symbol->type = type;
          symbol->value = value;
        }
        else {
          compile_error_at(c, node->token, "symbol `%.*s` has already been declared\n", node->token.length, node->token.buffer);
          return TypeNone;
        }
      }
      break;
    }
    case AstType: {
      if (ast->count > 0) {
        typecheck_node_list(c, block, fs, ast);
      }
      break;
    }
    case AstCastExpression: {
      Ast* expr = ast->node[0];
      Ast* type_expr = ast->node[1];
      typecheck(c, block, fs, expr);
      ts_pop(c);
      Compile_type cast_type = token_to_compile_type(type_expr->token);
      ts_push(c, cast_type);
      break;
    }
    default: {
      assert(0);
      break;
    }
  }
  return ts_top(c);
}

Compile_type typecheck_node_list(Compile* c, Block* block, Function* fs, Ast* ast) {
  for (u32 i = 0; i < ast->count; ++i) {
    typecheck(c, block, fs, ast->node[i]);
  }
  return TypeNone;
}

Compile_type typecheck_let_statement(Compile* c, Block* block, Function* fs, Ast* ast) {
  i32 konst = ast->type == AstConstStatement;
  i32 ts_count = c->ts_count;
  i32 num_elements = 1;
  Ast* rhs = ast->node[0];
  Ast* ast_type = NULL;
  if (ast->count == 2) {
    ast_type = ast->node[1];
  }
  // typecheck the rhs of the statement
  typecheck_node_list(c, block, fs, rhs);
  ast->konst = rhs->konst = is_branch_konst_eval(rhs);

  // make sure that rhs actually produced any values
  i32 ts_delta = c->ts_count - ts_count;
  if (ts_delta == 0) {
    typecheck_error_at(c, ast->token, "no value was produced in the rhs of the let statement\n");
    return TypeNone;
  }

  // typecheck explicit type, if there is one
  Compile_type explicit_type = -1;
  if (ast_type) {
    explicit_type = token_to_compile_type(ast_type->token);
    if (ast_type->count > 0) { // array specifier
      typecheck(c, block, fs, ast_type);
      Value value;
      vs_pop(c, &value);
      Compile_type array_specifier_type = ts_pop(c);
      if (array_specifier_type != TypeUnsigned64) {
        typecheck_error_at(c, ast_type->node[0]->token, "only numeric values are allowed in array size specifier\n");
        return TypeNone;
      }
      num_elements = (i32)value.num;
      if (ts_delta > num_elements) {
        typecheck_error_at(c, ast_type->node[0]->token, "number of elements in rhs exceeded the array size specifier\n");
        return TypeNone;
      }
    }
  }

  i32 imm = -1; // index to immediate value, in case this is a constant

  Value value = vs_top(c);
  Value prev_value = value;

  Compile_type type = ts_top(c);
  Compile_type prev_type = type;

  if (konst && !typecheck_is_numerical(c, type)) {
    typecheck_error_at(c, ast->token, "only numeric values are allowed in constants\n");
    return TypeNone;
  }

  // TODO(lucas): storing of constants of different types. right now only unsigned 64-bit integers works properly
  for (i32 i = 0; i < ts_delta; ++i) {
    type = ts_pop(c);
    if (type != prev_type && explicit_type != TypeAny) {
      typecheck_error_at(c, ast->token, "incompatible type in expression list\n");
      return TypeNone;
    }
    prev_type = type;
    vs_pop(c, &value);
    if (type == TypeFunc && explicit_type != TypeAny) {
      if (!check_func_signatures(c, &value.func, &prev_value.func)) {
        typecheck_error_at(c, ast->token, "incompatible type in expression list\n");
        return TypeNone;
      }
    }
    prev_value = value;
    if (konst) {
      imm = ir_push_value(c, &value.num, sizeof(value.num));
    }
  }

  // no array specifier was used, therefore num_elements is set to however many elements there was in the rhs
  if (num_elements == 1) {
    num_elements = ts_delta;
  }
  if ((i32)explicit_type != -1) {
    type = explicit_type;
  }

  // if this is a const, make sure to update the immediate value index
  if (konst) {
    imm -= (ts_delta - 1) * compile_type_size[TypeUnsigned64];
  }

  // store symbol
  Symbol* symbol = NULL;
  i32 symbol_index = -1;
  if (compile_declare_value(c, block, fs, ast->token, &symbol, ast, &symbol_index) == NoError) {
    symbol->imm = imm;
    symbol->size = num_elements * compile_type_size[type];
    symbol->num_elemements_init = ts_delta;
    symbol->konst = konst;
    symbol->sym_type = (const Symbol_type[]){SYM_LOCAL_VAR, SYM_GLOBAL_VAR}[block == &c->global];
    symbol->type = type;
    symbol->value = value;
    ast->token.v.i = symbol_index;
    return type;
  }
  compile_error_at(c, ast->token, "symbol `%.*s` has already been declared\n", ast->token.length, ast->token.buffer);
  return TypeNone;
}

u32 typecheck_is_numerical(Compile* c, Compile_type type) {
  (void)c; // unused for now
  return type == TypeUnsigned64 || type == TypeUnsigned32;
}

Compile_type ts_push(Compile* c, Compile_type type) {
  if (c->ts_count >= 0 && c->ts_count < MAX_TYPE_STACK) {
    c->ts[c->ts_count++] = type;
    return type;
  }
  assert("type stack list capacity exceeded" && 0);
  return TypeNone;
}

Compile_type ts_pop(Compile* c) {
  if (c->ts_count > 0) {
    return c->ts[--c->ts_count];
  }
  --c->ts_count;
  return TypeNone;
}

Compile_type ts_top(Compile* c) {
  if (c->ts_count > 0) {
    return c->ts[c->ts_count - 1];
  }
  return c->ts[0];
}

i32 vs_push(Compile* c, Value v) {
  if (c->vs_count >= 0 && c->vs_count < MAX_VALUE_STACK) {
    c->vs[c->vs_count++] = v;
    return NoError;
  }
  typecheck_error(c, "type stack underflow\n");
  return Error;
}

i32 vs_pop(Compile* c, Value* v) {
  if (c->vs_count > 0) {
    --c->vs_count;
    if (v) {
      *v = c->vs[c->vs_count];
    }
    return NoError;
  }
  typecheck_error(c, "value type stack underflow (current type stack = %d)\n", c->ts_count);
  --c->vs_count;
  return Error;
}

Value vs_top(Compile* c) {
  if (c->vs_count > 0) {
    return c->vs[c->vs_count - 1];
  }
  return c->vs[0];
}

// checks for function signature equality
i32 check_func_signatures(Compile* c, Function* a, Function* b) {
  if ((a->argc != b->argc) || (a->rtype != b->rtype)) {
    return 0;
  }
  for (u32 i = 0; i < a->argc; ++i) {
    const Symbol* arg_a = &c->symbols[a->args[i]];
    const Symbol* arg_b = &c->symbols[b->args[i]];
    if (arg_a->type != arg_b->type) {
      return 0;
    }
  }
  return 1;
}

u32 is_branch_konst_eval(Ast* ast) {
  u32 konst = 1;
  for (u32 i = 0; i < ast->count; ++i) {
    Ast* node = ast->node[i];
    if (!node->konst) {
      konst = 0;
      break;
    }
  }
  return konst;
}

void ir_compile_warning(Compile* c, const char* fmt, ...) {
  if (!enable_warnings) {
    return;
  }
  (void)c; // unused
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stdout;
  fprintf(fp, "[ir-compile-warning]: %s", buffer);
}

void ir_compile_warning_at(Compile* c, Token token, const char* fmt, ...) {
  if (!enable_warnings) {
    return;
  }
  (void)c; // unused
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stdout;
  fprintf(fp, "[ir-compile-warning]: %s:%d:%d: %s", token.filename, token.line, token.column, buffer);
  printline(fp, token.source, token.buffer + token.length, token.length, 1, 1);
}

void ir_print(Compile* c, FILE* fp) {
  fprintf(fp, "%s:\n", __FUNCTION__);
  for (u32 i = 0; i < c->ins_count; ++i) {
    Op* op = &c->ins[i];
    fprintf(fp, "%3u: <%s, %d, %d, %d>\n", i, ir_code_str[op->i], op->dest, op->src0, op->src1);
  }
}

// ir binary layout:
// --------------+----------------+-----
// id            | size in bytes  | type
// --------------+----------------+-----
// IR_MAGIC      | 4              | u8
// imm size      | 4              | u32
// cstr size     | 4              | u32
// bss+data size | 4              | u32
// entry point   | 4              | u32
// func count    | 4              | u32
// ins count     | 4              | u32
// imm data      | imm size       | *
// data+bss      | data+cstr size | *
// ins           | 4*ins count    | Op*
// id map size   | 4              | u32
// id map        | 4*id map size  | u32*
//
// data layout (one element):
// { type : Compile_type, size : u32, [list of data elements] }
//
// bss data layout (one element):
// { type : Compile_type, size : u32, pre-allocated memory based on the size }
void ir_binary_output(Compile* c, FILE* fp) {
  u32 write_index = 0;
#define o(_ptr, _size, _count) fwrite(_ptr, _size, _count, fp); write_index += (_size * _count)

  struct {
    u32 data;
    u32 cstring;
    u32 ins;
  } addr = {
    .data = 0,
    .cstring = 0,
    .ins = 0,
  };

  // write spl ir magic constant
  const u8 IR_MAGIC[] = {'S', 'P', 'L', '0'};
  o(&IR_MAGIC, ARR_SIZE(IR_MAGIC), 1);

  u32 imm_size = c->imm_index;
  o(&imm_size, sizeof(imm_size), 1);

  u32 id_map[MAX_CSTRING + MAX_SYMBOL] = {0};
  u32 id_map_size = 0;

  u32 data_size = 0;
  u32 cstr_size = 0;
  u32 func_count = 1; // count entry point as well
  for (u32 i = 0; i < c->cstring_count; ++i) {
    u8* cstring_address = &c->imm[c->cstrings[i]];
    u32 length = *(u32*)cstring_address;
    cstr_size += length + 1 + sizeof(length);
  }
  for (u32 i = 0; i < c->symbol_count; ++i) {
    Symbol* s = &c->symbols[i];
    if (s->type == TypeSyscallFunc || s->type == TypeFunc) {
      func_count++;
      continue;
    }
    if (s->sym_type != SYM_LOCAL_VAR) {
      continue;
    }
    data_size += sizeof(s->type) + sizeof(s->size) + s->size;
  }

  o(&cstr_size, sizeof(cstr_size), 1);
  o(&data_size, sizeof(data_size), 1);
  o(&c->entry_point_address, sizeof(c->entry_point_address), 1);
  o(&func_count, sizeof(func_count), 1);
  o(&c->ins_count, sizeof(c->ins_count), 1);

  // write immediate data
  o(&c->imm[0], imm_size, 1);

  addr.cstring = write_index;
  // write cstring data
  for (u32 i = 0; i < c->cstring_count; ++i) {
    u8* buffer = &c->imm[c->cstrings[i]];
    u32 length = *(u32*)buffer + 1;
    buffer += sizeof(length);
    o(&length, sizeof(length), 1);
    o(buffer, length - 1, 1);
    u8 null = 0;
    o(&null, sizeof(null), 1);
  }

  addr.data = write_index;
  // write data section
  for (u32 i = 0; i < c->symbol_count; ++i) {
    Symbol* s = &c->symbols[i];
    Value* v = &s->value;
    if (s->sym_type != SYM_LOCAL_VAR) {
      continue;
    }
    if (s->konst) {
      switch (s->type) {
        case TypeUnsigned64: {
          o(&s->type, sizeof(s->type), 1);
          o(&s->size, sizeof(s->size), 1);
          o(&v->num, sizeof(v->num), 1);
          break;
        }
        default: {
          assert("type not implemented yet" && 0);
          break;
        }
      }
    }
  }

  // write bss section
  for (u32 i = 0; i < c->symbol_count; ++i) {
    Symbol* s = &c->symbols[i];
    if (s->sym_type == SYM_LOCAL_VAR && s->konst == 0) {
      o(&s->type, sizeof(s->type), 1);
      o(&s->size, sizeof(s->size), 1);
      // pre-allocate memory for this data based on the specified size
      for (i32 m = 0; m < s->size; ++m) {
        u8 byte = 0;
        o(&byte, sizeof(byte), 1);
      }
    }
  }

  addr.ins = write_index;
  // write ir op codes (instructions)
  for (u32 i = 0; i < c->ins_count; ++i) {
    Op* op = &c->ins[i];
    o(op, sizeof(Op), 1);
  }

  // absolute addresses within the ir layout
  for (u32 i = 0; i < c->symbol_count; ++i) {
    Symbol* s = &c->symbols[i];
    u32 address = 0;
    switch (s->type) {
      case TypeFunc: {
        Function* func = &s->value.func;
        address = addr.ins + func->ir_address;
        break;
      }
      case TypeCString: {
        // skip for now
        break;
      }
      default: {
        address = i;
        break;
      }
    }
    id_map[id_map_size] = address;
    ++id_map_size;
  }
  o(&id_map_size, sizeof(id_map_size), 1);
  o(&id_map[0], id_map_size * sizeof(id_map[0]), 1);
#undef o
}

i32 ir_push_ins(Compile* c, Op ins, u32* ins_count) {
  list_push(c->ins, c->ins_count, ins);
  if (ins_count) {
    (*ins_count)++;
  }
  return NoError;
}

i32 ir_pop_ins(Compile* c, Op* ins, u32* ins_count) {
  if (c->ins_count > 0) {
    list_shrink(c->ins, c->ins_count, 1);
    if (ins_count) {
      (*ins_count)--;
    }
    *ins = c->ins[c->ins_count];
    return NoError;
  }
  return Error;
}

i32 ir_push_value(Compile* c, void* value, u32 size) {
  i32 address = c->imm_index;
  if (address + size < MAX_STATIC_DATA) {
    memcpy(&c->imm[c->imm_index], value, size);
    c->imm_index += size;
    return address;
  }
  assert("out of immediate data memory" && 0);
  return -1;
}

i32 ir_push_cstring(Compile* c, char* buffer, u32 length, u32* cstring_index) {
  i32 cstring_address = ir_push_value(c, &length, sizeof(length));
  i32 value_address = ir_push_value(c, buffer, length);
  if (cstring_address >= 0 && value_address > cstring_address) {
    if (c->cstring_count < MAX_CSTRING) {
      *cstring_index = c->cstring_count;
      c->cstrings[c->cstring_count++] = cstring_address;
      return cstring_address;
    }
    return -1;
  }
  return -1;
}

i32 ir_push_symbol(Compile* c, Function* fs, Symbol* symbol, i32 id, u32* ins_count) {
  switch (symbol->sym_type) {
    case SYM_FUNC: {
      Function* func = &symbol->value.func;
      ir_push_ins(c, (Op) {
        .i = I_PUSH,
        .dest = symbol->type,
        .src0 = func->label,
        .src1 = -1,
      }, ins_count);
      break;
    }
    case SYM_LOCAL_VAR: {
      if (symbol->konst) {
        ir_push_ins(c, (Op) {
          .i = I_PUSH,
          .dest = symbol->type,
          .src0 = id,
          .src1 = -1,
        }, ins_count);
        break;
      }
      ir_push_ins(c, (Op) {
        .i = I_PUSH_LOCAL,
        .dest = symbol->type,
        .src0 = ((1 + fs->argc) * 0x8) + symbol->local_id,
        .src1 = -1,
      }, ins_count);
      break;
    }
    case SYM_GLOBAL_VAR: {
      ir_push_ins(c, (Op) {
        .i = I_PUSH,
        .dest = symbol->type,
        .src0 = id,
        .src1 = -1,
      }, ins_count);
      break;
    }
    case SYM_FUNC_ARG: {
      i32 arg = symbol->token.v.i;
      ir_push_ins(c, (Op) {
        .i = I_PUSH_LOCAL,
        .dest = symbol->type,
        .src0 = 0x8 + (0x8 * arg),
        .src1 = -1,
      }, ins_count);
      break;
    }
    default: {
      assert(0);
      break;
    }
  }
  return NoError;
}

i32 ir_start_compile(Compile* c, Ast* ast) {
  REAL_TIMER_START();
  assert("something went very wrong" && ast->type == AstRoot && ast);
  Block* block = &c->global;
  for (u32 i = 0; i < ast->count; ++i) {
    if (ir_compile(c, block, NULL, ast->node[i], NULL) != NoError) {
      break;
    }
  }
  if (c->entry_point != 1) {
    compile_error(c, "missing entry point `main`\n");
  }
  REAL_TIMER_END(
    print_info("ir code generation took %lf seconds\n", _dt);
    (void)_dt;
  );
  return c->status;
}

i32 ir_compile(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count) {
  switch (ast->type) {
    case AstValue: {
      switch (ast->token.type) {
        case T_NUMBER: {
          i32 imm = ir_push_value(c, &ast->token.v.num, sizeof(ast->token.v.num));
          if (imm >= 0) {
            ir_push_ins(c, (Op) {
              .i = I_PUSH_IMM,
              .dest = TypeUnsigned64,
              .src0 = imm,
              .src1 = -1,
            }, ins_count);
          }
          else {
            assert("out of memory" && 0); // TODO(lucas): Handle
          }
          break;
        }
        case T_CSTRING: {
          u32 cstring_index = 0;
          i32 cstring_address = ir_push_cstring(c, ast->token.buffer, ast->token.length, &cstring_index);
          if (cstring_address >= 0) {
            ir_push_ins(c, (Op) {
              .i = I_PUSH,
              .dest = TypeCString,
              .src0 = cstring_index,
              .src1 = -1,
            }, ins_count);
            break;
          }
          assert("out of memory" && 0); // TODO(lucas): Handle
          break;
        }
        case T_POP: {
          ir_push_ins(c, OP(I_POP), ins_count);
          break;
        }
        case T_IDENTIFIER: {
          i32 id = ast->token.v.i;
          Symbol* symbol = &c->symbols[id];
          if (symbol->ref_count == 0 && !disable_dce && dce_all) {
            break;
          }
          ir_push_symbol(c, fs, symbol, id, ins_count);
          break;
        }
        case T_AT: {
          i32 id = ast->token.v.i;
          Symbol* symbol = &c->symbols[id];
          if (symbol->ref_count == 0 && !disable_dce && dce_all) {
            break;
          }
          switch (symbol->sym_type) {
            case SYM_FUNC: {
              ir_push_ins(c, (Op) {
                .i = I_PUSH_ADDR_OF,
                .dest = symbol->type,
                .src0 = id,
                .src1 = -1,
              }, ins_count);
              break;
            }
            case SYM_LOCAL_VAR: {
              if (symbol->konst) {
                ir_push_ins(c, (Op) {
                  .i = I_PUSH_ADDR_OF,
                  .dest = symbol->type,
                  .src0 = id,
                  .src1 = -1,
                }, ins_count);
                break;
              }
              ir_push_ins(c, (Op) {
                .i = I_PUSH_LOCAL_ADDR_OF,
                .dest = symbol->type,
                .src0 = ((1 + fs->argc) * 0x8) + symbol->local_id,
                .src1 = -1,
              }, ins_count);
              break;
            }
            case SYM_GLOBAL_VAR: {
              ir_push_ins(c, (Op) {
                .i = I_PUSH_ADDR_OF,
                .dest = symbol->type,
                .src0 = id,
                .src1 = -1,
              }, ins_count);
              break;
            }
            case SYM_FUNC_ARG: {
              id = symbol->token.v.i;
              ir_push_ins(c, (Op) {
                .i = I_PUSH_LOCAL_ADDR_OF,
                .dest = symbol->type,
                .src0 = 0x8 + 0x8 * id,
                .src1 = -1,
              }, ins_count);
              break;
            }
            default: {
              assert(0);
              break;
            }
          }
          break;
        }
        default: {
          assert(!"value token not implemented");
          break;
        }
      }
      break;
    }
    case AstExpression:
    case AstExprList:
    case AstStatement:
    case AstStatementList: {
      return ir_compile_stmts(c, block, fs, ast, ins_count);
    }
    // op arg0 arg1 -> arg0 arg1 op
    case AstBinopExpression: {
      i32 result = ir_compile_binop(c, block, fs, ast, ins_count);
      if (result == NoError) {
        if (ast->token.type == T_ADD) {
          ir_push_ins(c, OP(I_ADD), ins_count);
        }
        else if (ast->token.type == T_SUB) {
          ir_push_ins(c, OP(I_SUB), ins_count);
        }
        else if (ast->token.type == T_MUL) {
          ir_push_ins(c, OP(I_MUL), ins_count);
        }
        else if (ast->token.type == T_DIV) {
          ir_push_ins(c, OP(I_DIV), ins_count);
        }
        else if (ast->token.type == T_DIVMOD) {
          ir_push_ins(c, OP(I_DIVMOD), ins_count);
        }
        else if (ast->token.type == T_LSHIFT) {
          ir_push_ins(c, OP(I_LSHIFT), ins_count);
        }
        else if (ast->token.type == T_RSHIFT) {
          ir_push_ins(c, OP(I_RSHIFT), ins_count);
        }
        else if (ast->token.type == T_LT) {
          ir_push_ins(c, OP(I_LT), ins_count);
        }
        else if (ast->token.type == T_GT) {
          ir_push_ins(c, OP(I_GT), ins_count);
        }
        else if (ast->token.type == T_AND) {
          ir_push_ins(c, OP(I_AND), ins_count);
        }
        else if (ast->token.type == T_OR) {
          ir_push_ins(c, OP(I_OR), ins_count);
        }
        else if (ast->token.type == T_EQ) {
          ir_push_ins(c, OP(I_EQ), ins_count);
        }
        else if (ast->token.type == T_NEQ) {
          ir_push_ins(c, OP(I_NEQ), ins_count);
        }
        else {
          assert(!"operation not implemented yet");
        }
      }
      else {
        c->status = result;
        return c->status;
      }
      break;
    }
    case AstUopExpression: {
      i32 result = ir_compile_uop(c, block, fs, ast, ins_count);
      if (result == NoError) {
        if (ast->token.type == T_PRINT) {
          ir_push_ins(c, OP(I_PRINT), ins_count);
        }
        else if (ast->token.type == T_DEREF) {
          ir_push_ins(c, OP(I_LOAD64), ins_count);
        }
        else if (ast->token.type == T_LOAD64) {
          ir_push_ins(c, OP(I_LOAD64), ins_count);
        }
        else if (ast->token.type == T_LOAD32) {
          ir_push_ins(c, OP(I_LOAD32), ins_count);
        }
        else if (ast->token.type == T_LOAD16) {
          ir_push_ins(c, OP(I_LOAD16), ins_count);
        }
        else if (ast->token.type == T_LOAD8) {
          ir_push_ins(c, OP(I_LOAD8), ins_count);
        }
        else if (ast->token.type == T_LOGICAL_NOT) {
          ir_push_ins(c, OP(I_LOGICAL_NOT), ins_count);
        }
        else {
          assert(!"operation not implemented yet");
        }
      }
      else {
        c->status = result;
        return c->status;
      }
      break;
    }
    case AstNone: {
      ir_compile_warning(c, "unused AST branch type\n");
      break;
    }
    case AstConstStatement: {
      // NOTE(lucas): value has already been defined in the type-checking phase
      break;
    }
    case AstLetStatement: {
      i32 id = ast->token.v.i;
      Symbol* symbol = &c->symbols[id];
      if (symbol->ref_count == 0 && !disable_dce && dce_all) {
        break;
      }
      Ast* node = ast->node[0];
      u32 count = ast_count(node);
      if (count == 1) {
        symbol->token = node->token;
      }
      if (symbol->type == TypeCString) {
        // NOTE(lucas): we change the type from CString -> Any because symbol is pointing to a string
        symbol->type = TypeAny;
      }
      // NOTE(lucas): completely ignore the rhs of the let statement if it is in the global scope
      // TODO(lucas): figure out how to deal with globals when it comes to their value assignment(s)
      // TODO(lucas): align to an 8 byte boundary
      // TODO(lucas): FIXME: cannot use the type size to change the locals offset counter, because the stack grows down (in the negative direction, if you will)
      if (fs != NULL) {
        i32 local_id = fs->locals_offset_counter;
        if (ir_compile_stmts(c, block, fs, ast, ins_count) == NoError) {
          u32 type_size = compile_type_size[symbol->type];
          u32 count = symbol->size / type_size;
          // fs->locals_offset_counter += type_size * count;
          fs->locals_offset_counter += sizeof(u64) * count; // NOTE(lucas): this is a quick fix to align the memory to an 8 byte boundary
          for (u32 i = 0; i < symbol->num_elemements_init; ++i) {
            ir_push_ins(c, (Op) {
              .i = I_MOVE_LOCAL,
              .dest = symbol->type,
              .src0 = ((1 + fs->argc) * 0x8) + local_id,
              .src1 = -1,
            }, ins_count);
            symbol->local_id = local_id;
            local_id += type_size;
          }
        }
      }
      break;
    }
    case AstFuncDefinition: {
      assert(ast->count == 2 || ast->count == 3);
      Block local_block;
      block_init(&local_block, block);
      ir_compile_func(c, &local_block, fs, ast, ins_count);
      break;
    }
    case AstParamList: {
      assert("something went very wrong" && 0);
      break;
    }
    case AstFuncCall: {
      i32 id = ast->token.v.i;
      Symbol* symbol = &c->symbols[id];
      if (symbol->ref_count == 0 && !disable_dce && dce_all) {
        break;
      }
      Function* func = &symbol->value.func;
      ir_compile_func_args(c, block, fs, ast->node[0], ins_count); // compile function args in reverse order
      // TODO(lucas): fully implement
      switch (symbol->type) {
        case TypeAny:
        case TypeFunc: {
          switch (symbol->sym_type) {
            case SYM_FUNC: {
              ir_push_ins(c, (Op) {
                .i = I_CALL,
                .dest = func->label, 
                .src0 = func->argc,
                .src1 = ((const i32[]){0, -1})[func->rtype == TypeNone],
              }, ins_count);
              break;
            }
            case SYM_LOCAL_VAR: {
              if (symbol->konst) {
                ir_push_ins(c, (Op) {
                  .i = I_PUSH,
                  .dest = symbol->type,
                  .src0 = id,
                  .src1 = -1,
                }, ins_count);
              }
              else {
                ir_push_ins(c, (Op) {
                  .i = I_PUSH_LOCAL,
                  .dest = symbol->type,
                  .src0 = ((1 + fs->argc) * 0x8) + symbol->local_id,
                  .src1 = -1,
                }, ins_count);
              }
              ir_push_ins(c, (Op) {
                .i = I_ADDR_CALL,
                .dest = -1,
                .src0 = func->argc,
                .src1 = ((const i32[]){0, -1})[func->rtype == TypeNone],
              }, ins_count);
              break;
            }
            case SYM_GLOBAL_VAR: {
              ir_push_ins(c, (Op) {
                .i = I_PUSH,
                .dest = symbol->type,
                .src0 = id,
                .src1 = -1,
              }, ins_count);
              ir_push_ins(c, (Op) {
                .i = I_ADDR_CALL,
                .dest = -1,
                .src0 = func->argc,
                .src1 = ((const i32[]){0, -1})[func->rtype == TypeNone],
              }, ins_count);
              break;
            }
            case SYM_FUNC_ARG: {
              i32 arg = symbol->token.v.i;
              ir_push_ins(c, (Op) {
                .i = I_PUSH_LOCAL,
                .dest = symbol->type,
                .src0 = 0x8 + (0x8 * arg),
                .src1 = -1,
              }, ins_count);
              ir_push_ins(c, (Op) {
                .i = I_ADDR_CALL,
                .dest = -1,
                .src0 = func->argc,
                .src1 = ((const i32[]){0, -1})[func->rtype == TypeNone],
              }, ins_count);
              break;
            }
            default: {
              assert(0);
              break;
            }
          }
          break;
        }
        case TypeSyscallFunc: {
          const i32 syscall_map[] = {
            I_SYSCALL0,
            I_SYSCALL1,
            I_SYSCALL2,
            I_SYSCALL3,
            I_SYSCALL4,
            I_SYSCALL5,
            I_SYSCALL6,
          };
          ir_push_ins(c, OP(syscall_map[func->argc - 1]), ins_count);
          break;
        }
        default: {
          compile_error_at(c, ast->token, "symbol `%s` is not a function, and can not be called\n", symbol->name);
          return c->status;
        }
      }
      break;
    }
    case AstBlockStatement: {
      ir_compile_stmts(c, block, fs, ast, ins_count);
      break;
    }
    case AstMemoryStatement: {
      // NOTE(lucas): this is handled in the type checking phase
      break;
    }
    case AstAssignment: {
      i32 result = ir_compile_binop(c, block, fs, ast, ins_count);
      if (result == NoError) {
        switch (ast->token.type) {
          case T_ASSIGN:
          case T_STORE64:
            ir_push_ins(c, OP(I_STORE64), ins_count);
            break;
          case T_STORE32:
            ir_push_ins(c, OP(I_STORE32), ins_count);
            break;
          case T_STORE16:
            ir_push_ins(c, OP(I_STORE16), ins_count);
            break;
          case T_STORE8:
            ir_push_ins(c, OP(I_STORE8), ins_count);
            break;
          default:
            assert(0);
            break;
        }
      }
      else {
        c->status = result;
        return c->status;
      }
      break;
    }
    case AstWhileStatement: {
      assert("invalid while statement construction" && ast->count == 2);
      i32 loop_label = c->label_count++;
      u32 cond_size = 0;
      u32 body_size = 0;
      ir_push_ins(c, (Op) { .i = I_LOOP_LABEL, .dest = loop_label, .src0 = -1, .src1 = -1, }, &cond_size);

      Ast* cond = ast->node[0];
      Ast* body = ast->node[1];

      if (ir_compile_stmts(c, block, fs, cond, &cond_size) == NoError) {
        i32 body_start_address = c->ins_count;
        i32 loop_end_label = c->label_count++;
        ir_push_ins(c, (Op) { .i = I_JZ, .dest = loop_end_label, .src0 = 0, .src1 = 0, }, &body_size); // conditional jump if zero
        if (ir_compile_stmts(c, block, fs, body, &body_size) == NoError) {
          ir_push_ins(c, (Op) {
            .i = I_JMP,
            .dest = loop_label,
            .src0 = -(i32)(cond_size + body_size),
            .src1 = -1,
          }, &body_size);
          ir_push_ins(c, (Op) { .i = I_LOOP_LABEL, .dest = loop_end_label, .src0 = -1, .src1 = -1, }, &body_size);
          Op* jz = &c->ins[body_start_address]; // NOTE(lucas): this is not really nessesary because we are using label jumps. but we are keeping this in case we are adding support for other code generators.
          jz->src0 = body_size;
        }
        else {
          return c->status;
        }
      }
      else {
        return c->status;
      }
      if (ins_count) {
        *ins_count += cond_size + body_size;
      }
      break;
    }
    case AstIfStatement: {
      assert("invalid if statement construction" && (ast->count >= 2 && ast->count <= 3));
      u32 cond_size = 0;
      u32 body_size = 0;
      u32 else_body_size = 0;

      Ast* cond = ast->node[0];
      Ast* body = ast->node[1];

      if (ir_compile_stmts(c, block, fs, cond, &cond_size) == NoError) {
        i32 body_start_address = c->ins_count;
        i32 end_label = c->label_count++;
        ir_push_ins(c, (Op) { .i = I_JZ, .dest = end_label, .src0 = 0, .src1 = 0, }, &body_size);
        if (ir_compile_stmts(c, block, fs, body, &body_size) == NoError) {
          if (ast->count == 3) { // else body
            Ast* else_body = ast->node[2];
            i32 else_label = c->label_count++;
            i32 else_start_address = c->ins_count;
            ir_push_ins(c, (Op) {
              .i = I_JMP,
              .dest = else_label,
              .src0 = -1,
              .src1 = -1,
            }, &body_size);
            ir_push_ins(c, (Op) { .i = I_LOOP_LABEL, .dest = end_label, .src0 = -1, .src1 = -1, }, &else_body_size);
            if (ir_compile_stmts(c, block, fs, else_body, &else_body_size) == NoError) {
              ir_push_ins(c, (Op) { .i = I_LOOP_LABEL, .dest = else_label, .src0 = -1, .src1 = -1, }, &else_body_size);
              Op* jz = &c->ins[body_start_address];
              jz->src0 = else_body_size;
              Op* else_jmp = &c->ins[else_start_address];
              else_jmp->src0 = (i32)else_body_size;
            }
            else {
              return c->status;
            }
          }
          else {
            ir_push_ins(c, (Op) {
              .i = I_LOOP_LABEL,
              .dest = end_label,
              .src0 = -1,
              .src1 = -1,
            }, &body_size);
            Op* jz = &c->ins[body_start_address];
            jz->src0 = body_size;
          }
        }
        else {
          return c->status;
        }
      }
      else {
        return c->status;
      }
      if (ins_count) {
        *ins_count += cond_size + body_size + else_body_size;
      }
      break;
    }
    case AstSizeof: {
      i32 imm = ir_push_value(c, &ast->token.v.num, sizeof(ast->token.v.num));
      if (imm >= 0) {
        ir_push_ins(c, (Op) {
          .i = I_PUSH_IMM,
          .dest = TypeUnsigned64,
          .src0 = imm,
          .src1 = -1,
        }, ins_count);
      }
      break;
    }
    case AstEnum:
    case AstType:
      break;
    case AstCastExpression: {
      ir_compile(c, block, fs, ast->node[0], ins_count);
      break;
    }
    default: {
      compile_error(c, "invalid or unhandled AST branch type `%d`\n", ast->type);
      c->status = Error;
      break;
    }
  }
  return c->status;
}

i32 ir_compile_stmts(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count) {
  for (u32 i = 0; i < ast->count; ++i) {
    if (ir_compile(c, block, fs, ast->node[i], ins_count) != NoError) {
      break;
    }
  }
  return c->status;
}

i32 ir_compile_func_args(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count) {
  for (i32 i = ast->count - 1; i >= 0; --i) {
    if (ir_compile(c, block, fs, ast->node[i], ins_count) != NoError) {
      break;
    }
  }
  return c->status;
}

i32 ir_compile_binop(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count) {
  if (ast->count < 2) {
    compile_error_at(c, ast->token, "expected 2 arguments in binary operator expression\n");
    return c->status;
  }
  for (u32 i = 0; i < ast->count; ++i) {
    Ast* node = ast->node[i];
    i32 result = ir_compile(c, block, fs, node, ins_count);
    if (result != NoError) {
      c->status = result;
      return c->status;
    }
  }
  return c->status;
}

// alias of ir_compile_binop
i32 ir_compile_uop(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count) {
  for (u32 i = 0; i < ast->count; ++i) {
    Ast* node = ast->node[i];
    i32 result = ir_compile(c, block, fs, node, ins_count);
    if (result != NoError) {
      c->status = result;
      return c->status;
    }
  }
  return c->status;
}

i32 ir_compile_func(Compile* c, Block* block, Function* fs, Ast* ast, u32* ins_count) {
  (void)fs;
  Ast* body = ast->node[1];
  i32 id = ast->token.v.i;
  Symbol* symbol = &c->symbols[id];
  if (symbol->ref_count == 0 && !disable_dce) {
    return c->status;
  }
  Function* func = &symbol->value.func;
  ir_push_ins(c, (Op) {
    .i = I_LABEL,
    .dest = id,
    .src0 = -1,
    .src1 = -1,
  }, ins_count);
  func->ir_address = c->ins_count;
  if (!strncmp(symbol->name, "main", ast->token.length)) {
    c->entry_point_address = func->ir_address;
  }

  u32 begin_func_address = c->ins_count; // we don't know how many local variables there are in this function until the function body is compiled, therefore we store the index to the I_BEGIN_FUNC instruction so that we can change it later when we do.
  ir_push_ins(c, (Op) {
    .i = I_BEGIN_FUNC,
    .dest = -1,
    .src0 = func->argc,
    .src1 = 0,
  }, ins_count);
  u32 func_size = 0;
  ir_compile_stmts(c, block, func, body, &func_size);
  Op* begin_func = &c->ins[begin_func_address];
  begin_func->src1 = (0x8 * func->argc) + func->locals_offset_counter;
  ir_push_ins(c, (Op) {
    .i = ((i32[]){I_RET, I_NORET})[func->rtype == TypeNone],
    .dest = -1,
    .src0 = (0x8 * func->argc) + func->locals_offset_counter,
    .src1 = -1,
  }, ins_count);
  return c->status;
}

i32 compile(Compile* c, Compile_target target, FILE* fp) {
  if (target >= 0 && target < MAX_COMPILE_TARGET) {
    REAL_TIMER_START();
    compile_targets[target](c, fp);
    REAL_TIMER_END(
      print_info("code generation for `%s` took %lf seconds\n", compile_target_str[target], _dt);
      (void)_dt;
    );
  }
  else {
    assert(!"invalid compile target");
  }
  return c->status;
}

i32 compile_linux_nasm_x86_64(Compile* c, FILE* fp) {
  #define o(...) fprintf(fp, __VA_ARGS__)
#if VERBOSE_ASSEMBLY
  #define vo(...) fprintf(fp, __VA_ARGS__)
#else
  #define vo(...)
#endif
#define ENTRY "_start"
  o("bits 64\n");
  o("section .text\n");
  o("global %s\n", ENTRY);
  o(
  "print:\n"
  "mov r9, -3689348814741910323\n"
  "sub rsp, 40\n"
  "mov BYTE [rsp+31], 10\n"
  "lea rcx, [rsp+30]\n"
  ".L2:\n"
  "mov rax, rdi\n"
  "lea r8, [rsp+32]\n"
  "mul r9\n"
  "mov rax, rdi\n"
  "sub r8, rcx\n"
  "shr rdx, 3\n"
  "lea rsi, [rdx+rdx*4]\n"
  "add rsi, rsi\n"
  "sub rax, rsi\n"
  "add eax, 48\n"
  "mov BYTE [rcx], al\n"
  "mov rax, rdi\n"
  "mov rdi, rdx\n"
  "mov rdx, rcx\n"
  "sub rcx, 1\n"
  "cmp rax, 9\n"
  "ja .L2\n"
  "lea rax, [rsp+32]\n"
  "mov edi, 1\n"
  "sub rdx, rax\n"
  "xor eax, eax\n"
  "lea rsi, [rsp+32+rdx]\n"
  "mov rdx, r8\n"
  "mov rax, 1\n"
  "syscall\n"
  "add rsp, 40\n"
  "ret\n"
  );

  for (u32 i = 0; i < c->ins_count; ++i) {
    const Op* op = &c->ins[i];
    switch (op->i) {
      case I_NOP: {
        o("nop\n");
        break;
      }
      case I_POP: {
        o("pop rax\n");
        break;
      }
      case I_MOVE: {
        vo("; I_MOVE\n");
        switch (op->dest) {
          case TypeAny:
          case TypeCString:
          case TypeFunc:
          case TypeUnsigned64: {
            o("pop rax\n");
            o("mov [v%d+%d], rax\n", op->src0, op->src1);
            break;
          }
          case TypeUnsigned32: {
            o("pop rax\n");
            o("mov [v%d+%d], eax\n", op->src0, op->src1);
            break;
          }
          default: {
            assert("type not implemented yet" && 0);
            break;
          }
        }
        break;
      }
      case I_MOVE_LOCAL: {
        vo("; I_MOVE_LOCAL\n");
        switch (op->dest) {
          case TypeAny:
          case TypeCString:
          case TypeFunc:
          case TypeUnsigned64: {
            o("pop rax\n");
            o("mov [rbp-%d], rax\n", op->src0);
            break;
          }
          case TypeUnsigned32: {
            o("pop rax\n");
            o("mov DWORD [rbp-%d], eax\n", op->src0);
            break;
          }
          default: {
            assert("type not implemented yet" && 0);
            break;
          }
        }
        break;
      }
      case I_STORE64: {
        vo("; I_STORE64\n");
        o(
        "pop rbx\n"
        "pop rax\n"
        "mov [rax], rbx\n"
        );
        break;
      }
      case I_STORE32: {
        vo("; I_STORE32\n");
        o(
        "pop rbx\n"
        "pop rax\n"
        "mov [rax], ebx\n"
        );
        break;
      }
      case I_STORE16: {
        vo("; I_STORE16\n");
        o(
        "pop rbx\n"
        "pop rax\n"
        "mov [rax], bx\n"
        );
        break;
      }
      case I_STORE8: {
        vo("; I_STORE8\n");
        o(
        "pop rbx\n"
        "pop rax\n"
        "mov [rax], bl\n"
        );
        break;
      }
      case I_LOAD64: {
        vo("; I_LOAD64\n");
        o(
        "pop rax\n"
        "xor rbx, rbx\n"
        "mov rbx, [rax]\n"
        "push rbx\n"
        );
        break;
      }
      case I_LOAD32: {
        vo("; I_LOAD32\n");
        o("pop rax\n");
#if 1
        o("xor rbx, rbx\n");
        o("mov ebx, [rax]\n");
#else
        o("movzx ebx, WORD [rax]\n"); // i want to use DWORD here, but it does not seem to work? why?
#endif
        o("push rbx\n");
        break;
      }
      case I_LOAD16: {
        vo("; I_LOAD16\n");
        o(
        "pop rax\n"
        "xor rbx, rbx\n"
        "mov bx, [rax]\n"
        "push rbx\n"
        );
        break;
      }
      case I_LOAD8: {
        vo("; I_LOAD8\n");
        o(
        "pop rax\n"
        "xor rbx, rbx\n"
        "mov bl, [rax]\n"
        "push rbx\n"
        );
        break;
      }
      case I_PUSH_ADDR_OF: {
        vo("; I_PUSH_ADDR_OF\n");
        o("mov rax, v%d\n", op->src0);
        o("push rax\n");
        break;
      }
      case I_PUSH_LOCAL_ADDR_OF: {
        vo("; I_PUSH_LOCAL_ADDR_OF\n");
        switch (op->dest) {
          case TypeAny:
          case TypeCString:
          case TypeFunc:
          case TypeUnsigned32:
          case TypeUnsigned64: {
            o("lea rax, [rbp-%d]\n", op->src0);
            o("push rax\n");
            break;
          }
          default: {
            assert("I_PUSH_LOCAL_ADDR_OF: type not implemented yet" && 0);
          }
        }
        break;
      }
      case I_PUSH: {
        vo("; I_PUSH\n");
        if (op->src0 >= 0) {
          switch (op->dest) {
            case TypeAny:
            case TypeUnsigned64: {
              o("mov rax, [v%d]\n", op->src0);
              o("push rax\n");
              break;
            }
            case TypeUnsigned32: {
              o("mov eax, [v%d]\n", op->src0);
              o("push rax\n");
              break;
            }
            case TypeCString: {
              o("mov rax, str%d\n", op->src0);
              o("push rax\n");
              break;
            }
            case TypeFunc: {
              o("mov rax, v%d\n", op->src0);
              o("push rax\n");
              break;
            }
            default: {
              assert("I_PUSH: type not implemented yet" && 0);
              break;
            }
          }
        }
        break;
      }
      case I_PUSH_LOCAL: {
        vo("; I_PUSH_LOCAL\n");
        switch (op->dest) {
          case TypeAny:
          case TypeCString:
          case TypeFunc:
          case TypeUnsigned64: {
            o("push QWORD [rbp-%d]\n", op->src0);
            break;
          }
          case TypeUnsigned32: {
            o("mov eax, [rbp-%d]\n", op->src0);
            o("push rax\n");
            break;
          }
          default: {
            assert("I_PUSH_LOCAL: type not implemented yet" && 0);
            break;
          }
        }
        break;
      }
      case I_PUSH_IMM: {
        vo("; I_PUSH_IMM\n");
        switch (op->dest) {
          case TypeUnsigned64: {
            u64 value = *(u64*)&c->imm[op->src0];
            o("mov rax, %ld\n", value);
            o("push rax\n");
            break;
          }
          default: {
            assert("I_PUSH_IMM: imm type not implemented yet" && 0);
            break;
          }
        }
        break;
      }
      case I_ADD: {
        vo("; I_ADD\n");
        o(
        "pop rax\n"
        "pop rbx\n"
        "add rbx, rax\n"
        "push rbx\n"
        );
        break;
      }
      case I_SUB: {
        vo("; I_SUB\n");
        o(
        "pop rax\n"
        "pop rbx\n"
        "sub rbx, rax\n"
        "push rbx\n"
        );
        break;
      }
      case I_MUL: {
        vo("; I_MUL\n");
        o(
        "pop rax\n"
        "pop rbx\n"
        "mul rbx\n"
        "push rax\n"
        );
        break;
      }
      case I_DIV: {
        vo("; I_DIV\n");
        o(
        "xor rdx, rdx\n"
        "pop r8\n"
        "pop rax\n"
        "div r8\n"
        "push rax\n"
        );
        break;
      }
      // shl and shr requires immediate value on the second argument
      // TODO(lucas): implement
      case I_LSHIFT: {
        vo("; I_LSHIFT\n");
        assert("I_LSHIFT: instruction not implemented yet" && 0);
        break;
      }
      case I_RSHIFT: {
        vo("; I_LSHIFT\n");
        assert("I_RSHIFT: instruction not implemented yet" && 0);
        break;
      }
      case I_DIVMOD: {
        vo("; I_DIVMOD\n");
        o(
        "xor rdx, rdx\n"
        "pop rbx\n"
        "pop rax\n"
        "div rbx\n"
        "push rdx\n"
        );
        break;
      }
      case I_LT: {
        vo("; I_LT\n");
        o(
        "mov rcx, 0\n"
        "mov rdx, 1\n"
        "pop rax\n"
        "pop rbx\n"
        "cmp rbx, rax\n"
        "cmovl rcx, rdx\n"
        "push rcx\n"
        );
        break;
      }
      case I_GT: {
        vo("; I_GT\n");
        o(
        "mov rcx, 0\n"
        "mov rdx, 1\n"
        "pop rax\n"
        "pop rbx\n"
        "cmp rbx, rax\n"
        "cmovg rcx, rdx\n"
        "push rcx\n"
        );
        break;
      }
      case I_AND: {
        vo("; I_AND\n");
        o(
        "pop rax\n"
        "pop rbx\n"
        "and rbx, rax\n"
        "push rbx\n"
        );
        break;
      }
      case I_LOGICAL_NOT: {
        vo("; I_LOGICAL_NOT\n");
        o(
        "pop rax\n"
        "cmp rax, 0\n"
        "sete al\n"
        "movzx rax, al\n" // TODO(lucas): i might not have to zero-extend here, investigate
        "push rax\n"
        );
        break;
      }
      case I_OR: {
        vo("; I_OR\n");
        o(
        "pop rax\n"
        "pop rbx\n"
        "or rbx, rax\n"
        "push rbx\n"
        );
        break;
      }
      case I_EQ: {
        vo("; I_EQ\n");
        o(
        "mov rcx, 0\n"
        "mov rdx, 1\n"
        "pop rax\n"
        "pop rbx\n"
        "cmp rbx, rax\n"
        "cmove rcx, rdx\n"
        "push rcx\n"
        );
        break;
      }
      case I_NEQ: {
        vo("; I_NEQ\n");
        o(
        "mov rcx, 0\n"
        "mov rdx, 1\n"
        "pop rax\n"
        "pop rbx\n"
        "cmp rbx, rax\n"
        "cmovne rcx, rdx\n"
        "push rcx\n"
        );
        break;
      }
      case I_RET: {
        vo("; I_RET\n");
        i32 frame_size = op->src0;
        o("pop rax\n");
        o("pop rbp\n");
        o("add rsp, %d\n", frame_size);
        o("ret\n");
        break;
      }
      case I_NORET: {
        vo("; I_NORET\n");
        i32 frame_size = op->src0;
        o("pop rbp\n");
        o("add rsp, %d\n", frame_size);
        o("ret\n");
        break;
      }
      case I_PRINT: {
        vo("; I_PRINT\n");
        o(
        "pop rdi\n"
        "call print\n"
        );
        break;
      }
      case I_LABEL: {
        assert((u32)op->dest < c->symbol_count);
        Symbol* symbol = &c->symbols[op->dest];
        if (strcmp(symbol->name, "main") == 0) {
          o("%s:\n", symbol->name);
        }
        else {
          vo("; `%s`\n", symbol->name);
          o("v%d:\n", op->dest);
        }
        break;
      }
      case I_CALL: {
        vo("; I_CALL\n");
        assert(op->dest >= 0);
        i32 argc = op->src0;
        for (i32 arg = 0; arg < argc; ++arg) {
          o("pop %s\n", func_call_regs_x86_64[arg]);
        }
        o("push rbp\n");
        o("call v%d\n", op->dest);
        o("pop rbp\n");
        if (op->src1 >= 0) {
          o("push rax\n");
        }
        break;
      }
      case I_ADDR_CALL: {
        vo("; I_ADDR_CALL\n");
        i32 argc = op->src0;
        o("pop rax\n");
        for (i32 arg = 0; arg < argc; ++arg) {
          o("pop %s\n", func_call_regs_x86_64[arg]);
        }
        o("push rbp\n");
        // o("mov rbx, [v%d]\n", op->dest);
        o("call rax\n");
        o("pop rbp\n");
        if (op->src1 >= 0) {
          o("push rax\n");
        }
        break;
      }
      case I_JMP: {
        o("jmp L%d\n", op->dest);
        break;
      }
      case I_JZ: { // jump if zero
        vo("; I_JZ\n");
        o(
        "pop rax\n"
        "test rax, rax\n"
        );
        o("jz L%d\n", op->dest);
        break;
      }
      case I_BEGIN_FUNC: {
        o(
        "push rbp\n"
        "mov rbp, rsp\n"
        );
        // NOTE(lucas): can pack argc and frame_size into a single integer
        i32 argc = op->src0;
        i32 frame_size = op->src1;
        o("sub rsp, %d\n", frame_size);
        for (i32 arg = 0; arg < argc; ++arg) {
          o("mov [rbp-%d], %s\n", (arg + 1) * 0x8, func_call_regs_x86_64[arg]); // +1 because we have pushed rbp onto stack
        }
        break;
      }
      case I_LOOP_LABEL: {
        o("L%d:\n", op->dest);
        break;
      }
      case I_SYSCALL0: {
        vo("; I_SYSCALL0\n");
        o(
        "pop rax\n"
        "syscall\n"
        "push rax\n"
        );
        break;
      }
      case I_SYSCALL1: {
        vo("; I_SYSCALL1\n");
        o(
        "pop rax\n"
        "pop rdi\n"
        "syscall\n"
        "push rax\n"
        );
        break;
      }
      case I_SYSCALL2: {
        vo("; I_SYSCALL2\n");
        o(
        "pop rax\n"
        "pop rdi\n"
        "pop rsi\n"
        "syscall\n"
        "push rax\n"
        );
        break;
      }
      case I_SYSCALL3: {
        vo("; I_SYSCALL3\n");
        o(
        "pop rax\n"
        "pop rdi\n"
        "pop rsi\n"
        "pop rdx\n"
        "syscall\n"
        "push rax\n"
        );
        break;
      }
      case I_SYSCALL4: {
        vo("; I_SYSCALL4\n");
        o(
        "pop rax\n"
        "pop rdi\n"
        "pop rsi\n"
        "pop rdx\n"
        "pop r10\n"
        "syscall\n"
        "push rax\n"
        );
        break;
      }
      case I_SYSCALL5: {
        vo("; I_SYSCALL5\n");
        o(
        "pop rax\n"
        "pop rdi\n"
        "pop rsi\n"
        "pop rdx\n"
        "pop r10\n"
        "pop r8\n"
        "syscall\n"
        "push rax\n"
        );
        break;
      }
      case I_SYSCALL6: {
        vo("; I_SYSCALL6\n");
        o(
        "pop rax\n"
        "pop rdi\n"
        "pop rsi\n"
        "pop rdx\n"
        "pop r10\n"
        "pop r8\n"
        "pop r9\n"
        "syscall\n"
        "push rax\n"
        );
        break;
      }
      default: {
        fprintf(stderr, "instruction `%d` not implemented\n", op->i);
        assert(0);
        break;
      }
    }
  }

  o("\n");
  o("%s:\n", ENTRY);
  o("mov rdi, [rsp]\n");
  o("mov rax, rsp\n");
  o("add rax, 8\n");
  o("mov rsi, rax\n");
  o("push rbp\n");
  o("call main\n");
  o("pop rbp\n");
#if __APPLE__
  // macos uses different system call codes, they can be found here: https://sigsegv.pl/osx-bsd-syscalls/
  o("mov rax, 1 ; exit syscall\n");
  o("mov rdi, 0 ; return code\n");
  o("syscall\n");
#else
  o("mov rax, 60 ; exit syscall\n");
  o("mov rdi, 0  ; return code\n");
  o("syscall\n");
#endif
  o("ret\n");
  o("section .data\n");
  // TODO(lucas): make strings constants
  for (u32 i = 0; i < c->cstring_count; ++i) {
    u8* buffer = &c->imm[c->cstrings[i]];
    u32 length = *(u32*)buffer;
    buffer += sizeof(length);
    o("str%u: db ", i);
    for (u32 str_index = 0; str_index < length; ++str_index) {
      char ch = buffer[str_index];
      o("%d, ", ch);
      if (ch == '\n') {
        str_index++;
      }
    }
    o("0\n");
  }
  for (u32 i = 0; i < c->symbol_count; ++i) {
    Symbol* s = &c->symbols[i];
    if (s->ref_count == 0 && !disable_dce && dce_all) {
      continue;
    }
    if (!(s->sym_type == SYM_LOCAL_VAR || s->sym_type == SYM_GLOBAL_VAR)) {
      continue;
    }
    if (s->konst) {
      u32 size = compile_type_size[s->type];
      u32 count = s->size / size;
      switch (s->type) {
        case TypeUnsigned64: {
          assert(s->imm >= 0);
          i32 imm = s->imm + s->size - size;
          o("v%d: dq", i);
          for (u32 v = 0; v < count; ++v, imm -= size) {
            u64 value = *(u64*)&c->imm[imm];
            o(" %ld,", value);
          }
          o(" ; `%s`\n", s->name);
          break;
        }
        // TODO(lucas): all values are stored in 8 byte chunks even though a type might specify a size which is less than that. change this to reduce waste of storage.
        case TypeUnsigned32: {
          assert(s->imm >= 0);
          size = sizeof(u64);
          i32 imm = s->imm + (count * size) - size;
          o("v%d: dd", i);
          for (u32 v = 0; v < count; ++v, imm -= size) {
            u32 value = (u32)(*(u64*)&c->imm[imm]);
            o(" %d,", value);
          }
          o(" ; `%s`\n", s->name);
          break;
        }
        default: {
          assert("type not implemented yet" && 0);
          break;
        }
      }
    }
  }
  o("section .bss\n");
  for (u32 i = 0; i < c->symbol_count; ++i) {
    Symbol* s = &c->symbols[i];
    if (s->ref_count == 0 && !disable_dce && dce_all) {
      continue;
    }
    if ((s->sym_type == SYM_GLOBAL_VAR || s->sym_type == SYM_LOCAL_VAR) && s->konst == 0) {
      switch (s->type) {
        case TypeUnsigned64: {
          o("v%d: resb %d ; `%s` : Unsigned64\n", i, s->size, s->name);
          break;
        }
        case TypeUnsigned32: {
          o("v%d: resb %d ; `%s` : Unsigned32\n", i, s->size, s->name);
          break;
        }
        case TypeCString: {
          o("v%d: resb %d ; `%s` : CString\n", i, s->size, s->name);
          break;
        }
        case TypeFunc: {
          o("v%d: resb %d ; `%s` : Func\n", i, s->size, s->name);
          break;
        }
        case TypeAny: {
          o("v%d: resb %d ; `%s` : Any\n", i, s->size, s->name);
        }
        case TypeNone:
        case TypeSyscallFunc:
          break;
        default: {
          assert("type not implemented yet" && 0);
          break;
        }
      }
    }
  }
  return c->status;
#undef o
#undef vo
}

i32 compile_win_nasm_x86_64(Compile* c, FILE* fp) {
  assert("compile_win_nasm_x86_64 not implemented yet" && 0);
  (void)fp; // unused
  return c->status;
}

i32 alias_store(Parser* p, Alias* alias) {
  if (alias_lookup(p, alias) != NoError) {
    if (p->alias_count < MAX_ALIAS) {
      p->aliases[p->alias_count++] = *alias;
      return NoError;
    }
  }
  return Error;
}

i32 alias_lookup(Parser* p, Alias* alias) {
  for (u32 i = 0; i < p->alias_count; ++i) {
    Alias* a = &p->aliases[i];
    char copy_a[MAX_NAME_SIZE] = {0};
    char copy_b[MAX_NAME_SIZE] = {0};
    strncpy(copy_a, a->token.buffer, a->token.length);
    strncpy(copy_b, alias->token.buffer, alias->token.length);
    if (!strncmp(copy_a, copy_b, MAX_NAME_SIZE)) {
      *alias = *a;
      return NoError;
    }
  }
  return Error;
}

// TODO(lucas): free the contents of aliases
i32 preprocess(Parser* p) {
  i32 result = NoError;
  REAL_TIMER_START();
  u32 done = 0;
  lexer_next(&p->l);
  while (!done) {
    Token t = lexer_peek(&p->l);
    switch (t.type) {
      case T_EOF: {
        done = 1;
        p->total_lines += p->l.line;
        break;
      }
      case T_IDENTIFIER: {
        Alias alias;
        alias.token = t;
        char* source = NULL;
        u32 current_index = (u32)(p->l.index - &p->l.source[0]);
        u32 ident_index = (u32)(t.buffer - &p->l.source[0]);
        if (alias_lookup(p, &alias) == NoError) {
          source = p->source[p->source_count - 1];
          source = string_shrink_at(source, strlen(source), ident_index, t.length);
          current_index -= t.length;
          source = string_insert(source, alias.content.buffer, strlen(source), alias.content.length, ident_index);
          p->source[p->source_count - 1] = source;
          p->l.source = source;
          p->l.index = &p->l.source[current_index + alias.content.length];
          // printf("`%s`\n", source);
          break;
        }
        lexer_next(&p->l); // skip identifier
        break;
      }
      case T_ALIAS: {
        if (parse_alias(p, 0) == Error) {
          done = 1;
          result = Error;
        }
        break;
      }
      default: {
        lexer_next(&p->l);
        break;
      }
    }
  }

  lexer_init(&p->l, p->path[p->source_count - 1], p->source[p->source_count - 1]);
  REAL_TIMER_END(
    print_info("preprocessing of `%s` took %lf seconds\n", p->l.filename, _dt);
  );
  return result;
}

i32 parser_init(Parser* p, char* filename, char* source) {
  if (!source) {
    return Error;
  }
  lexer_init(&p->l, filename, source);
  p->ast = NULL;
  p->status = NoError;
  p->source[0] = source;
  p->path[0] = filename;
  p->source_count = 1;
  p->alias_count = 0;
  p->total_lines = 0;
  return NoError;
}

void parser_free(Parser* p) {
  ast_free(p->ast);
  char** path = (char**)&p->path[1];
  for (u32 i = 1; i < p->source_count; ++i, ++path) { // first path lives on throughout the program runtime, thus i = 1. remaining paths are allocated on heap.
    free(*path);
  }
  char** source = (char**)&p->source;
  for (u32 i = 0; i < p->source_count; ++i, ++source) {
    free(*source);
  }
  for (u32 i = 0; i < p->alias_count; ++i) {
    Alias* alias = &p->aliases[i];
    free(alias->token.buffer);
    free(alias->content.buffer);
  }
}

void parser_error(Parser* p, const char* fmt, ...) {
  if (p->status == Error) {
    return;
  }
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  Lexer* l = &p->l;

  FILE* fp = stderr;
  fprintf(fp, "[parse-error]: %s:%d:%d: %s", l->token.filename, l->token.line, l->token.column, buffer);
  printline(fp, l->source, l->index, l->token.length, 1, NUM_LINES_TO_PRINT);
  l->token.type = T_EOF;
  p->status = Error;
}

Ast* parse(Parser* p) {
  Ast* ast = NULL;
  if (preprocess(p) == NoError) {
    REAL_TIMER_START();
    lexer_next(&p->l);
    ast = parse_statements(p);
    REAL_TIMER_END(
      print_info("parsing of `%s` took %lf seconds\n", p->l.filename, _dt);
    );
  }
  return ast;
}

/*
 
 stmts : stmt
       | stmts stmt
       | `;`
       ;
*/
Ast* parse_statements(Parser* p) {
  Ast* stmts = ast_create(AstStatementList);
  for (;;) {
    Token t = lexer_peek(&p->l);
    switch (t.type) {
      case T_EOF: {
        goto done;
      }
      case T_SEMICOLON: {
        lexer_next(&p->l);
        break;
      }
      case T_RIGHT_CURLY: {
        return stmts;
      }
      case T_FN: {
        Ast* func_def = parse_func_def(p);
        if (!func_def) {
          goto done;
        }
        ast_push(stmts, func_def);
        break;
      }
      case T_INCLUDE: {
        t = lexer_next(&p->l); // skip `include`
        if (t.type != T_CSTRING) {
          parser_error(p, "expected string after include statement, but got `%.*s`\n", t.length, t.buffer);
          return NULL;
        }
        lexer_next(&p->l);

        u32 path_length = t.length + 1;
        char* path = malloc(path_length);
        snprintf(path, path_length, "%.*s", t.length, t.buffer);

        // do not include a file that has already been included, i.e. inclusion protection/guards
        u32 guard = 0;
        for (u32 i = 0; i < p->source_count; ++i) {
          char* included_path = p->path[i];
          if (!strncmp(path, included_path, MAX_PATH_SIZE)) {
            guard = 1;
            free(path);
            break;
          }
        }
        if (!guard) {
          char* source = read_entire_file(path);
          if (!source) {
            parser_error(p, "failed to include source file `%s`\n", path);
            free(path);
            return stmts;
          }
          // copy current lexer state
          Lexer l_copy;
          memcpy(&l_copy, &p->l, sizeof(Lexer));
          if (p->source_count < MAX_SOURCE) {
            p->path[p->source_count] = path;
            p->source[p->source_count] = source;
            p->source_count++;
            // initialize new lexer state
            lexer_init(&p->l, path, source);
            ast_push(stmts, parse(p));
            // restore lexer state
            memcpy(&p->l, &l_copy, sizeof(Lexer));
          }
          else {
            assert("max includes was reached, increase capacity!" && 0); // TODO: handle
          }
        }
        break;
      }
      case T_ALIAS: {
        if (parse_alias(p, 1) == Error) {
          return stmts;
        }
        break;
      }
      default: {
        Ast* stmt = parse_statement(p);
        if (!stmt) {
          goto done;
        }
        ast_push(stmts, stmt);
        break;
      }
    }
  }
done:
  return stmts;
}

/*
 stmt : expr `;`
      | const ident expr `;`
      | memory ident NUMBER `;`
      | `{` stmts `}`
      | `=` expr expr `;`
      | fn name `{` stmts `}`
      | while expr `{` stmts `}`
      | alias ident expr `;`
      ;
*/
Ast* parse_statement(Parser* p) {
  Token t = lexer_peek(&p->l);
  Token_type token_type = t.type;
  switch (token_type) {
    case T_CONST:
    case T_LET: {
      Ast_type branch_type[] = {AstConstStatement, AstLetStatement};
      t = lexer_next(&p->l); // skip `let`
      if (t.type != T_IDENTIFIER) {
        parser_error(p, "expected identifier in let statement, but got `%.*s`\n", t.length, t.buffer);
        return NULL;
      }
      lexer_next(&p->l);  // skip `identifier`
      Ast* expr = ast_create(
        branch_type[token_type == T_LET]
      );
      Ast* explicit_type = NULL;
      expr->token = t;
      t = lexer_peek(&p->l);
      if (t.type == T_COLON) { // explicit type
        t = lexer_next(&p->l); // skip `:`
        explicit_type = parse_type(p);
        if (!explicit_type) {
          parser_error(p, "expected type after `:`, but got `%.*s`\n", t.length, t.buffer);
          return NULL;
        }
        if (explicit_type->token.type == T_NONE) {
          parser_error(p, "explicit type cannot be `%.*s`\n", t.length, t.buffer);
          return NULL;
        }
      }
      t = lexer_peek(&p->l);
      if (t.type == T_LEFT_P) {
        lexer_next(&p->l); // skip `(`
        ast_push(expr, parse_expr_list(p));
        t = lexer_peek(&p->l);
        if (t.type != T_RIGHT_P) {
          parser_error(p, "expected closing parenthesis `)` expression list, but got `%.*s`\n", t.length, t.buffer);
          return expr;
        }
        lexer_next(&p->l); // skip `)`
      }
      else {
        ast_push(expr, parse_expr_list(p));
      }
      ast_push(expr, explicit_type);
      t = lexer_peek(&p->l);
      if (t.type != T_SEMICOLON) {
        parser_error(p, "expected `;` semicolon after statement, but got `%.*s`\n", t.length, t.buffer);
      }
      return expr;
    }
    case T_MEMORY: {
      t = lexer_next(&p->l); // skip `memory`
      if (t.type != T_IDENTIFIER) {
        parser_error(p, "expected identifier in memory statement, but got `%.*s`\n", t.length, t.buffer);
        return NULL;
      }
      Ast* expr = ast_create(AstMemoryStatement);
      expr->token = t;
      t = lexer_next(&p->l);  // skip `identifier`
      ast_push(expr, parse_expr(p));
      t = lexer_peek(&p->l);
      if (t.type != T_SEMICOLON) {
        parser_error(p, "expected `;` semicolon after memory statement, but got `%.*s`\n", t.length, t.buffer);
      }
      lexer_next(&p->l); // skip `;`
      return expr;
    }
    // TODO(lucas): fix parsing bug where there is a mismatch of curly brackets
    // `{` stmts `}`
    case T_LEFT_CURLY: {
      lexer_next(&p->l); // skip `{`
      Ast* block = ast_create(AstBlockStatement);
      Ast* sub_stmts = parse_statements(p);
      if (sub_stmts) {
        ast_push(block, sub_stmts);
        t = lexer_peek(&p->l);
        if (t.type != T_RIGHT_CURLY) {
          parser_error(p, "expected closing `}` curly bracket in block, but got `%.*s`\n", t.length, t.buffer);
        }
        lexer_next(&p->l);
      }
      return block;
    }
    case T_ASSIGN:
    case T_STORE64:
    case T_STORE32:
    case T_STORE16:
    case T_STORE8: {
      lexer_next(&p->l); // skip `=`
      Ast* assignment = ast_create(AstAssignment);
      assignment->token = t;
      ast_push(assignment, parse_expr(p));
      ast_push(assignment, parse_expr(p));
      return assignment;
    }
    case T_WHILE: {
      lexer_next(&p->l); // skip `while`
      Ast* while_stmt = ast_create(AstWhileStatement);
      while_stmt->token = t;
      Ast* cond = ast_create(AstExpression);
      cond->token = lexer_peek(&p->l);
      ast_push(cond, parse_expr(p));
      ast_push(while_stmt, cond);
      t = lexer_peek(&p->l);
      if (t.type == T_LEFT_CURLY) {
        lexer_next(&p->l); // skip `{`
        ast_push(while_stmt, parse_statements(p));
        t = lexer_peek(&p->l);
        if (t.type != T_RIGHT_CURLY) {
          parser_error(p, "expected closing `}` curly bracket in while statement, but got `%.*s`\n", t.length, t.buffer);
          return while_stmt;
        }
        lexer_next(&p->l); // skip `}`
      }
      else {
        ast_push(while_stmt, parse_statement(p));
      }
      return while_stmt;
    }
    case T_IF: {
      lexer_next(&p->l); // skip `if`
      Ast* if_stmt = ast_create(AstIfStatement);
      if_stmt->token = t;
      Ast* cond = ast_create(AstExpression);
      cond->token = lexer_peek(&p->l);
      ast_push(cond, parse_expr(p));
      ast_push(if_stmt, cond);
      t = lexer_peek(&p->l);
      if (t.type == T_LEFT_CURLY) {
        lexer_next(&p->l); // skip `{`
        ast_push(if_stmt, parse_statements(p));
        t = lexer_peek(&p->l);
        if (t.type != T_RIGHT_CURLY) {
          parser_error(p, "expected closing `}` curly bracket in if statement, but got `%.*s`\n", t.length, t.buffer);
          return if_stmt;
        }
        lexer_next(&p->l); // skip `}`
      }
      else {
        Ast* if_body = ast_create(AstStatementList);
        ast_push(if_body, parse_statement(p));
        ast_push(if_stmt, if_body);
      }
      if (lexer_peek(&p->l).type == T_ELSE) {
        t = lexer_next(&p->l);
        if (t.type == T_LEFT_CURLY) {
          lexer_next(&p->l); // skip `{`

          ast_push(if_stmt, parse_statements(p));
          t = lexer_peek(&p->l);
          if (t.type != T_RIGHT_CURLY) {
            parser_error(p, "expected closing `}` curly bracket in else body, but got `%.*s`\n", t.length, t.buffer);
            return if_stmt;
          }
          lexer_next(&p->l); // skip `}`
        }
        else {
          Ast* else_body = ast_create(AstStatementList);
          ast_push(else_body, parse_statement(p));
          ast_push(if_stmt, else_body);
        }
      }
      return if_stmt;
    }
    case T_ENUM: {
      Ast* enum_expr = ast_create(AstEnum);
      t = lexer_next(&p->l); // skip `enum`
      if (t.type == T_LEFT_P) {
        lexer_next(&p->l); // skip `(`
        ast_push(enum_expr, parse_ident_list(p));
        t = lexer_peek(&p->l);
        if (t.type != T_RIGHT_P) {
          parser_error(p, "expected closing `)` parenthesis, but got `%.*s`\n", t.length, t.buffer);
          return enum_expr;
        }
        lexer_next(&p->l); // skip `)`
      }
      else {
        ast_push(enum_expr, parse_ident_list(p));
      }
      t = lexer_peek(&p->l);
      if (t.type != T_SEMICOLON) {
        parser_error(p, "expected `;` after enum expression, but got `%.*s`\n", t.length, t.buffer);
      }
      lexer_next(&p->l); // skip `;`
      return enum_expr;
    }
    default: {
      Ast* expr = parse_expr(p);
      t = lexer_peek(&p->l);
      if (t.type != T_SEMICOLON) {
        parser_error(p, "expected `;` semicolon after statement, but got `%.*s`\n", t.length, t.buffer);
      }
      lexer_next(&p->l); // skip `;`
      return expr;
    }
  }
  assert("something went wrong" && 0);
  return NULL;
}

/*
  expr : expr
       | OP expr
       | OP expr expr
       | NUMBER
       | IDENTIFIER
       | PRINT expr
*/
Ast* parse_expr(Parser* p) {
  Token t = lexer_peek(&p->l);
  switch (t.type) {
    case T_POP:
    case T_NUMBER:
    case T_CSTRING: {
      lexer_next(&p->l);
      Ast* node = ast_create(AstValue);
      node->token = t;
      return node;
    }
    case T_IDENTIFIER: {
      lexer_next(&p->l); // skip identifier
      if (lexer_peek(&p->l).type == T_LEFT_P) { // function call
        lexer_next(&p->l); // skip `(`
        Ast* node = ast_create(AstFuncCall);
        node->token = t;
        ast_push(node, parse_expr_list(p));
        t = lexer_peek(&p->l);
        if (t.type != T_RIGHT_P) {
          parser_error(p, "expected closing `)` parenthesis, but got `%.*s`\n", t.length, t.buffer);
        }
        lexer_next(&p->l); // skip `)`
        return node;
      }
      Ast* node = ast_create(AstValue);
      node->token = t;
      return node;
    }
    case T_ADD:
    case T_SUB:
    case T_MUL:
    case T_DIV:
    case T_DIVMOD:
    case T_LSHIFT:
    case T_RSHIFT:
    case T_LT:
    case T_GT:
    case T_AND:
    case T_OR:
    case T_EQ:
    case T_NEQ: {
      lexer_next(&p->l);
      Ast* expr = ast_create(AstBinopExpression);
      expr->token = t;
      ast_push(expr, parse_expr(p));
      ast_push(expr, parse_expr(p));
      return expr;
    }
    case T_AT: {
      t = lexer_next(&p->l);
      if (t.type == T_IDENTIFIER) {
        Ast* expr = ast_create(AstValue);
        expr->token = t;
        expr->token.type = T_AT;
        lexer_next(&p->l);
        return expr;
      }
      parser_error(p, "expected identifer after `@` operator, but got `%.*s`\n", t.length, t.buffer);
      return NULL;
    }
    case T_DEREF:
    case T_LOAD64:
    case T_LOAD32:
    case T_LOAD16:
    case T_LOAD8:
    case T_PRINT:
    case T_LOGICAL_NOT: {
      lexer_next(&p->l); // skip op
      Ast* expr = ast_create(AstUopExpression);
      expr->token = t;
      Ast* sub_expr = parse_expr(p);
      if (sub_expr != NULL) {
        ast_push(expr, sub_expr);
        return expr;
      }
      parser_error(p, "invalid syntax in unary operator expression\n");
      return NULL;
    }
    case T_SIZEOF: {
      u8 ok = 0;
      t = lexer_next(&p->l); // skip `sizeof`
      switch (t.type) {
        case T_IDENTIFIER:
        case T_NUMBER:
        case T_CSTRING:
        case T_UNSIGNED64:
        case T_UNSIGNED32:
        case T_CSTR:
        case T_ANY: {
          ok = 1;
          break;
        }
        default:
          break;
      }
      if (!ok) {
        parser_error(p, "expected identifier, number, string, or type in sizeof operator, but got `%.*s`\n", t.length, t.buffer);
        return NULL;
      }
      Ast* node = ast_create(AstSizeof);
      node->token = t;
      lexer_next(&p->l);
      return node;
    }
    case T_LEFT_P: {
      lexer_next(&p->l);
      Ast* expr = parse_expr(p);
      t = lexer_peek(&p->l);
      if (t.type != T_RIGHT_P) {
        parser_error(p, "missing closing `)` parenthesis\n");
        return NULL;
      }
      lexer_next(&p->l); // skip `)`
      return expr;
    }
    case T_RIGHT_P: {
      parser_error(p, "unexpected closing `)` parenthesis in expression\n");
      return NULL;
    }
    case T_CAST: {
      lexer_next(&p->l); // skip `cast`
      Ast* expr = ast_create(AstCastExpression);
      Ast* inner_expr = ast_create(AstExpression);
      expr->token = t;
      ast_push(expr, ast_push(inner_expr, parse_expr(p)));
      ast_push(expr, parse_type(p));
      return expr;
    }
    default: {
      parser_error(p, "unexpected `%.*s` in expression\n", t.length, t.buffer);
      p->status = Error;
      return NULL;
    }
  }
  return NULL;
}

// func :
//      | fn name `{` stmts `}`
//      | fn name stmt `;`
//      | fn name `(` params `)` stmt `;`
//      | fn name `(` params `)` `{` stmts `}` `;`
//      | fn name params `->` rtype body
Ast* parse_func_def(Parser* p) {
  Token t = lexer_next(&p->l); // skip `fn`
  if (t.type == T_IDENTIFIER) {
    Ast* func_def = ast_create(AstFuncDefinition);
    Ast* type = NULL;
    func_def->token = t;
    t = lexer_next(&p->l); // skip `name`
    if (t.type == T_LEFT_P) {
      lexer_next(&p->l); // skip `(`
      ast_push(func_def, parse_param_list(p));
      if (p->status == Error) {
        return func_def;
      }
      t = lexer_peek(&p->l);
      if (t.type != T_RIGHT_P) {
        parser_error(p, "expected `)` right parenthesis in function parameter list, but got `%.*s`\n", t.length, t.buffer);
        return func_def;
      }
      lexer_next(&p->l); // skip `)`
    }
    else {
      ast_push(func_def, ast_create(AstParamList)); // push an empty parameter list
    }
    t = lexer_peek(&p->l);
    if (t.type == T_ARROW) {
      lexer_next(&p->l);
      type = parse_type(p);
      if (!type) {
        t = lexer_peek(&p->l);
        parser_error(p, "expected type after `->`, but got `%.*s`\n", t.length, t.buffer);
        return func_def;
      }
    }
    t = lexer_peek(&p->l);
    if (t.type == T_LEFT_CURLY) {
      lexer_next(&p->l); // skip `{`
      Ast* stmts = parse_statements(p);
      if (stmts) {
        ast_push(func_def, stmts);
        t = lexer_peek(&p->l);
        if (t.type != T_RIGHT_CURLY) {
          parser_error(p, "expected `}` right curly in function definition, but got `%.*s`\n", t.length, t.buffer);
        }
        lexer_next(&p->l); // skip `}`
      }
    }
    else {
      Ast* stmts = ast_create(AstStatementList);
      ast_push(func_def,
        ast_push(stmts,
          parse_expr(p)
        )
      );
      t = lexer_peek(&p->l);
      if (t.type != T_SEMICOLON) {
        parser_error(p, "expected `;` after function definition expression, but got `%.*s`\n", t.length, t.buffer);
      }
      lexer_next(&p->l); // skip `;`
    }
    ast_push(func_def, type);
    return func_def;
  }
  parser_error(p, "expected identifier in function definition, but got `%.*s`\n", t.length, t.buffer);
  return NULL;
}

// L : `(` expr `,` ... `)`
//   | `(` empty `)`
Ast* parse_expr_list(Parser* p) {
  Ast* expr_list = ast_create(AstExprList);
  if (lexer_peek(&p->l).type == T_RIGHT_P) {
    return expr_list;
  }
  for (;;) {
    ast_push(expr_list, parse_expr(p));
    if (lexer_peek(&p->l).type == T_COMMA) {
      lexer_next(&p->l);
      continue;
    }
    break;
  }
  return expr_list;
}

// L : `(` identifier `,` ... `)`
Ast* parse_ident_list(Parser* p) {
  Ast* ident_list = ast_create(AstExprList);
  for (;;) {
    Token t = lexer_peek(&p->l);
    if (t.type == T_IDENTIFIER) {
      ast_push_node(ident_list, AstValue, t);
      lexer_next(&p->l); // skip identifier
    }
    if (lexer_peek(&p->l).type == T_COMMA) {
      lexer_next(&p->l);
      continue;
    }
    break;
  }
  return ident_list;
}
Ast* parse_param_list(Parser* p) {
  Ast* param_list = ast_create(AstParamList);
  if (lexer_peek(&p->l).type == T_RIGHT_P) {
    return param_list;
  }
  for (;;) {
    Token t = lexer_peek(&p->l);
    if (t.type == T_IDENTIFIER) {
      Token ident = t;
      t = lexer_next(&p->l); // skip argument identifier
      if (t.type != T_COLON) {
        parser_error(p, "expected `:` colon after argument identifier\n");
        return param_list;
      }
      t = lexer_next(&p->l); // skip `:`
      if (token_to_compile_type(t) != TypeNone) {
        Ast* arg = ast_create(AstType);
        arg->token = t;
        ast_push_node(arg, AstValue, ident);
        ast_push(param_list, arg);
        
        t = lexer_next(&p->l); // skip type
        if (t.type == T_COMMA) {
          lexer_next(&p->l);
          continue;
        }
        break;
      }
      else {
        parser_error(p, "expected type, but got `%.*s` in parameter list\n", t.length, t.buffer);
        return param_list;
      }
    }
    break;
  }
  return param_list;
}

Ast* parse_type(Parser* p) {
  Token t = lexer_peek(&p->l);
  if (t.type == T_NONE || t.type == T_ANY || t.type == T_UNSIGNED64 || t.type == T_UNSIGNED32 || t.type == T_CSTR) {
    Ast* type = ast_create(AstType);
    type->token = t;
    t = lexer_next(&p->l); // skip type
    if (t.type == T_COLON) {
      t = lexer_next(&p->l); // skip `:`
      ast_push(type, parse_expr(p));
    }
    return type;
  }
  return NULL;
}

i32 parse_alias(Parser* p, u32 skip_store_of_alias) {
  Token t;
  Token ident = lexer_next(&p->l); // skip `alias`
  if (ident.type != T_IDENTIFIER) {
    parser_error(p, "expected identifier after alias keyword, but got `%.*s`\n", ident.length, ident.buffer);
    return Error;
  }
  t = lexer_next(&p->l); // skip identifier
  if (t.type != T_LEFT_CURLY) {
    parser_error(p, "expected `{` to begin alias body, but got `%.*s`\n", t.length, t.buffer);
    return Error;
  }
  lexer_next(&p->l); // skip `{`
  Token first_token = lexer_peek(&p->l);
  Token last_token;
  for (;;) {
    last_token = lexer_peek(&p->l);
    if (last_token.type == T_EOF || last_token.type == T_RIGHT_CURLY) {
      break;
    }
    lexer_next(&p->l);
  }
  t = lexer_peek(&p->l);
  if (t.type != T_RIGHT_CURLY) {
    parser_error(p, "expected `}` to end alias body, but got `%.*s`\n", t.length, t.buffer);
    return Error;
  }
  lexer_next(&p->l); // skip `}`
  if (!skip_store_of_alias) {
    Alias alias = (Alias) {
      .token = ident,
      .content = first_token,
    };
    char* tmp = NULL;

    tmp = malloc(alias.token.length);
    strncpy(tmp, alias.token.buffer, alias.token.length);
    alias.token.buffer = tmp;
    alias.content.length = (u32)(last_token.buffer - first_token.buffer);

    tmp = malloc(alias.content.length);
    strncpy(tmp, alias.content.buffer, alias.content.length);
    alias.content.buffer = tmp;

    if (alias_store(p, &alias) == Error) {
      // TODO(lucas): implement and use parser_error_at here, to point to the actual alias location
      parser_error(p, "alias `%.*s` already exists\n", alias.token.length, alias.token.buffer);
      return Error;
    }
  }
  return NoError;
}

void lexer_init(Lexer* l, char* filename, char* source) {
  l->token = (Token) {
    .buffer = source,
    .length = 1,
    .type = T_EOF,
    .v.num = 0,
    .filename = filename,
    .source = source,
    .line = 1,
    .column = 1,
  };
  l->filename = filename;
  l->source = source;
  l->index = source;
  l->line = 1;
  l->column = 1;
  l->status = NoError;
}

void lexer_error(Lexer* l, const char* fmt, ...) {
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stderr;
  fprintf(fp, "[lex-error]: %d:%d: %s", l->token.line, l->token.column, buffer);
  printline(fp, l->source, l->index, l->token.length, 1, NUM_LINES_TO_PRINT);
  l->token.type = T_EOF;
  l->status = Error;
}

void next(Lexer* l) {
  l->token.buffer = l->index;
  l->token.length = 1;
  l->token.filename = l->filename;
  l->token.source = l->source;
  l->token.line = l->line;
  l->token.column = l->column;
}

i32 compare(Token t, const char* b) {
  const char* index = b;
  for (int i = 0; i < t.length; ++i, ++index) {
    if (t.buffer[i] != *index) {
      return 0;
    }
  }
  return (*index == 0);
}

i32 expect(Token t, Token_type type) {
  return t.type == type;
}

Token lexer_read_symbol(Lexer* l) {
  while (
    is_alpha(*l->index) ||
    is_digit(*l->index) ||
#if USE_EXTENDED_ASCII
    is_extended_ascii((u8)*l->index) ||
#endif
    *l->index == '_' ||
    *l->index == '-' ||
    *l->index == '.') {
    l->index++;
    l->column++;
  }
  l->token.length = l->index - l->token.buffer;
  if (compare(l->token, "pop")) {
    l->token.type = T_POP;
  }
  else if (compare(l->token, "print")) {
    l->token.type = T_PRINT;
  }
  else if (compare(l->token, "include")) {
    l->token.type = T_INCLUDE;
  }
  else if (compare(l->token, "const")) {
    l->token.type = T_CONST;
  }
  else if (compare(l->token, "let")) {
    l->token.type = T_LET;
  }
  else if (compare(l->token, "memory")) {
    l->token.type = T_MEMORY;
  }
  else if (compare(l->token, "fn")) {
    l->token.type = T_FN;
  }
  else if (compare(l->token, "while")) {
    l->token.type = T_WHILE;
  }
  else if (compare(l->token, "if")) {
    l->token.type = T_IF;
  }
  else if (compare(l->token, "else")) {
    l->token.type = T_ELSE;
  }
  else if (compare(l->token, "store64")) {
    l->token.type = T_STORE64;
  }
  else if (compare(l->token, "store32")) {
    l->token.type = T_STORE32;
  }
  else if (compare(l->token, "store16")) {
    l->token.type = T_STORE16;
  }
  else if (compare(l->token, "store8")) {
    l->token.type = T_STORE8;
  }
  else if (compare(l->token, "load64")) {
    l->token.type = T_LOAD64;
  }
  else if (compare(l->token, "load32")) {
    l->token.type = T_LOAD32;
  }
  else if (compare(l->token, "load16")) {
    l->token.type = T_LOAD16;
  }
  else if (compare(l->token, "load8")) {
    l->token.type = T_LOAD8;
  }
  else if (compare(l->token, "and")) {
    l->token.type = T_AND;
  }
  else if (compare(l->token, "not")) {
    l->token.type = T_LOGICAL_NOT;
  }
  else if (compare(l->token, "or")) {
    l->token.type = T_OR;
  }
  else if (compare(l->token, "eq")) {
    l->token.type = T_EQ;
  }
  else if (compare(l->token, "neq")) {
    l->token.type = T_NEQ;
  }
  else if (compare(l->token, "lshift")) {
    l->token.type = T_LSHIFT;
  }
  else if (compare(l->token, "rshift")) {
    l->token.type = T_RSHIFT;
  }
  else if (compare(l->token, "sizeof")) {
    l->token.type = T_SIZEOF;
  }
  else if (compare(l->token, "enum")) {
    l->token.type = T_ENUM;
  }
  else if (compare(l->token, "alias")) {
    l->token.type = T_ALIAS;
  }
  else if (compare(l->token, "cast")) {
    l->token.type = T_CAST;
  }
  else if (compare(l->token, "none")) {
    l->token.type = T_NONE;
  }
  else if (compare(l->token, "any")) {
    l->token.type = T_ANY;
  }
  else if (compare(l->token, "u64")) {
    l->token.type = T_UNSIGNED64;
  }
  else if (compare(l->token, "u32")) {
    l->token.type = T_UNSIGNED32;
  }
  else if (compare(l->token, "cstr")) {
    l->token.type = T_CSTR;
  }
  else {
    l->token.type = T_IDENTIFIER;
  }
  return l->token;
}

Token lexer_read_number(Lexer* l) {
  while (is_hex(*l->index) || *l->index == 'x') {
    l->index++;
    l->column++;
  }
  l->token.length = l->index - l->token.buffer;
  l->token.type = T_NUMBER;
  return l->token;
}

i32 is_digit(char ch) {
  return (ch >= '0') && (ch <= '9');
}

i32 is_hex(char ch) {
  return is_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

i32 is_alpha(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

char to_lower(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch + 32;
  }
  return ch;
}

i32 is_extended_ascii(u8 ch) {
  return (ch >= 128 && ch < 255);
}

Token lexer_next(Lexer* l) {
  for (;;) {
    next(l);
    char ch = *l->token.buffer;
    l->token.filename = l->filename;
    l->token.source = l->source;
    l->token.column = l->column;
    l->index++;
    l->column++;
    switch (ch) {
      case '\r': {
        l->column = 1;
        if (*l->index == '\n') {
          break;
        }
        l->line++;
        break;
      }
      case '\n': {
        l->line++;
        l->column = 1;
        break;
      }
      case '/': {
        if (*l->index == '/') {
          while (*l->index != '\n' && *l->index != '\0') {
            l->index++;
            l->column++;
          }
          break;
        }
        l->token.type = T_DIV;
        goto done;
      }
      case '"': {
        char delimiter = ch;
        for (;;) {
          if (*l->index == '\0') {
            lexer_error(l, "unfinished string\n");
            l->token.type = T_EOF;
            return l->token;
          }
          else if (*l->index == '\\') {
            if (*(l->index + 1) == '0') {
              *l->index++ = '\0';
            }
            else if (*(l->index + 1) == 'n') {
              *l->index++ = '\n';
            }
          }
          else if (*l->index == delimiter) {
            break;
          }
          l->index++;
          l->column++;
        }
        l->token.buffer++;
        l->column++;
        l->token.type = T_CSTRING;
        l->token.length = l->index - l->token.buffer;
        l->index++;
        goto done;
      }
      case '=': {
        l->token.type = T_ASSIGN;
        goto done;
      }
      case ',': {
        l->token.type = T_COMMA;
        goto done;
      }
      case '@': {
        l->token.type = T_AT;
        goto done;
      }
      case '#': {
        l->token.type = T_DEREF;
        goto done;
      }
      case '+': {
        l->token.type = T_ADD;
        goto done;
      }
      case '-': {
        if (*l->index == '>') {
          l->index++;
          l->column++;
          l->token.length++;
          l->token.type = T_ARROW;
          goto done;
        }
        l->token.type = T_SUB;
        goto done;
      }
      case '*': {
        l->token.type = T_MUL;
        goto done;
      }
      case '%': {
        l->token.type = T_DIVMOD;
        goto done;
      }
      case '<': {
        l->token.type = T_LT;
        goto done;
      }
      case '>': {
        l->token.type = T_GT;
        goto done;
      }
      case ':': {
        l->token.type = T_COLON;
        goto done;
      }
      case ';': {
        l->token.type = T_SEMICOLON;
        goto done;
      }
      case '(': {
        l->token.type = T_LEFT_P;
        goto done;
      }
      case ')': {
        l->token.type = T_RIGHT_P;
        goto done;
      }
      case '[': {
        l->token.type = T_LEFT_BRACKET;
        goto done;
      }
      case ']': {
        l->token.type = T_RIGHT_BRACKET;
        goto done;
      }
      case '{': {
        l->token.type = T_LEFT_CURLY;
        goto done;
      }
      case '}': {
        l->token.type = T_RIGHT_CURLY;
        goto done;
      }
      case ' ': case '\t': case '\f': case '\v': {
        break;
      }
      case '\0': {
        l->token.type = T_EOF;
        goto done;
      }
      case '\'': {
        char ch = *l->index;
        l->index++;
        l->column++;
        if (*l->index != '\'') {
          l->token.type = T_EOF;
          lexer_error(l, "missing closing `'`\n");
          goto done;
        }
        l->index++;
        l->column++;
        l->token.v.num = ch;
        l->token.type = T_NUMBER;
        l->token.length = 1;
        goto done;
      }
      default: {
        if (
          is_alpha(ch) ||
#if USE_EXTENDED_ASCII
          is_extended_ascii((u8)ch) ||
#endif
          ch == '_'
        ) {
          lexer_read_symbol(l);
          goto done;
        }
        else if (is_digit(ch)) {
          lexer_read_number(l);
          i32 result = str_to_int(l->token.buffer, l->token.length, &l->token.v.num);
          if (result == Error) {
            lexer_error(l, "invalid number `%.*s`\n", l->token.length, l->token.buffer);
            l->token.type = T_EOF;
          }
          goto done;
        }
        lexer_error(l, "unrecognized token `%.*s`\n", l->token.length, l->token.buffer);
        l->token.type = T_EOF;
        goto done;
      }
    }
  }
  l->token.column = l->column;
done:
  l->token.line = l->line;
  return l->token;
}

Token lexer_peek(Lexer* l) {
  return l->token;
}

Compile_type token_to_compile_type(Token t) {
  switch (t.type) {
    case T_ANY:         return TypeAny;
    case T_UNSIGNED64:  return TypeUnsigned64;
    case T_UNSIGNED32:  return TypeUnsigned32;
    case T_CSTRING:
    case T_CSTR:        return TypeCString;
    default: break;
  }
  return TypeNone;
}

// NOTE(lucas): index points to the end of the token, maybe change this.
void printline(FILE* fp, char* source, char* index, i32 token_length, i32 print_arrow, u32 num_lines_to_print) {
  if (!source) {
    return;
  }
  assert(num_lines_to_print != 0);
  index -= token_length;
  i32 offset = index - source + 1; // NOTE(lucas): + 1 because the first character of the file was not printed
  i32 start_offset = 0;
  i32 cur_line_offset = 0;
  i32 end_offset = 0;
  const u32 num_lines = num_lines_to_print;

  // scan to the beginning of the line
  char* at = index;
  while (start_offset < offset) {
    start_offset++;
    if (num_lines_to_print == num_lines) {
      cur_line_offset++;
    }
    char ch = *(at - start_offset);
    if (ch == '\n' || ch == '\r') {
      num_lines_to_print--;
      if (num_lines_to_print == 0) {
        break;
      }
    }
  }
  // and then we scan to the end of the line
  // TODO(lucas): handle carriage returns
  do {
    char ch = *(at + end_offset);
    if (ch == '\n' || ch == '\r' || ch == '\0') {
      break;
    }
    end_offset++;
  } while (1);
  i32 line_length = end_offset + start_offset - 1;
  fprintf(fp, "%.*s\n", line_length, at - start_offset + 1);
  if (print_arrow) {
    for (i32 i = 0; i < cur_line_offset - 1; ++i) {
      fprintf(fp, "-");
    }
    fprintf(fp, "^\n");
  }
  fprintf(fp, "\n");
}

void print_info(const char* fmt, ...) {
#if VERBOSE
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stdout;
  fprintf(fp, "[info]: %s", buffer);
#endif
}

// TODO(lucas): clean up
i32 str_to_int(char* str, u32 length, u64* out) {
  *out = 0;
  i32 base = 10;
  if (length > 2) {
    if (str[0] == '0' && str[1] == 'x') {
      base = 16;
      for (u32 i = 2; i < length; ++i) {
        char ch = to_lower(str[i]);
        if (is_digit(ch)) {
          *out = *out * base + (ch - '0');
          continue;
        }
        else if (is_hex(ch)) {
          *out = *out * base + (10 + ch - 'a');
          continue;
        }
        *out = 0;
        return Error;
      }
      return NoError;
    }
  }
  for (u32 i = 0; i < length; ++i) {
    char ch = str[i];
    if (is_digit(ch)) {
      *out = *out * base + (str[i] - '0');
      continue;
    }
    *out = 0;
    return Error;
  }
  return NoError;
}

Ast* ast_create(Ast_type type) {
  Ast* ast = ast_alloc_node();
  ast_init_node(ast);
  ast->type = type;
  return ast;
}

void ast_init_node(Ast* node) {
  *node = (Ast) {
    .node = NULL,
    .count = 0,
    .type = AstNone,
    .token = {0},
    .konst = 0,
    .pointer = 0,
  };
}

Ast* ast_alloc_node() {
  Ast* node = malloc(sizeof(Ast));
  if (!node) {
    fprintf(stderr, "Failed to allocate ast node\n");
    return NULL;
  }
  ast_init_node(node);
  return node;
}

Ast* ast_push_node(Ast* ast, Ast_type type, Token token) {
  if (!ast) {
    return NULL;
  }
  Ast* node = ast_alloc_node();
  if (!node) {
    return NULL;
  }
  node->type = type;
  node->token = token;
  return ast_push(ast, node);
}

Ast* ast_push(Ast* ast, Ast* node) {
  if (!ast) {
    assert(0);
    return NULL;
  }
  if (!node) {
    return ast;
  }
  if (ast->count == 0) {
    ast->node = malloc(sizeof(Ast*));
  }
  else {
    Ast** tmp = realloc(ast->node, sizeof(Ast*) * (ast->count + 1));
    if (!tmp) {
      fprintf(stderr, "failed to resize list of ast node references\n");
      return NULL;
    }
    ast->node = tmp;
  }
  ast->node[ast->count++] = node;
  return ast;
}

u32 ast_count(Ast* ast) {
  u32 count = 1;
  if (ast->count > 0) {
    for (u32 i = 0; i < ast->count; i++) {
      count += ast_count(ast->node[i]);
    }
  }
  return count;
}

void ast_print(const Ast* ast, i32 level, FILE* fp) {
  if (!ast) {
    return;
  }
  for (i32 i = 0; i < level; ++i, fprintf(fp, "    "));
  assert("something went very wrong" && ast->type < MAX_AST_TYPE && ast->token.type < MAX_TOKEN_TYPE);
  fprintf(fp, "<%s, %s, %d>: `%.*s`\n", ast_type_str[ast->type], token_type_str[ast->token.type], ast->konst, ast->token.length, ast->token.buffer);
  for (u32 i = 0; i < ast->count; ++i) {
    ast_print(ast->node[i], level + 1, fp);
  }
}

void ast_free(Ast* ast) {
  if (!ast) {
    return;
  }
  for (u32 i = 0; i < ast->count; ++i) {
    ast_free(ast->node[i]);
  }
  if (ast->node) {
    if (!ast->pointer) {
      free(ast->node);
      ast->count = 0;
    }
  }
  if (ast) {
    if (!ast->pointer) {
      free(ast);
      ast = NULL;
    }
  }
}

#define MAX_COMMAND_SIZE 512

i32 exec_command(const char* fmt, ...) {
  char command[MAX_COMMAND_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(command, MAX_COMMAND_SIZE, fmt, args);
  va_end(args);

  fprintf(stdout, "[cmd]: %s\n", command);
  FILE* fp = popen(command, "w");
  if (!fp) {
    return Error;
  }
  fclose(fp);
  return NoError;
}

u32 first_dot(const char* s) {
  u32 index = 0;
  while (*s != '.' && *s != '\0') {
    ++index;
    s++;
  }
  return index;
}
