// interpreter.c

#define COMMON_IMPLEMENTATION
#include "common.h"

#include <fcntl.h>

#define MAX_SYMBOL (4096)
#define MAX_SYMBOL_PER_BLOCK (4096)
#define MAX_STATIC_DATA (128 * 1024)
#define MAX_CSTRING (2048)
#define MAX_SYSCALL_FUNCTION (7)
#define MAX_BLOCK (256)
#define MAX_TYPE_STACK (256)
#define MAX_INS (128 * 1024)
#define MAX_TYPE_CONTRACT_STACK (16)
#define MAX_FUNC_ARGC (6)
#define MAX_NAME_SIZE (64)
#define MAX_COMPILE_TYPE (1024)
#define MAX_NUM_STRUCT_FIELDS (64)
#define MAX_TYPE_CONTEXT (512)
#define MAX_FIELD_ACCESS_LEVEL (8)

#define NONE UINT64_MAX

#ifdef VERBOSE
  #define verbose_printf(...) printf(__VA_ARGS__)
#else
  #define verbose_printf(...)
#endif

#if 0
  #define PACK_AND_ALIGN __attribute__((packed, aligned(sizeof(size_t))))
#else
  #define PACK_AND_ALIGN
#endif

typedef enum Primitive_type {
  TypeNone,
  TypeAny,
  TypePtr,
  TypeUnsigned64,
  TypeUnsigned32,
  TypeUnsigned16,
  TypeUnsigned8,
  TypeCString,
  TypeFunc,
  TypeSyscallFunc,
  TypeStruct,
  TypeUnion,

  MAX_PRIMITIVE_TYPE
} Primitive_type;

typedef enum Symbol_type {
  SYM_FUNC_ARG,
  SYM_FUNC,
  SYM_LOCAL_VAR,
  SYM_GLOBAL_VAR,

  MAX_SYM_TYPE,
} Symbol_type;

typedef struct Type {
  size_t base;
  size_t sub_id;
  size_t hash;
  size_t count;
  size_t size;
} PACK_AND_ALIGN Type;

typedef struct Struct_field {
  u8 name[MAX_NAME_SIZE];
  Type type;
  size_t offset;
} PACK_AND_ALIGN Struct_field;

typedef struct Struct_type_info {
  u8 name[MAX_NAME_SIZE];
  Struct_field fields[MAX_NUM_STRUCT_FIELDS];
  size_t count;
} PACK_AND_ALIGN Struct_type_info;

typedef struct Func_signature {
  size_t argc;
  Type args[MAX_FUNC_ARGC + 1];
  Type rtype;
} PACK_AND_ALIGN Func_signature;

typedef struct Type_context {
  Type type;
  size_t offset;
  size_t symbol_id;
} PACK_AND_ALIGN Type_context;

typedef struct Token {
  char* buffer;
  size_t length;
  size_t type;
  size_t v;
  char* filename;
  char* source;
  size_t line;
  size_t column;
} PACK_AND_ALIGN Token;

typedef struct Function {
  size_t ir_address;
  size_t ir_body_size;
  size_t label;
  size_t argc;
  size_t locals_offset;
} PACK_AND_ALIGN Function;

typedef struct Value {
  Function func;
  size_t num;
  size_t konst;
} PACK_AND_ALIGN Value;

typedef enum Ir_code {
  I_NOP,
  I_POP,
  I_MOVE_LOCAL, // <size, local_id, x>
  I_STORE64,
  I_STORE32,
  I_STORE16,
  I_STORE8,
  I_LOAD64,
  I_LOAD32,
  I_LOAD16,
  I_LOAD8,
  I_PUSH_ADDR_OF, // <x, id, x>
  I_PUSH_LOCAL_ADDR_OF, // <x, local_id, x>
  I_PUSH, // <type, id, size>
  I_PUSH_LOCAL, // <size, local_id, x>
  I_PUSH_IMM, // <size, imm, x>
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
  I_NOT,
  I_OR,
  I_XOR,
  I_EQ,
  I_NEQ,
  I_RET, // <x, frame_size, x>
  I_NORET, // <x, frame_size, x>
  I_PRINT,
  I_LABEL,
  I_CALL, // <label, argc, rtype>
  I_ADDR_CALL, // <x, argc, rtype>
  I_JMP, // <label, positite_offset, negative_offset>
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

  MAX_IR_CODE
} Ir_code;

static const char* ir_code_str[MAX_IR_CODE] = {
  "I_NOP",
  "I_POP",
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
  "I_NOT",
  "I_OR",
  "I_XOR",
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

typedef struct Symbol {
  u8 name[MAX_NAME_SIZE];
  size_t imm;
  size_t size;
  size_t num_elements_init;
  size_t konst;
  size_t local_id;
  size_t sym_type;
  Type type;
  Token token;
  Value value;
  size_t ref_count;
} PACK_AND_ALIGN Symbol;

typedef struct Block {
  size_t symbols[MAX_SYMBOL_PER_BLOCK];
  size_t symbol_count;
  struct Block* parent;
} PACK_AND_ALIGN Block;

typedef struct Op {
  size_t i;
  size_t dest;
  size_t src0;
  size_t src1;
} Op;

typedef struct Compile {
  Op ins[MAX_INS];
  size_t ins_count;

  u8 imm[MAX_STATIC_DATA];
  size_t imm_index;

  Symbol symbols[MAX_SYMBOL];
  size_t symbol_count;

  size_t cstrings[MAX_CSTRING];
  size_t cstring_count;

  Block global;

  Block blocks[MAX_BLOCK];
  size_t block_count;
  size_t block_count_largest;

  size_t label_count;

  size_t status;
  size_t entry_point;

  Type ts[MAX_TYPE_STACK];
  size_t ts_count;

  Value vs[MAX_TYPE_STACK];
  size_t vs_count;

  Type types[MAX_COMPILE_TYPE];
  size_t type_count;

  Type type_contract_stack[MAX_TYPE_CONTRACT_STACK];
  size_t type_contract_count;

  Struct_type_info struct_type_info[MAX_COMPILE_TYPE];
  size_t struct_id;

  Func_signature func_signatures[MAX_COMPILE_TYPE];
  size_t func_sig_count;

  Type_context type_contexts[MAX_TYPE_CONTEXT];
  size_t type_context_count;

  size_t warning_count;
  size_t error_count;
} PACK_AND_ALIGN Compile;

Compile* compile_state_new(const char* path);
void prepare_interpreter(Compile* c, size_t* entry_point);
void interpret(Compile* c, size_t ip);
void compile_state_free(Compile* c);

i32 main(void) {
  Compile* c = compile_state_new("test.spl.bin");
  if (c) {
    size_t ip = 0;
    prepare_interpreter(c, &ip);
    interpret(c, ip);
    compile_state_free(c);
  }
  return EXIT_SUCCESS;
}

Compile* compile_state_new(const char* path) {
  i32 fd = open(path, O_RDONLY);
  if (fd < 0) {
    return NULL;
  }
  Compile* state = malloc(sizeof(Compile));
  i32 n = read(fd, state, sizeof(Compile));
  ASSERT(n == sizeof(Compile));
  close(fd);
  return state;
}

void prepare_interpreter(Compile* c, size_t* entry_point) {
  ASSERT(entry_point != NULL);
  for (size_t i = 0; i < c->symbol_count; ++i) {
    Symbol* symbol = &c->symbols[i];
    if (!strncmp((char*)symbol->name, "main", MAX_NAME_SIZE)) {
      Function main_func = symbol->value.func;
      *entry_point = main_func.ir_address;
      break;
    }
  }
}

void interpret(Compile* c, size_t ip) {
  (void)ir_code_str;
  #define MAX_STACK (Kb(8) / sizeof(size_t))
  size_t stack[MAX_STACK] = {0};
  size_t sp = 0; // stack pointer
  size_t bsp = 0; // base stack pointer

  #define STACK_PUSH(V) (stack[sp++] = (size_t)V)
  #define STACK_POP(N) (sp -= N, stack[sp])
  #define STACK_TOP(OFFSET) (stack[sp + OFFSET])

  // x86_64 style registers
  typedef union Register {
    size_t v64;
    u32 v32;
    u16 v16;
    u8 v8;
  } Register;

  #define REG_FROM_TYPE(REGISTER_TYPE) &regs[REGISTER_TYPE]
  #define REG_VALUE_FROM_TYPE(REGISTER_TYPE, BITS) REG_FROM_TYPE(REGISTER_TYPE).v##BITS

  typedef enum Register_type {
    RAX,
    RCX,
    RDX,
    RBX,
    RSP,
    RBP,
    RSI,
    RDI,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,

    MAX_REGISTER,
  } Register_type;

  static Register regs[MAX_REGISTER] = {0};

  Register_type func_call_regs[MAX_FUNC_ARGC] = {
    RDI,
    RSI,
    RDX,
    RCX,
    R8,
    R9
  };

  const char* regs_str[MAX_REGISTER] = {
    "RAX",
    "RCX",
    "RDX",
    "RBX",
    "RSP",
    "RBP",
    "RSI",
    "RDI",
    "R8",
    "R9",
    "R10",
    "R11",
    "R12",
    "R13",
    "R14",
    "R15",
  };
  (void)regs_str;

  for (; ip < c->ins_count; ++ip) {
    const Op* op = &c->ins[ip];
    verbose_printf("%4zu: %s: ", ip, ir_code_str[op->i]);
    switch (op->i) {
      case I_NOP: {
        verbose_printf("\n");
        break;
      }
      case I_POP: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_MOVE_LOCAL: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_STORE64: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_STORE32: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_STORE16: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_STORE8: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_LOAD64: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_LOAD32: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_LOAD16: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_LOAD8: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_PUSH_ADDR_OF: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_PUSH_LOCAL_ADDR_OF: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_PUSH: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_PUSH_LOCAL: {
        const size_t size = op->dest;
        const size_t local_offset = op->src0;
        const size_t offset_index = local_offset / sizeof(size_t);
        size_t value = stack[bsp + offset_index];
        switch (size) {
          case 1: {
            STACK_PUSH((size_t)((u8)value));
            break;
          }
          case 2: {
            STACK_PUSH((size_t)((u16)value));
            break;
          }
          case 4: {
            STACK_PUSH((size_t)((u32)value));
            break;
          }
          case 8: {
            STACK_PUSH(value);
            break;
          }
          default:
            ASSERT(!"invalid size");
            break;
        }
        verbose_printf("value = %zu, size = %zu, local_offset = %zu, offset_index = %zu\n", value, size, local_offset, offset_index);
        break;
      }
      // <size, imm, x>
      case I_PUSH_IMM: {
        const size_t size = op->dest;
        const size_t imm = op->src0;
        ASSERT(size == 8);
        size_t value = *(size_t*)&c->imm[imm];
        STACK_PUSH(value);
        verbose_printf("%zu\n", value);
        break;
      }
      case I_ADD: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a + b);
        verbose_printf("%zu + %zu = %zu\n", a, b, a + b);
        break;
      }
      case I_SUB: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a - b);
        verbose_printf("\n");
        break;
      }
      case I_MUL: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a * b);
        verbose_printf("\n");
        break;
      }
      case I_LSHIFT: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a << b);
        verbose_printf("\n");
        break;
      }
      case I_RSHIFT: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a >> b);
        verbose_printf("\n");
        break;
      }
      case I_DIV: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a / b);
        verbose_printf("\n");
        break;
      }
      case I_DIVMOD: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a % b);
        verbose_printf("\n");
        break;
      }
      case I_LT: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a < b);
        verbose_printf("\n");
        break;
      }
      case I_GT: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a > b);
        verbose_printf("\n");
        break;
      }
      case I_AND: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a && b);
        verbose_printf("\n");
        break;
      }
      case I_LOGICAL_NOT: {
        size_t a = STACK_POP(1);
        STACK_PUSH(!a);
        verbose_printf("\n");
        break;
      }
      case I_NOT: {
        size_t a = STACK_POP(1);
        STACK_PUSH(~a);
        verbose_printf("\n");
        break;
      }
      case I_OR: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_XOR: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_EQ: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_NEQ: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      // <x, frame_size, x>
      case I_RET: {
        size_t frame_size = op->src0;
        size_t argc = frame_size / 8;
        Register* r = REG_FROM_TYPE(RAX);
        r->v64 = STACK_POP(1);
        size_t _ = STACK_POP(argc);
        (void)_;
        verbose_printf("frame_size = %zu, argc = %zu, rax = %zu\n", frame_size, argc, r->v64);
        return;
      }
      // <x, frame_size, x>
      case I_NORET: {
        size_t frame_size = op->src0;
        size_t argc = frame_size / 8;
        size_t _ = STACK_POP(argc);
        (void)_;
        verbose_printf("frame_size = %zu, argc = %zu\n", frame_size, argc);
        return;
      }
      case I_PRINT: {
        size_t value = STACK_POP(1);
        (void)value;
        printf("%zu\n", value);
        break;
      }
      case I_LABEL: {
        verbose_printf("\n");
        break;
      }
      // <label, argc, rtype>
      case I_CALL: {
        size_t label = op->dest;
        size_t argc = op->src0;
        size_t has_return_value = op->src1 != NONE;
        Symbol* symbol = &c->symbols[label];
        Function func = symbol->value.func;
        size_t ip_jump = func.ir_address;
        for (size_t i = 0; i < argc; ++i) {
          Register* r = REG_FROM_TYPE(func_call_regs[i]);
          r->v64 = STACK_POP(1);
        }
        verbose_printf("%s (addr: %zu)\n", symbol->name, ip_jump);
        interpret(c, ip_jump);
        if (has_return_value) {
          STACK_PUSH(*REG_VALUE_FROM_TYPE(RAX, 64));
        }
        break;
      }
      case I_ADDR_CALL: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_JMP: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_JZ: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_BEGIN_FUNC: {
        size_t argc = op->src0;
        bsp = sp - 1;
        for (size_t i = 0; i < argc; ++i) {
          const Register* r = REG_FROM_TYPE(func_call_regs[i]);
          STACK_PUSH(r->v64);
          verbose_printf("mov %s, %zu; push %s; ", regs_str[func_call_regs[i]], r->v64, regs_str[func_call_regs[i]]);
        }
        verbose_printf("\n");
        break;
      }
      case I_LOOP_LABEL: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_SYSCALL0: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_SYSCALL1: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_SYSCALL2: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_SYSCALL3: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_SYSCALL4: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_SYSCALL5: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      case I_SYSCALL6: {
        NOT_IMPLEMENTED();
        verbose_printf("\n");
        break;
      }
      default: {
        ASSERT(!"unknown instruction");
        break;
      }
    }
  }
}

void compile_state_free(Compile* c) {
  free(c);
}
