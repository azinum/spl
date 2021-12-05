/* spl.cpp - simple programming language */

#include <stdio.h>  /* puts, printf */
#include <string.h> /* strcmp */
#include <stdlib.h> /* malloc, fread */
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

typedef enum Error_code {
  Error = -1,
  NoError = 0,
} Error_code;

typedef float f32;
typedef double f64;
typedef int32_t i32;
typedef uint32_t u32;
typedef int16_t i16;
typedef uint16_t u16;
typedef int8_t i8;
typedef uint8_t u8;

#define KB(n) (n * 1024)
#define MB(n) (KB(n * 1024))
#define GB(n) (MB(n * 1024))

#define MAX_ERR_SIZE 512

typedef enum Token_type {
  T_EOF = 0,

  T_IDENTIFIER,
  T_NUMBER,
  T_ASSIGN,
  T_SEMICOLON,
} Token_type;

typedef struct Token {
  char* buffer;
  i32 length;
  Token_type type;
  i32 line;
  i32 column;
} Token;

typedef struct Lexer {
  Token token;
  char* source; /* null terminated input string  */
  char* index;  /* pointer alias of source */
  i32 line;
  i32 column;
} Lexer;

typedef enum Ir_code {
  I_NOP = 0,
  I_COPY,
  I_ADD,
} Ir_code;

/* intermidiate representation of the instructions which are to be generated or interpreted */
typedef struct Op {
  Ir_code i;
  i32 dest;
  i32 src0;
  i32 src1;
} Op;

#define MAX_INS 1024

/* compile state */
typedef struct Compile {
  i32 status;
  Op ins[MAX_INS];
  u32 ins_count;
} Compile;

typedef Token Value;
typedef enum Ast_type {
  AstNone = 0,
  AstValue,
  AstExpression,
  AstStatement,

  MAX_AST_TYPE,
} Ast_type;

static const char* ast_type_str[] = {
  "AstNone",
  "AstValue",
  "AstExpression",
  "AstStatement",
};

typedef struct Ast {
  struct Ast** node;
  u32 count;
  Ast_type type;
  Value value;
} Ast;

/*
  frontend: input -> lexer -> parser -> ast/other data structure -> intermidiate representation ->
  backend:  -> code optimization -> code generation (or interpret the intermidiate representation)
*/
typedef struct Parser {
  Lexer l;
  Ast* ast;
  Ast* current;
} Parser;

static i32 parser_init(Parser* p, char* source);
static void parser_free(Parser* p);
static void parser_error(Parser* p, const char* fmt, ...);
static i32 parse(Parser* p);
static i32 parse_statements(Parser* p);
static Ast* parse_statement(Parser* p);
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

static Ast* ast_create();
static void ast_init_node(Ast* node);
static Ast* ast_alloc_node();
static Ast* ast_push_node(Ast* ast, Ast_type type, Value value);
static Ast* ast_push(Ast* ast, Ast* node);
static void ast_print(const Ast* ast, i32 level, FILE* fp);
static void ast_free(Ast* ast);

i32 main(i32 argc, char** argv) {
  char* source = NULL;
  FILE* fp = NULL;
  if (argc < 2) {
    printf("Usage; %s [-t | <filename>]\n", argv[0]);
    return EXIT_SUCCESS;
  }
  else {
    char* filename = argv[1];
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
      source = (char*)malloc(size);
      if (!source) {
        fprintf(stderr, "Failed to allocate memory for source file `%s`\n", filename);
        fclose(fp);
      }
      i32 read_size = fread(source, 1, size, fp);
      assert("something went wrong when reading file" && read_size == size);
      (void)read_size;  // Ignore
    }
  }

  Parser p;
  if (parser_init(&p, (char*)source) == NoError) {
    if (parse(&p) == NoError) {
      printf("Parsing complete\n");
      ast_print(p.ast, 0, stdout);
    }
    parser_free(&p);
  }
  if (fp) {
    fclose(fp);
  }
  free(source);
  return 0;
}

i32 parser_init(Parser* p, char* source) {
  if (!source) {
    return Error;
  }
  lexer_init(&p->l, source);
  p->ast = ast_create();
  p->current = NULL;
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
  i32 result = NoError;
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
  return result;
}

/*
 stmt : identifier ';'
      | number ';'
      ;
*/
Ast* parse_statement(Parser* p) {
  Ast* stmt = ast_alloc_node();
  if (!stmt) return NULL;
  stmt->type = AstStatement;

  Token t = lexer_peek(&p->l);
  switch (t.type) {
    case T_IDENTIFIER: {
      ast_push_node(stmt, AstValue, t);
      t = lexer_next(&p->l);
      if (!expect(t, T_SEMICOLON)) {
        parser_error(p, "expected `;` semicolon, but got `%.*s`\n", t.length, t.buffer);
        return NULL;
      }
      break;
    }
    case T_NUMBER: {
      ast_push_node(stmt, AstValue, t);
      t = lexer_next(&p->l);
      if (!expect(t, T_SEMICOLON)) {
        parser_error(p, "expected `;` semicolon, but got `%.*s`\n", t.length, t.buffer);
        return NULL;
      }
      break;
    }
    default: {
      parser_error(p, "expected identifier or number, but got `%.*s`\n", t.length, t.buffer);
      return NULL;
    }
  }

  return stmt;
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
  while (is_alpha(*l->index) || is_digit(*l->index) || *l->index == '_') {
    l->index++;
    l->column++;
  }
  l->token.length = l->index - l->token.buffer;
  if (compare(l->token, "nop")) {
    assert("token `nop` not implemented" && 0);
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
  return ch >= '0' && ch <= '9';
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
      case '=': {
        l->token.type = T_ASSIGN;
        goto done;
      }
      case ';': {
        l->token.type = T_SEMICOLON;
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

  /* Scan to the beginning of the line */
  char* at = index;
  while (start_offset < offset) {
    start_offset++;
    char ch = *(at - start_offset);
    if (ch == '\n' || ch == '\r') {
      break;
    }
  }
  /* And then we scan to the end of the line */
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

Ast* ast_create() {
  Ast* ast = ast_alloc_node();
  ast_init_node(ast);
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

Ast* ast_push_node(Ast* ast, Ast_type type, Value value) {
  Ast* node = ast_alloc_node();
  if (!node) {
    return NULL;
  }
  node->type = type;
  node->value = value;
  return ast_push(ast, node);
}

Ast* ast_push(Ast* ast, Ast* node) {
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
  assert("something went very wrong" && ast->type < MAX_AST_TYPE);
  fprintf(fp, "<%d, %s>: `%.*s`\n", ast->value.type, ast_type_str[ast->type], ast->value.length, ast->value.buffer);
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
