/* spl.c - simple programming language */

#include <stdio.h>  /* puts, printf */
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

typedef enum Token_type {
  T_IDENTIFIER,
  T_NUMBER,
  T_ASSIGN,

  T_EOF,
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

static void lexer_init(Lexer* l, char* source);
static void lexer_error(Lexer* l, const char* fmt, ...);
static void next(Lexer* l);
inline i32 compare(Token a, const char* b);
inline Token lexer_read_symbol(Lexer* l);
inline Token lexer_read_number(Lexer* l);
inline i32 is_digit(char ch);
inline i32 is_alpha(char ch);
static Token lexer_next(Lexer* l);
static Token lexer_peek(Lexer* l);

static void error_printline(FILE* fp, char* source, char* index);

i32 main(i32 argc, char** argv) {
  (void)lexer_peek; // @Unused

  Lexer lexer;
  Lexer* l = &lexer;
  lexer_init(l, "hello 12\r\nb = 1");
  for (;;) {
    Token t = lexer_next(l);
    switch (t.type) {
      case T_EOF: {
        goto done;
      }
      default: {
        printf("%d:%d: `%.*s`\n", t.line, t.column, t.length, t.buffer);
        break;
      }
    }
  }
done:
  return 0;
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

#define MAX_ERR_SIZE 512
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

i32 compare(Token a, const char* b) {
  const char* index = b;
  for (int i = 0; i < a.length; ++i, ++index) {
    if (a.buffer[i] != *index) {
      return 0;
    }
  }
  return (*index == 0);
}

Token lexer_read_symbol(Lexer* l) {
  while (is_alpha(*l->index) || is_digit(*l->index) || *l->index == '_') {
    l->index++;
    l->column++;
  }
  l->token.length = l->index - l->token.buffer;
  if (compare(l->token, "nop")) {

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

void error_printline(FILE* fp, char* source, char* index) {
  assert(source);
  i32 offset = index - source;
  i32 start_offset = 0;
  i32 end_offset = 0;

  // Scan to the beginning of the line
  char* at = index;
  while (start_offset < offset) {
    char ch = *(at - start_offset);
    if (ch == '\n' || ch == '\r') {
      break;
    }
    start_offset++;
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
