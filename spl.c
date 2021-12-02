/* spl.c - simple programming language */

#include <stdio.h>  /* puts, printf */
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
typedef struct Ir {
  Ir_code i;
} Ir;

/* compile state */
typedef struct Compile {
  i32 status;
} Compile;

#define MAX_INS (1024 * 10)

/*
  input -> lexer -> parser -> ast? -> intermidiate representation -> interpreter/code generator
*/
typedef struct Parser {
  Lexer l;
  Ir ins[MAX_INS];
  u32 ins_count;
} Parser;

static i32 parser_init(Parser* p, char* source);
static void parser_error(Parser* p, const char* fmt, ...);
static i32 parse(Parser* p);
static i32 parse_statements(Parser* p);
static i32 parse_statement(Parser* p);
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

static void error_printline(FILE* fp, char* source, char* index);

i32 main(i32 argc, char** argv) {
  const char* source =
    "hello; 1234;\n"
    "b;;;;;; 1;\n"
  ;
  Parser p;
  if (parser_init(&p, (char*)source) == NoError) {
    if (parse(&p) == NoError) {
      printf("Parsing complete\n");
    }
  }
  return 0;
}

i32 parser_init(Parser* p, char* source) {
  lexer_init(&p->l, source);
  p->ins_count = 0;
  return NoError;
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
  error_printline(fp, l->source, l->index);
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
        result = parse_statement(p);
        if (result != NoError) {
          goto done;
        }
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
i32 parse_statement(Parser* p) {
  i32 result = NoError;

  Token t = lexer_peek(&p->l);
  switch (t.type) {
    case T_IDENTIFIER: {
      t = lexer_next(&p->l);
      if (!expect(t, T_SEMICOLON)) {
        parser_error(p, "expected `;` semicolon, but got `%.*s`\n", t.length, t.buffer);
        return Error;
      }
      break;
    }
    case T_NUMBER: {
      t = lexer_next(&p->l);
      if (!expect(t, T_SEMICOLON)) {
        parser_error(p, "expected `;` semicolon, but got `%.*s`\n", t.length, t.buffer);
        return Error;
      }
      break;
    }
    default: {
      parser_error(p, "expected identifier or number, but got `%.*s`\n", t.length, t.buffer);
      return Error;
    }
  }

  return result;
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
  error_printline(fp, l->source, l->index);
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
        // Error
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

// FIXME: The arrow points to the end of the offending token, however it should point to the beginning.
void error_printline(FILE* fp, char* source, char* index) {
  assert(source);
  i32 offset = index - source;
  i32 start_offset = 0;
  i32 end_offset = 0;

  // Scan to the beginning of the line
  char* at = index;
  while (start_offset < offset) {
    start_offset++;
    char ch = *(at - start_offset);
    if (ch == '\n' || ch == '\r') {
      break;
    }
  }
  // And then we scan to the end of the line
  do {
    char ch = *(at + end_offset);
    if (ch == '\n' || ch == '\r' || ch == '\0') {
      break;
    }
    end_offset++;
  } while (1);
  fprintf(fp, "%.*s\n", end_offset + start_offset, at - start_offset);
  for (i32 i = 0; i < start_offset - 1; ++i) {
    fprintf(fp, "-");
  }
  fprintf(fp, "^\n");
}
