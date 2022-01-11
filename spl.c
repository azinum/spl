// spl.c - simple programming language

#include <stdio.h>  // puts, printf
#include <string.h> // strcmp
#include <stdlib.h> // malloc, fread
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

typedef float f32;
typedef double f64;
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
  T_ADD,
  T_SUB,
  T_SEMICOLON,
  T_POP,
  T_LEFT_P,
  T_RIGHT_P,

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
  "T_ADD",
  "T_SUB",
  "T_SEMICOLON",
  "T_POP",
  "T_LEFT_P",
  "T_RIGHT_P",
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
  AstBinopExpression,

  MAX_AST_TYPE,
} Ast_type;

static const char* ast_type_str[] = {
  "AstNone",
  "AstRoot",
  "AstValue",
  "AstExpression",
  "AstStatement",
  "AstBinopExpression",
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
  I_PUSH_INT,
  I_ADD,
  I_SUB,
  I_RET,

  MAX_IR_CODE,
} Ir_code;

static const char* ir_code_str[] = {
  "I_NOP",
  "I_POP",
  "I_COPY",
  "I_PUSH_INT",
  "I_ADD",
  "I_SUB",
  "I_RET",
};

// intermidiate representation of the instructions which are to be generated or interpreted
typedef struct Op {
  Ir_code i;
  i32 dest;
  i32 src0;
  i32 src1;
} Op;

#define OP(_i) ((Op) {.i = _i, })

#define MAX_INS (1024) // temp
#define MAX_DATA (KB(32)) // temp

// compile state
typedef struct Compile {
  Op* ins;
  u32 ins_count;
  u8 data[MAX_DATA];
  u32 data_index;
  i32 status;
} Compile;

static i32 ir_init(Compile* c);
static void ir_free(Compile* c);
static void ir_print(Compile* c, FILE* fp);
static void ir_compile_error(Compile* c, const char* fmt, ...);
static i32 ir_start_compile(Compile* c, Ast* ast);
static i32 ir_push_ins(Compile* c, Op ins, i32* ins_count);
static i32 ir_push_value(Compile* c, void* value, u32 size);
static i32 ir_compile(Compile* c, Ast* ast);
static i32 ir_compile_stmt(Compile* c, Ast* ast);
static i32 ir_compile_expr(Compile* c, Ast* ast); // NOTE(lucas): these will probably change to something more specific
static i32 ir_compile_binop(Compile* c, Ast* ast);

static i32 program_typecheck(Ast* ast);

static i32 compile_linux_nasm_x86_64(Compile* c, FILE* fp);

static i32 parser_init(Parser* p, char* source);
static void parser_free(Parser* p);
static void parser_error(Parser* p, const char* fmt, ...);
static i32 parse(Parser* p);
static i32 parse_statements(Parser* p);
static Ast* parse_statement(Parser* p);
static Ast* parse_expr(Parser* p);
static void lexer_init(Lexer* l, char* source);
static void lexer_error(Lexer* l, const char* fmt, ...);
static void next(Lexer* l);
inline i32 compare(Token t, const char* b);
inline i32 expect(Token t, Token_type type);
inline Token lexer_read_symbol(Lexer* l);
inline Token lexer_read_number(Lexer* l);
inline i32 is_digit(char ch);
inline i32 is_alpha(char ch);
static Token lexer_next(Lexer* l);
static Token lexer_peek(Lexer* l);

static void error_printline(FILE* fp, char* source, char* index, i32 token_length);
static i32 str_to_int(char* str, i32 length, i32* out);

static Ast* ast_create(Ast_type type);
static void ast_init_node(Ast* node);
static Ast* ast_alloc_node();
static Ast* ast_push_node(Ast* ast, Ast_type type, Token value);
static Ast* ast_push(Ast* ast, Ast* node);
static void ast_print(const Ast* ast, i32 level, FILE* fp);
static void ast_free(Ast* ast);

i32 main(i32 argc, char** argv) {
  (void)ast_print; (void)ir_code_str;
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
          ast_print(p.ast, 0, stdout);
          if (ir_start_compile(&c, p.ast) == NoError) {
            ir_print(&c, stdout);

            char path[MAX_PATH_SIZE] = {0};
            snprintf(path, MAX_PATH_SIZE, "%s.asm", filename);
            FILE* fp = fopen(path, "w");
            if (fp) {
              compile_linux_nasm_x86_64(&c, fp);
              fclose(fp);
            }
          }
          else {
            // TODO: Handle
          }
          ir_free(&c);
        }
        else {
          // TODO: Handle
        }
      }
    }
    else {
      // Handle
    }
#if 0
      ast_print(p.ast, 0, stdout);
#endif
    parser_free(&p);
  }
  if (fp) {
    fclose(fp);
  }
  free(source);
  return EXIT_SUCCESS;
}

i32 ir_init(Compile* c) {
  c->ins = NULL;
  c->ins_count = 0;
  c->data_index = 0;
  c->status = NoError;
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

i32 ir_push_ins(Compile* c, Op ins, i32* ins_count) {
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
  if (!ast) {
    return c->status;
  }
  assert("something went very wrong" && ast->type == AstRoot);
  for (i32 i = 0; i < ast->count; ++i) {
    if (ir_compile(c, ast->node[i]) != NoError) {
      break;
    }
  }
  ir_push_ins(c, OP(I_RET), NULL);
  return c->status;
}

i32 ir_compile(Compile* c, Ast* ast) {
  switch (ast->type) {
    case AstExpression: {
      return ir_compile_expr(c, ast);
    }
    case AstStatement: {
      return ir_compile_stmt(c, ast);
    }
    case AstValue: {
      switch (ast->value.type) {
        case T_NUMBER: {
          i32 num = 0;
          str_to_int(ast->value.buffer, ast->value.length, &num);
          i32 address = ir_push_value(c, &num, sizeof(num));
          if (address >= 0) {
            ir_push_ins(c, (Op) {
              .i = I_PUSH_INT,
              .dest = 0,
              .src0 = address,
              .src1 = 0,
            }, NULL);
          }
          else {
            // TODO(lucas): Handle
          }
          break;
        }
        case T_POP: {
          ir_push_ins(c, OP(I_POP), NULL);
          break;
        }
        case T_IDENTIFIER: {
          assert("not handled yet" && 0);
          break;
        }
        default: {
          assert(0);
          break;
        }
      }
      break;
    }
    // op arg0 arg1 -> arg0 arg1 op
    case AstBinopExpression: {
      i32 result = ir_compile_binop(c, ast);
      if (result == NoError) {
        if (ast->value.type == T_ADD) {
          ir_push_ins(c, OP(I_ADD), NULL);
        }
        else if (ast->value.type == T_ADD) {
          ir_push_ins(c, OP(I_SUB), NULL);
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
    default: {
      ir_compile_error(c, "invalid AST branch type\n");
      c->status = Error;
      break;
    }
  }
  return c->status;
}

i32 ir_compile_stmt(Compile* c, Ast* ast) {
  for (i32 i = 0; i < ast->count; ++i) {
    if (ir_compile(c, ast->node[i]) != NoError) {
      break;
    }
  }
  return c->status;
}

i32 ir_compile_expr(Compile* c, Ast* ast) {
  return c->status;
}

i32 ir_compile_binop(Compile* c, Ast* ast) {
  for (i32 i = 0; i < ast->count; ++i) {
    Ast* node = ast->node[i];
    i32 result = ir_compile(c, node);
    if (result != NoError) {
      c->status = result;
      return c->status;
    }
  }
  return c->status;
}
i32 program_typecheck(Ast* ast) {
  return NoError;
}

i32 compile_linux_nasm_x86_64(Compile* c, FILE* fp) {
#define o(...) fprintf(fp, __VA_ARGS__)
#define ENTRY "_start"
#define MEMORY_CAPACITY KB(512)

  o("bits 64\n");
  o("section .text\n");
  o("global %s\n", ENTRY);
  o("%s:\n", ENTRY);

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
      case I_PUSH_INT: {
        i32 value = *(i32*)&c->data[op->src0];
        o("  mov rax, %d\n", value);
        o("  push rax\n");
        break;
      }
      case I_ADD: {
        o("  pop rax\n");
        o("  pop rbx\n");
        o("  add rax, rbx\n");
        o("  push rbx\n");
        break;
      }
      case I_SUB: {
        o("  pop rax\n");
        o("  pop rbx\n");
        o("  sub rax, rbx\n");
        o("  push rbx\n");
        break;
      }
      default: {
        break;
      }
    }
  }

  o("\n");
  o("  mov rax, 60 ; exit syscall\n");
  o("  mov rdi, 0\n");
  o("  syscall\n");
  o("section .data\n");
  o("section .bss\n");
  o("memory: resb %d\n", MEMORY_CAPACITY);
  return NoError;
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
}

i32 parse(Parser* p) {
  return parse_statements(p);
}

/*
 
 stmts : stmt
       | stmts stmt
       | ';' stmt
       ;
*/
i32 parse_statements(Parser* p) {
  Ast* statements = p->ast;
  for (;;) {
    Token t = lexer_next(&p->l);
    switch (t.type) {
      case T_EOF: {
        return NoError;
      }
      case T_SEMICOLON: {
        break;
      }
      default: {
        Ast* stmt = parse_statement(p);
        if (!stmt) {
          goto done;
        }
        ast_push(statements, stmt);
        break;
      }
    }
  }
done:
  return p->status;
}

/*
 stmt : expr ';'
      | ident '=' expr
      ;
*/
Ast* parse_statement(Parser* p) {
  Token t = lexer_peek(&p->l);
  switch (t.type) {
    case T_IDENTIFIER: {
      parser_error(p, "identifier not allowed at here\n");
      p->status = Error;
      return NULL;
      break;
    }
    case T_SEMICOLON: {
      break;
    }
    default: {
      Ast* expr = parse_expr(p);
      return expr;
    }
  }
  assert("something went wrong" && 0);
  return NULL;
}

/*
  expr : expr
       | op expr
       | op expr expr
       | number
       | identifier
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
    case T_SUB: {
      lexer_next(&p->l);
      Ast* expr = ast_create(AstBinopExpression);
      expr->value = t;
      ast_push(expr, parse_expr(p));
      ast_push(expr, parse_expr(p));
      return expr;
    }
    case T_LEFT_P: {
      lexer_next(&p->l);
      Ast* expr = parse_expr(p);
      t = lexer_peek(&p->l);
      if (t.type != T_RIGHT_P) {
        parser_error(p, "missing closing `)` parenthesis\n");
        return NULL;
      }
      lexer_next(&p->l); // skip ')'
      return expr;
    }
    case T_RIGHT_P: {
      lexer_next(&p->l);
      break;
    }
    default: {
      parser_error(p, "expected ??? but got `%.*s`\n", t.length, t.buffer);
      p->status = Error;
      return NULL;
    }
  }
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
    *l->index == '_' ||
    *l->index == '-') {
    l->index++;
    l->column++;
  }
  l->token.length = l->index - l->token.buffer;
  if (compare(l->token, "pop")) {
    l->token.type = T_POP;
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
      case '+': {
        l->token.type = T_ADD;
        goto done;
      }
      case '-': {
        l->token.type = T_SUB;
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
      case ' ': case '\t': case '\f': case '\v': {
        break;
      }
      case '\0': {
        l->token.type = T_EOF;
        goto done;
      }
      default: {
        if (is_alpha(ch) || ch == '_') {
          lexer_read_symbol(l);
          goto done;
        }
        else if (is_digit(ch)) {
          lexer_read_number(l);
          goto done;
        }
        lexer_error(l, "Unrecognized token `%.*s`\n", l->token.length, l->token.buffer);
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

i32 str_to_int(char* str, i32 length, i32* out) {
  *out = 0;
  for (i32 i = 0; i < length; ++i) {
    char ch = str[i];
    if (is_digit(ch)) {
      *out = *out * 10 + (str[i] - '0');
      continue;
    }
    *out = -1;
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
