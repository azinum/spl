//
// cspl.c - simple programming language (spl)
//
// original implementation in c
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

#define VERBOSE 1

#if VERBOSE
  #define v_printf(...) fprintf(stdout, __VA_ARGS__)
#else
  #define v_printf(...)
#endif

#define Error (-1)
#define NoError (0)

typedef enum Token_type {
  T_NONE = 0,
  T_EOF,

  T_IDENTIFIER,
  T_NUMBER,
  T_ASSIGN,
  T_AT,
  T_DEREF,
  T_ADD,
  T_SUB,
  T_LT,
  T_SEMICOLON,
  T_POP,
  T_CONST,
  T_LET,
  T_MEMORY,
  T_PRINT,
  T_FN,
  T_LEFT_P,
  T_RIGHT_P,
  T_LEFT_CURLY,
  T_RIGHT_CURLY,

  MAX_TOKEN_TYPE,
} Token_type;

typedef struct Token {
  char* buffer;
  i32 length;
  Token_type type;
  i32 line;
  i32 column;
} Token;

static const char* token_type_str[] = {
  "T_NONE",
  "T_EOF",

  "T_IDENTIFIER",
  "T_NUMBER",
  "T_ASSIGN",
  "T_AT",
  "T_DEREF",
  "T_ADD",
  "T_SUB",
  "T_LT",
  "T_SEMICOLON",
  "T_POP",
  "T_CONST",
  "T_LET",
  "T_MEMORY",
  "T_PRINT",
  "T_FN",
  "T_LEFT_P",
  "T_RIGHT_P",
  "T_LEFT_CURLY",
  "T_RIGHT_CURLY",
};

typedef struct Lexer {
  Token token;
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
  AstStatement,
  AstStatementList,
  AstBinopExpression,
  AstUopExpression,
  AstConstAssignment,
  AstLetStatement,
  AstFuncDefinition,
  AstMemoryStatement,
  AstAssignment,

  MAX_AST_TYPE,
} Ast_type;

static const char* ast_type_str[] = {
  "AstNone",
  "AstRoot",
  "AstValue",
  "AstExpression",
  "AstStatement",
  "AstStatementList",
  "AstBinopExpression",
  "AstUopExpression",
  "AstConstAssignment",
  "AstLetStatement",
  "AstFuncDefinition",
  "AstMemoryStatement",
  "AstAssignment",
};

typedef struct Ast {
  struct Ast** node;
  u32 count;
  Ast_type type;
  Token value;
} Ast;

// frontend: input -> lexer -> parser -> ast/other data structure -> intermidiate representation ->
// backend: -> code optimization -> code generation (or interpret the intermidiate representation)
typedef struct Parser {
  Lexer l;
  Ast* ast;
  Ast* current;
  i32 status;
} Parser;

typedef enum Ir_code {
  I_NOP = 0,
  I_POP,
  I_COPY,
  I_STORE64,
  I_LOAD64,
  I_PUSH_INT64,
  I_PUSH_ADDR_OF,
  I_ADD,
  I_SUB,
  I_LT,
  I_RET,
  I_PRINT,
  I_LABEL,
  I_CALL,

  MAX_IR_CODE,
} Ir_code;

typedef enum Compile_type {
  TypeNone = 0,
  TypeUnsigned64,
  TypeFunc,

  MaxType,
} Compile_type;

static const char* ir_code_str[] = {
  "I_NOP",
  "I_POP",
  "I_COPY",
  "I_STORE64",
  "I_LOAD64",
  "I_PUSH_INT64",
  "I_PUSH_ADDR_OF",
  "I_ADD",
  "I_SUB",
  "I_LT",
  "I_RET",
  "I_PRINT",
  "I_LABEL",
  "I_CALL",
};

typedef struct Function {
  i32 address;
  i32 argc;
} Function;

// intermidiate representation of the instructions which are to be generated or interpreted
typedef struct Op {
  Ir_code i;
  i32 dest;
  i32 src0;
  i32 src1;
} Op;

#define OP(_i) ((Op) {.i = _i, .dest = -1, .src0 = -1, .src1 = -1, })

#define MAX_INS (1024) // temp
#define MAX_DATA (KB(32)) // temp

#define MAX_NAME_SIZE 64
#define MAX_SYMBOL 256
#define MAX_FUNC 256

typedef enum Compile_target {
  TARGET_LINUX_NASM_X86_64,
  MAX_COMPILE_TARGET,
} Compile_target;

static const char* compile_target_str[MAX_COMPILE_TARGET] = {
  "compile_linux_nasm_x86_64"
};

typedef union Value {
  Function func;
  u64 num;
} Value;

typedef struct Symbol {
  char name[MAX_NAME_SIZE];
  i32 address;
  i32 size;
  Compile_type type;
  Token token;
  Value value;
} Symbol;

// compile state
typedef struct Compile {
  Op* ins;
  u32 ins_count;

  u8 data[MAX_DATA];
  u32 data_index;

  Symbol symbol_table[MAX_SYMBOL];
  u32 symbol_count;

  i32 status;
  i32 entry_point;
} Compile;

static void symbol_init(Symbol* symbol);

static i32 ir_init(Compile* c);
static void ir_free(Compile* c);
static void ir_print(Compile* c, FILE* fp);
static void ir_compile_error(Compile* c, const char* fmt, ...);
static i32 ir_start_compile(Compile* c, Ast* ast);
static i32 ir_declare_value(Compile* c, Token token, Symbol** symbol, i32* symbol_index);
static i32 ir_lookup_value(Compile* c, Token token, Symbol** symbol, i32* symbol_index);
static i32 ir_push_ins(Compile* c, Op ins, u32* ins_count);
static i32 ir_push_value(Compile* c, void* value, u32 size);
static i32 ir_compile(Compile* c, Ast* ast, u32* ins_count);
static i32 ir_compile_stmts(Compile* c, Ast* ast, u32* ins_count);
static i32 ir_compile_binop(Compile* c, Ast* ast, u32* ins_count);
static i32 ir_compile_uop(Compile* c, Ast* ast, u32* ins_count);
static i32 ir_compile_func(Compile* c, Ast* ast, u32* ins_count);

static i32 program_typecheck(Ast* ast);

static i32 compile(Compile* c, Compile_target target, FILE* fp);
static i32 compile_linux_nasm_x86_64(Compile* c, FILE* fp);

typedef i32 (*compile_target)(Compile*, FILE*);

static compile_target compile_targets[MAX_COMPILE_TARGET] = {
  compile_linux_nasm_x86_64,
};

static i32 parser_init(Parser* p, char* source);
static void parser_free(Parser* p);
static void parser_error(Parser* p, const char* fmt, ...);
static i32 parse(Parser* p);
static Ast* parse_statements(Parser* p);
static Ast* parse_statement(Parser* p);
static Ast* parse_expr(Parser* p);
static Ast* parse_func_def(Parser* p);
static void lexer_init(Lexer* l, char* source);
static void lexer_error(Lexer* l, const char* fmt, ...);
static void next(Lexer* l);
inline i32 compare(Token t, const char* b);
inline i32 expect(Token t, Token_type type);
inline Token lexer_read_symbol(Lexer* l);
inline Token lexer_read_number(Lexer* l);
inline i32 is_digit(char ch);
inline i32 is_alpha(char ch);
inline i32 is_extended_ascii(u8 ch);
static Token lexer_next(Lexer* l);
static Token lexer_peek(Lexer* l);

static void error_printline(FILE* fp, char* source, char* index, i32 token_length);
static void print_info(const char* fmt, ...);
static i32 str_to_int(char* str, i32 length, u64* out);

static Ast* ast_create(Ast_type type);
static void ast_init_node(Ast* node);
static Ast* ast_alloc_node();
static Ast* ast_push_node(Ast* ast, Ast_type type, Token value);
static Ast* ast_push(Ast* ast, Ast* node);
static void ast_print(const Ast* ast, i32 level, FILE* fp);
static void ast_free(Ast* ast);

i32 main(i32 argc, char** argv) {
  assert(ARR_SIZE(token_type_str) == MAX_TOKEN_TYPE);
  assert(ARR_SIZE(ast_type_str) == MAX_AST_TYPE);
  assert(ARR_SIZE(ir_code_str) == MAX_IR_CODE);
  assert(ARR_SIZE(compile_target_str) == MAX_COMPILE_TARGET);
  assert(ARR_SIZE(compile_targets) == MAX_COMPILE_TARGET);
  REAL_TIMER_START();
  (void)ast_print; (void)ir_code_str;
  i32 exit_status = EXIT_SUCCESS;
  assert(ARR_SIZE(token_type_str) == MAX_TOKEN_TYPE);
  assert(ARR_SIZE(ir_code_str) == MAX_IR_CODE);
  assert(ARR_SIZE(ast_type_str) == MAX_AST_TYPE);

  char* filename = NULL;
  char* source = NULL;
  FILE* fp = NULL;
  if (argc < 2) {
    printf("Usage; %s [-t | <filename>]\n", argv[0]);
    return EXIT_SUCCESS;
  }
  else {
    filename = argv[1];
    if (!strcmp(filename, "-t")) {
      return EXIT_SUCCESS;
    }
    else {
      fp = fopen(filename, "rb");
      if (!fp) {
        fprintf(stderr, "Failed to open file `%s`\n", filename);
        return EXIT_FAILURE;
      }
      fseek(fp, 0, SEEK_END);
      i32 size = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      source = (char*)malloc(size + 1);
      if (!source) {
        fprintf(stderr, "Failed to allocate memory for source file `%s`\n", filename);
        fclose(fp);
        exit_status = EXIT_FAILURE;
      }
      i32 read_size = fread(source, 1, size, fp);
      assert("something went wrong when reading file" && read_size == size);
      (void)read_size;  // ignore warning
      source[read_size] = '\0';
    }
  }
  assert(filename != NULL);

  Parser p;
  if (parser_init(&p, (char*)source) == NoError) {
    parse(&p);
    if (p.status == NoError && p.l.status == NoError) {
      if (program_typecheck(p.ast) == NoError) {
        Compile c;
        if (ir_init(&c) == NoError) {
          if (ir_start_compile(&c, p.ast) == NoError) {
            char path[MAX_PATH_SIZE] = {0};
            snprintf(path, MAX_PATH_SIZE, "%s.asm", filename);
            FILE* fp = fopen(path, "w");
            if (fp) {
              compile(&c, TARGET_LINUX_NASM_X86_64, fp);
              fclose(fp);
            }
#if 0
            ast_print(p.ast, 0, stdout);
            ir_print(&c, stdout);
#endif
          }
          else {
            exit_status = EXIT_FAILURE;
          }
          ir_free(&c);
        }
        else {
          exit_status = EXIT_FAILURE;
        }
      }
    }
    else {
      exit_status = EXIT_FAILURE;
    }
    parser_free(&p);
  }
  if (fp) {
    fclose(fp);
  }
  free(source);
  REAL_TIMER_END(
    print_info("%s: %g s\n", __FUNCTION__, _dt);
    (void)_dt;
  );
  return exit_status;
}

void symbol_init(Symbol* s) {
  memset(s->name, 0, ARR_SIZE(s->name));
  s->address = -1;
  s->size = 0;
  s->type = TypeNone;
  s->token = (Token) {0};
  s->value = (Value) { .num = 0, };
}

i32 ir_init(Compile* c) {
  c->ins = NULL;
  c->ins_count = 0;
  c->data_index = 0;
  c->symbol_count = 0;
  c->status = NoError;
  c->entry_point = 0;
  return NoError;
}

void ir_free(Compile* c) {
  free(c->ins);
  c->ins_count = 0;
}

void ir_print(Compile* c, FILE* fp) {
  fprintf(fp, "ins_count: %u\n", c->ins_count);
  for (u32 i = 0; i < c->ins_count; ++i) {
    Op* op = &c->ins[i];
    fprintf(fp, "%3u: <%s, %d, %d, %d>\n", i, ir_code_str[op->i], op->dest, op->src0, op->src1);
  }
}

// TODO(lucas): location printing
void ir_compile_error(Compile* c, const char* fmt, ...) {
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stderr;
  fprintf(fp, "[ir-compile-error]: %s", buffer);
  c->status = Error;
}

// TODO(lucas): location printing
void ir_compile_warning(Compile* c, const char* fmt, ...) {
  char buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stdout;
  fprintf(fp, "[ir-compile-warning]: %s", buffer);
}

i32 ir_push_ins(Compile* c, Op ins, u32* ins_count) {
  list_push(c->ins, c->ins_count, ins);
  if (ins_count) {
    (*ins_count)++;
  }
  return NoError;
}

i32 ir_push_value(Compile* c, void* value, u32 size) {
  i32 address = c->data_index;
  if (address + size < MAX_DATA) {
    memcpy(&c->data[c->data_index], value, size);
    c->data_index += size;
    return address;
  }
  return -1;
}

i32 ir_start_compile(Compile* c, Ast* ast) {
  REAL_TIMER_START();
  if (!ast) {
    return c->status;
  }
  assert("something went very wrong" && ast->type == AstRoot);
  for (i32 i = 0; i < ast->count; ++i) {
    if (ir_compile(c, ast->node[i], NULL) != NoError) {
      break;
    }
  }
  if (c->entry_point != 1) {
    ir_compile_error(c, "missing entry point `main`\n");
  }
  REAL_TIMER_END(
    print_info("%s: %g s\n", __FUNCTION__, _dt);
    (void)_dt;
  );
  return c->status;
}

i32 ir_declare_value(Compile* c, Token token, Symbol** symbol, i32* symbol_index) {
  if (MAX_NAME_SIZE < token.length) {
    return Error;
  }
  if (c->symbol_count < MAX_SYMBOL) {
    if (ir_lookup_value(c, token, symbol, NULL) == NoError) {
      return (c->status = Error);
    }
    if (symbol_index) {
      *symbol_index = c->symbol_count;
    }
    *symbol = &c->symbol_table[c->symbol_count++];
    Symbol* s = *symbol;
    symbol_init(s);
    s->token = token;
    strncpy(s->name, token.buffer, token.length);
    return NoError;
  }
  return Error;
}

i32 ir_lookup_value(Compile* c, Token token, Symbol** symbol, i32* symbol_index) {
  assert("must pass reference to a symbol to store the return value in" && symbol != NULL);
  if (MAX_NAME_SIZE < token.length) {
    return Error;
  }
  char copy[MAX_NAME_SIZE] = {0};
  strncpy(copy, token.buffer, token.length);
  for (u32 i = 0; i < c->symbol_count; ++i) {
    Symbol* sym = &c->symbol_table[i];
    if (strncmp(copy, sym->name, MAX_NAME_SIZE) == 0) {
      *symbol = sym;
      if (symbol_index) {
        *symbol_index = i;
      }
      return NoError;
    }
  }
  return Error;
}

i32 ir_compile(Compile* c, Ast* ast, u32* ins_count) {
  switch (ast->type) {
    case AstStatement: {
      return ir_compile_stmts(c, ast, ins_count);
    }
    case AstValue: {
      switch (ast->value.type) {
        case T_NUMBER: {
          u64 num = 0;
          str_to_int(ast->value.buffer, ast->value.length, &num);
          i32 address = ir_push_value(c, &num, sizeof(num));
          if (address >= 0) {
            ir_push_ins(c, (Op) {
              .i = I_PUSH_INT64,
              .dest = -1,
              .src0 = address,
              .src1 = -1,
            }, ins_count);
          }
          else {
            // TODO(lucas): Handle
          }
          break;
        }
        case T_POP: {
          // assert("T_POP: not handled yet" && 0);
          ir_push_ins(c, OP(I_POP), ins_count);
          break;
        }
        case T_PRINT: {
          assert("T_PRINT: not handled yet" && 0);
          break;
        }
        case T_IDENTIFIER: {
          Symbol* symbol = NULL;
          i32 symbol_index = -1;
          if (ir_lookup_value(c, ast->value, &symbol, &symbol_index) == NoError) {
            switch (symbol->type) {
              case TypeUnsigned64: {
                ir_push_ins(c, (Op) {
                  .i = I_PUSH_INT64,
                  .dest = 0,
                  .src0 = symbol->address,
                  .src1 = symbol_index,
                }, ins_count);
                break;
              }
              case TypeFunc: {
                ir_push_ins(c, (Op) {
                  .i = I_CALL,
                  .dest = symbol_index,
                  .src0 = -1,
                  .src1 = -1,
                }, ins_count);
                break;
              }
              default: {
                break;
              }
            }
          }
          else {
            ir_compile_error(c, "value `%.*s` not defined\n", ast->value.length, ast->value.buffer);
            return c->status;
          }
          break;
        }
        case T_AT: {
          Symbol* symbol = NULL;
          i32 index = -1;
          if (ir_lookup_value(c, ast->value, &symbol, &index) == NoError) {
            ir_push_ins(c, (Op) {
              .i = I_PUSH_ADDR_OF,
              .dest = 0,
              .src0 = symbol->address,
              .src1 = index,
            }, ins_count);
          }
          else {
            ir_compile_error(c, "value `%.*s` not defined\n", ast->value.length, ast->value.buffer);
            return c->status;
          }
          break;
        }
        default: {
          assert("value token not implemented" && 0);
          break;
        }
      }
      break;
    }
    // op arg0 arg1 -> arg0 arg1 op
    case AstBinopExpression: {
      i32 result = ir_compile_binop(c, ast, ins_count);
      if (result == NoError) {
        if (ast->value.type == T_ADD) {
          ir_push_ins(c, OP(I_ADD), ins_count);
        }
        else if (ast->value.type == T_SUB) {
          ir_push_ins(c, OP(I_SUB), ins_count);
        }
        else if (ast->value.type == T_LT) {
          ir_push_ins(c, OP(I_LT), ins_count);
        }
        else {
          // Handle
        }
      }
      else {
        c->status = result;
        return c->status;
      }
      break;
    }
    case AstUopExpression: {
      i32 result = ir_compile_uop(c, ast, ins_count);
      if (result == NoError) {
        if (ast->value.type == T_PRINT) {
          ir_push_ins(c, OP(I_PRINT), ins_count);
        }
        else if (ast->value.type == T_DEREF) {
          ir_push_ins(c, OP(I_LOAD64), ins_count);
        }
        else {
          // Handle
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
    case AstConstAssignment: {
      Symbol* symbol = NULL;
      i32 symbol_index = -1;
      if (ir_compile_stmts(c, ast, ins_count) == NoError) {
        if (ir_declare_value(c, ast->value, &symbol, &symbol_index) == NoError) {
          u64 empty = 0;
          u32 empty_size = sizeof(empty);
          symbol->address = ir_push_value(c, &empty, empty_size);
          symbol->size = sizeof(empty);
          symbol->type = TypeUnsigned64;

          ir_push_ins(c, (Op) { // store contents of rax into [v]
            .i = I_COPY,
            .dest = symbol_index,
            .src0 = -1,
            .src1 = -1,
          }, ins_count);
        }
        else {
          ir_compile_error(c, "symbol `%.*s` has already been declared\n", ast->value.length, ast->value.buffer);
          c->status = Error;
        }
      }
      else {
        // TODO: handle
      }
      break;
    }
    case AstLetStatement: {
      assert("AstLetStatement not implemented yet" && 0);
      break;
    }
    case AstFuncDefinition: {
      ir_compile_func(c, ast, ins_count);
      break;
    }
    case AstStatementList: {
      ir_compile_stmts(c, ast, ins_count);
      break;
    }
    case AstMemoryStatement: {
      Symbol* symbol = NULL;
      i32 symbol_index = -1;
      if (ir_declare_value(c, ast->value, &symbol, &symbol_index) == NoError) {
        assert(ast->count == 1);
        Token token = ast->node[0]->value;
        assert(token.type == T_NUMBER);
        u64 storage_size = 0;
        str_to_int(token.buffer, token.length, &storage_size);
        symbol->address = -1;
        symbol->size = storage_size;
        symbol->type = TypeUnsigned64;
      }
      else {
        ir_compile_error(c, "symbol `%.*s` has already been declared\n", ast->value.length, ast->value.buffer);
        c->status = Error;
      }
      break;
    }
    case AstAssignment: {
      i32 result = ir_compile_binop(c, ast, ins_count);
      if (result == NoError) {
        ir_push_ins(c, OP(I_STORE64), ins_count);
      }
      else {
        c->status = result;
        return c->status;
      }
      break;
    }
    default: {
      ir_compile_error(c, "invalid AST branch type\n");
      c->status = Error;
      break;
    }
  }
  return c->status;
}

i32 ir_compile_stmts(Compile* c, Ast* ast, u32* ins_count) {
  for (i32 i = 0; i < ast->count; ++i) {
    if (ir_compile(c, ast->node[i], ins_count) != NoError) {
      break;
    }
  }
  return c->status;
}

i32 ir_compile_binop(Compile* c, Ast* ast, u32* ins_count) {
  if (ast->count < 2) {
    ir_compile_error(c, "expected 2 arguments in binary operator action\n");
    c->status = Error;
    return c->status;
  }
  for (i32 i = 0; i < ast->count; ++i) {
    Ast* node = ast->node[i];
    i32 result = ir_compile(c, node, ins_count);
    if (result != NoError) {
      c->status = result;
      return c->status;
    }
  }
  return c->status;
}

// alias of ir_compile_binop
i32 ir_compile_uop(Compile* c, Ast* ast, u32* ins_count) {
  for (i32 i = 0; i < ast->count; ++i) {
    Ast* node = ast->node[i];
    i32 result = ir_compile(c, node, ins_count);
    if (result != NoError) {
      c->status = result;
      return c->status;
    }
  }
  return c->status;
}

i32 ir_compile_func(Compile* c, Ast* ast, u32* ins_count) {
  Symbol* symbol = NULL;
  i32 symbol_index = -1;
  if (ir_declare_value(c, ast->value, &symbol, &symbol_index) == NoError) {
    ir_push_ins(c, (Op) {
      .i = I_LABEL,
      .dest = symbol_index,
      .src0 = -1,
      .src1 = -1,
    }, ins_count);
    symbol->size = 0;
    symbol->type = TypeFunc;
    symbol->value.func = (Function) {
      .address = c->ins_count,
      .argc = 0,
    };

    u32 func_size = 0;
    ir_compile_stmts(c, ast, &func_size);
    ir_push_ins(c, OP(I_RET), ins_count);

    if (!strcmp(symbol->name, "main")) {
      c->entry_point = 1;
    }
  }
  else {
    ir_compile_error(c, "symbol `%.*s` has already been declared\n", ast->value.length, ast->value.buffer);
  }
  return c->status;
}

i32 program_typecheck(Ast* ast) {
  return NoError;
}

i32 compile(Compile* c, Compile_target target, FILE* fp) {
  if (target >= 0 && target < MAX_COMPILE_TARGET) {
    REAL_TIMER_START();
    compile_targets[target](c, fp);
    REAL_TIMER_END(
      print_info("%s: %g s (%s)\n", __FUNCTION__, _dt, compile_target_str[target]);
      (void)_dt;
    );
  }
  else {
    assert("invalid compile target" && 0);
  }
  return c->status;
}

i32 compile_linux_nasm_x86_64(Compile* c, FILE* fp) {
#define o(...) fprintf(fp, __VA_ARGS__)
#define ENTRY "_start"
#define MEMORY_CAPACITY KB(512)
  o("bits 64\n");
  o("section .text\n");
  o(
  "print:\n"
  "    mov     r9, -3689348814741910323\n"
  "    sub     rsp, 40\n"
  "    mov     BYTE [rsp+31], 10\n"
  "    lea     rcx, [rsp+30]\n"
  ".L2:\n"
  "    mov     rax, rdi\n"
  "    lea     r8, [rsp+32]\n"
  "    mul     r9\n"
  "    mov     rax, rdi\n"
  "    sub     r8, rcx\n"
  "    shr     rdx, 3\n"
  "    lea     rsi, [rdx+rdx*4]\n"
  "    add     rsi, rsi\n"
  "    sub     rax, rsi\n"
  "    add     eax, 48\n"
  "    mov     BYTE [rcx], al\n"
  "    mov     rax, rdi\n"
  "    mov     rdi, rdx\n"
  "    mov     rdx, rcx\n"
  "    sub     rcx, 1\n"
  "    cmp     rax, 9\n"
  "    ja      .L2\n"
  "    lea     rax, [rsp+32]\n"
  "    mov     edi, 1\n"
  "    sub     rdx, rax\n"
  "    xor     eax, eax\n"
  "    lea     rsi, [rsp+32+rdx]\n"
  "    mov     rdx, r8\n"
  "    mov     rax, 1\n"
  "    syscall\n"
  "    add     rsp, 40\n"
  "    ret\n"
  );
  o("global %s\n", ENTRY);

  for (i32 i = 0; i < c->ins_count; ++i) {
    const Op* op = &c->ins[i];
    switch (op->i) {
      case I_NOP: {
        o("  nop\n");
        break;
      }
      case I_POP: {
        o("  pop rax\n");
        break;
      }
      case I_COPY: {
        o("  pop rax\n");
        o("  mov [v%i], rax\n", op->dest);
        break;
      }
      case I_STORE64: {
        o(
        "  pop rbx\n"
        "  pop rax\n"
        "  mov [rax], rbx\n"
        );
        break;
      }
      case I_PUSH_INT64: {
        if (op->src1 >= 0) {
          if (op->src1 < c->symbol_count) {
            o("  mov rax, [v%d]\n", op->src1);
            o("  push rax\n");
          }
          else {
            assert("unhandled error" && 0);
            // TODO: handle
          }
          break;
        }
        i32 value = *(i32*)&c->data[op->src0];
        o("  mov rax, %d\n", value);
        o("  push rax\n");
        break;
      }
      case I_PUSH_ADDR_OF: {
        if (op->src1 >= 0) {
          if (op->src1 < c->symbol_count) {
            o("  mov rax, v%d\n", op->src1);
            o("  push rax\n");
          }
          else {
            assert("unhandled error" && 0);
            // TODO: handle
          }
          break;
        }
        assert(0);
        break;
      }
      case I_LOAD64: {
        o(
        "  pop rbx\n"
        "  mov rax, [rbx]\n"
        "  push rax\n"
        );
        break;
      }
      case I_ADD: {
        o("  pop rax\n");
        o("  pop rbx\n");
        o("  add rbx, rax\n");
        o("  push rbx\n");
        break;
      }
      case I_SUB: {
        o(
        "  pop rax\n"
        "  pop rbx\n"
        "  sub rbx, rax\n"
        "  push rbx\n"
        );
        break;
      }
      case I_LT: {
        o(
        "  mov rcx, 0\n"
        "  mov rdx, 1\n"
        "  pop rax\n"
        "  pop rbx\n"
        "  cmp rax, rbx\n"
        "  cmovl rcx, rdx\n"
        "  push rcx\n"
        );
        break;
      }
      case I_RET: {
        o("  pop rax\n");
        o("  ret\n");
        break;
      }
      case I_PRINT: {
        o(
        "  pop rdi\n"
        "  call print\n"
        );
        break;
      }
      case I_LABEL: {
        assert(op->dest < c->symbol_count);
        Symbol* symbol = &c->symbol_table[op->dest];
        if (strcmp(symbol->name, "main") == 0) {
          o("%s:\n", symbol->name);
        }
        else {
          o("v%d: ; `%s`\n", op->dest, symbol->name);
        }
        break;
      }
      case I_CALL: {
        assert(op->dest < c->symbol_count);
        Symbol* symbol = &c->symbol_table[op->dest];
        o("  call v%d ; `%s`\n", op->dest, symbol->name);
        o("  push rax ; function result\n");
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
  o("  call main\n");
  o("  mov rax, 60 ; exit syscall\n");
  o("  mov rdi, 0\n");
  o("  syscall\n");
  o("section .data\n");
  o("section .bss\n");
  for (i32 i = 0; i < c->symbol_count; ++i) {
    Symbol* s = &c->symbol_table[i];
    switch (s->type) {
      case TypeUnsigned64: {
        o("v%d: resb %d ; `%s` : TypeUnsigned64\n", i, s->size, s->name);
        break;
      }
      case TypeFunc: {
        // do nothing
        break;
      }
      default: {
        assert("type not implemented yet" && 0);
        break;
      }
    }
  }

  o("memory: resb %d\n", MEMORY_CAPACITY);
  return c->status;
}
#undef o

i32 parser_init(Parser* p, char* source) {
  if (!source) {
    return Error;
  }
  lexer_init(&p->l, source);
  p->ast = ast_create(AstRoot);
  p->current = NULL;
  p->status = NoError;
  return NoError;
}

void parser_free(Parser* p) {
  ast_free(p->ast);
  p->current = NULL;
}

void parser_error(Parser* p, const char* fmt, ...) {
  char err_buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(err_buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  Lexer* l = &p->l;

  FILE* fp = stderr;
  fprintf(fp, "[parse-error]: %d:%d: %s", l->token.line, l->token.column, err_buffer);
  error_printline(fp, l->source, l->index, l->token.length);
  l->token.type = T_EOF;
  p->status = Error;
}

i32 parse(Parser* p) {
  REAL_TIMER_START();
  lexer_next(&p->l);
  ast_push(p->ast, parse_statements(p));
  REAL_TIMER_END(
    print_info("%s: %g s\n", __FUNCTION__, _dt);
    (void)_dt;
  );
  return p->status;
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
      ;
*/
Ast* parse_statement(Parser* p) {
  Token t = lexer_peek(&p->l);
  switch (t.type) {
    case T_CONST: {
      t = lexer_next(&p->l); // skip `const`
      if (t.type != T_IDENTIFIER) {
        parser_error(p, "expected identifier in const statement, but got `%.*s`\n", t.length, t.buffer);
        return NULL;
      }
      lexer_next(&p->l);  // skip `identifier`
      Ast* expr = ast_create(AstConstAssignment);
      expr->value = t;
      ast_push(expr, parse_expr(p));
      if (lexer_peek(&p->l).type != T_SEMICOLON) {
        parser_error(p, "expected `;` semicolon after statement, but got `%.*s`\n", t.length, t.buffer);
      }
      return expr;
    }
    case T_LET: {
      assert("T_LET not implemented yet" && 0);
      break;
    }
    case T_MEMORY: {
      t = lexer_next(&p->l); // skip `memory`
      if (t.type != T_IDENTIFIER) {
        parser_error(p, "expected identifier in memory statement, but got `%.*s`\n", t.length, t.buffer);
        return NULL;
      }
      Ast* expr = ast_create(AstMemoryStatement);
      expr->value = t;
      t = lexer_next(&p->l);  // skip `identifier`
      if (t.type != T_NUMBER) {
        parser_error(p, "expected number in memory statement, but got `%.*s`\n", t.length, t.buffer);
        return expr;
      }
      ast_push_node(expr, AstValue, t);
      lexer_next(&p->l); // skip NUMBER
      if (lexer_peek(&p->l).type != T_SEMICOLON) {
        parser_error(p, "expected `;` semicolon after memory statement, but got `%.*s`\n", t.length, t.buffer);
      }
      return expr;
    }
    // `{` stmts `}`
    case T_LEFT_CURLY: {
      lexer_next(&p->l); // skip `{`
      Ast* sub_stmts = parse_statements(p);
      if (sub_stmts) {
        t = lexer_peek(&p->l);
        if (t.type != T_RIGHT_CURLY) {
          parser_error(p, "expected closing `}` curly bracket in, but got `%.*s`\n", t.length, t.buffer);
        }
        lexer_next(&p->l);
        return sub_stmts;
      }
      break;
    }
    case T_ASSIGN: {
      lexer_next(&p->l); // skip `=`
      Ast* assignment = ast_create(AstAssignment);
      assignment->value = t;
      ast_push(assignment, parse_expr(p));
      ast_push(assignment, parse_expr(p));
      return assignment;
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
    case T_IDENTIFIER: {
      lexer_next(&p->l);
      Ast* node = ast_create(AstValue);
      node->value = t;
      return node;
    }
    case T_ADD:
    case T_SUB:
    case T_LT: {
      lexer_next(&p->l);
      Ast* expr = ast_create(AstBinopExpression);
      expr->value = t;
      ast_push(expr, parse_expr(p));
      ast_push(expr, parse_expr(p));
      return expr;
    }
    case T_AT: {
      t = lexer_next(&p->l);
      if (t.type == T_IDENTIFIER) {
        Ast* expr = ast_create(AstValue);
        expr->value = t;
        expr->value.type = T_AT;
        lexer_next(&p->l);
        return expr;
      }
      parser_error(p, "expected identifer after `@` operator, but got `%.*s`\n", t.length, t.buffer);
      return NULL;
    }
    case T_DEREF:
    case T_PRINT: {
      lexer_next(&p->l); // skip op
      Ast* expr = ast_create(AstUopExpression);
      expr->value = t;
      Ast* sub_expr = parse_expr(p);
      if (sub_expr != NULL) {
        ast_push(expr, sub_expr);
        return expr;
      }
      parser_error(p, "invalid syntax in unary operator expression\n");
      return NULL;
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
    default: {
      parser_error(p, "unexpected `%.*s` in expression\n", t.length, t.buffer);
      p->status = Error;
      return NULL;
    }
  }
  return NULL;
}

Ast* parse_func_def(Parser* p) {
  Token t = lexer_next(&p->l); // skip `fn`
  if (t.type == T_IDENTIFIER) {
    Ast* func_def = ast_create(AstFuncDefinition);
    func_def->value = t;
    t = lexer_next(&p->l); // skip `name`
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
    return func_def;
  }
  parser_error(p, "expected identifier in function definition, but got `%.*s`\n", t.length, t.buffer);
  return NULL;
}

void lexer_init(Lexer* l, char* source) {
  l->token = (Token) {
    .buffer = source,
    .length = 1,
    .type = T_EOF,
    .line = 1,
    .column = 1,
  };
  l->source = source;
  l->index = source;
  l->line = 1;
  l->column = 1;
  l->status = NoError;
}

void lexer_error(Lexer* l, const char* fmt, ...) {
  char err_buffer[MAX_ERR_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(err_buffer, MAX_ERR_SIZE, fmt, args);
  va_end(args);

  FILE* fp = stderr;
  fprintf(fp, "[lex-error]: %d:%d: %s", l->token.line, l->token.column, err_buffer);
  error_printline(fp, l->source, l->index, l->token.length);
  l->token.type = T_EOF;
  l->status = Error;
}

void next(Lexer* l) {
  l->token.buffer = l->index;
  l->token.length = 1;
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
    *l->index == '-') {
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
  else {
    l->token.type = T_IDENTIFIER;
  }
  return l->token;
}

Token lexer_read_number(Lexer* l) {
  while (is_digit(*l->index)) {
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

i32 is_alpha(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

i32 is_extended_ascii(u8 ch) {
  return (ch >= 128 && ch < 255);
}

Token lexer_next(Lexer* l) {
  for (;;) {
    next(l);
    char ch = *l->token.buffer;
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
        }
        break;
      }
      case '=': {
        l->token.type = T_ASSIGN;
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
        l->token.type = T_SUB;
        goto done;
      }
      case '<': {
        l->token.type = T_LT;
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

void error_printline(FILE* fp, char* source, char* index, i32 token_length) {
  assert(source);
  index -= token_length;
  i32 offset = index - source;
  i32 start_offset = 0;
  i32 end_offset = 0;

  // scan to the beginning of the line
  char* at = index;
  while (start_offset < offset) {
    start_offset++;
    char ch = *(at - start_offset);
    if (ch == '\n' || ch == '\r') {
      break;
    }
  }
  // and then we scan to the end of the line
  do {
    char ch = *(at + end_offset);
    if (ch == '\n' || ch == '\r' || ch == '\0') {
      break;
    }
    end_offset++;
  } while (1);
  i32 line_length = end_offset + start_offset - 1;
  fprintf(fp, "%.*s\n", line_length, at - start_offset + 1);
  for (i32 i = 0; i < start_offset - 1; ++i) {
    fprintf(fp, "-");
  }
  fprintf(fp, "^\n");
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

i32 str_to_int(char* str, i32 length, u64* out) {
  *out = 0;
  for (i32 i = 0; i < length; ++i) {
    char ch = str[i];
    if (is_digit(ch)) {
      *out = *out * 10 + (str[i] - '0');
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
    .value = {0},
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

Ast* ast_push_node(Ast* ast, Ast_type type, Token value) {
  if (!ast) {
    return NULL;
  }
  Ast* node = ast_alloc_node();
  if (!node) {
    return NULL;
  }
  node->type = type;
  node->value = value;
  return ast_push(ast, node);
}

Ast* ast_push(Ast* ast, Ast* node) {
  if (!ast || !node) {
    return NULL;
  }
  if (ast->count == 0) {
    ast->node = malloc(sizeof(Ast*));
  }
  else {
    Ast** tmp = realloc(ast->node, sizeof(Ast*) * (ast->count + 1));
    if (!tmp) {
      fprintf(stderr, "Failed to resize list of ast node references\n");
      return NULL;
    }
    ast->node = tmp;
  }
  ast->node[ast->count++] = node;
  return node;
}

void ast_print(const Ast* ast, i32 level, FILE* fp) {
  if (!ast) {
    return;
  }
  for (i32 i = 0; i < level; ++i, fprintf(fp, "    "));
  assert("something went very wrong" && ast->type < MAX_AST_TYPE && ast->value.type < MAX_TOKEN_TYPE);
  fprintf(fp, "<%s, %s>: `%.*s`\n", ast_type_str[ast->type], token_type_str[ast->value.type],  ast->value.length, ast->value.buffer);
  for (i32 i = 0; i < ast->count; ++i) {
    ast_print(ast->node[i], level + 1, fp);
  }
}

void ast_free(Ast* ast) {
  if (!ast) {
    return;
  }
  for (i32 i = 0; i < ast->count; ++i) {
    ast_free(ast->node[i]);
  }
  if (ast->node) {
    free(ast->node);
    ast->count = 0;
  }
  if (ast) {
    free(ast);
    ast = NULL;
  }
}
