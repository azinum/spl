/* spl.c - simple programming language */

#include <stdio.h>  // puts, printf
#include <stdint.h>

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
  char* index;  /* null terminated input string */
  i32 line;
  i32 column;
} Lexer;

static void lexer_init(Lexer* l, char* source);
static void next(Lexer* l);
static Token lexer_next(Lexer* l);
static Token lexer_peek(Lexer* l);

i32 main(i32 argc, char** argv) {
  Lexer lexer;
  Lexer* l = &lexer;
  lexer_init(l, "a = 1\r\nb = 1");
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
  l->index = source;
  l->line = 1;
  l->column = 1;
}

void next(Lexer* l) {
  l->token.buffer = l->index;
  l->token.length = 1;
  l->token.line = l->line;
  l->token.column = l->column;
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
      case '1': {
        l->token.type = T_NUMBER;
        goto done;
      }
      case '\n': {
        l->line++;
        l->column = 1;
        break;
      }
      case ' ': case '\t': case '\f': case '\v': {
        break;
      }
      case '\0': {
        l->token.type = T_EOF;
        goto done;
      }
      default: {
        break;
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
