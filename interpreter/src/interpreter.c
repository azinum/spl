// interpreter.c

#define COMMON_IMPLEMENTATION
#include "common.h"

#include <fcntl.h>
#include <time.h>
#include <sys/syscall.h>

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

#ifdef SLOW_INTERPRET
  #define THINK(ms) { \
    static struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = ms * 1000000, }; \
    nanosleep(&ts, NULL); \
  }
#else
  #define THINK(ms)
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
  I_JMP, // <label, positive_offset, negative_offset>
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

void printbits(size_t v);
void tabs(size_t count);
Compile* compile_state_new(const char* path);
void prepare_interpreter(Compile* c, size_t* entry_point);
void interpret(Compile* c, size_t ip, size_t depth);
void compile_state_free(Compile* c);

i32 main(void) {
  Compile* c = compile_state_new("test.spl.bin");
  if (c) {
    size_t ip = 0;
    prepare_interpreter(c, &ip);
    interpret(c, ip, 0);
    compile_state_free(c);
  }
  return EXIT_SUCCESS;
}

void printbits(size_t v) {
  for (size_t i = (size_t)sizeof(v) - 1;;) {
    for (size_t bit = 8; bit > 0; --bit) {
      char byte = *((char*)&v + i);
      printf("%d", EXTRACTBIT(bit, byte) != 0);
    }
    printf(" ");
    if (i == 0) {
      break;
    }
    i -= 1;
  }
  printf("\n");
}

void tabs(size_t count) {
  const char* tab = "  ";
  (void)tab;
  for (size_t i = 0; i < count; ++i) {
    verbose_printf("%s", tab);
  }
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

void interpret(Compile* c, size_t ip, size_t depth) {
  (void)ir_code_str;
  #define STACK_SIZE (Kb(8))
  #define MAX_STACK (STACK_SIZE / sizeof(size_t))
  size_t stack[MAX_STACK] = {0};
  size_t sp = 0; // stack pointer
  size_t bsp = 0; // base stack pointer

  #define STACK_PUSH(V) (stack[sp++] = (size_t)V)
  #define STACK_POP(N) (sp -= N, stack[sp])
  #define STACK_TOP(OFFSET) (stack[sp + OFFSET - 1])

  #define STACK_PRINT() do {                                          \
    for (size_t stack_index = 0; stack_index < sp; ++stack_index) {   \
      verbose_printf("%7zu: %zu", stack_index, stack[stack_index]);   \
      if (stack_index+1 == bsp) {                                     \
        verbose_printf(" <- bsp");                                    \
      }                                                               \
      verbose_printf("\n");                                           \
    }                                                                 \
  } while (0)

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
    tabs(depth);
    verbose_printf("%4zu: %s: ", ip, ir_code_str[op->i]);
    THINK(100);
    switch (op->i) {
      case I_NOP: {
        verbose_printf("\n");
        break;
      }
      case I_POP: {
        *(REG_VALUE_FROM_TYPE(RAX, 64)) = STACK_POP(1);
        verbose_printf("\n");
        break;
      }
      // <size, local_id, x>
      case I_MOVE_LOCAL: {
        const size_t size = op->dest;
        const size_t local_id = op->src0;
        const size_t offset = (bsp - 1) + (local_id / sizeof(size_t));
        size_t mask = ~0;
        mask >>= 64 - (size * 8);
        size_t top = STACK_TOP(0);
        (void)top;
        size_t value = STACK_POP(1);
        stack[offset] = value & mask;
        verbose_printf("value = %zu, size = %zu, local_id = %zu, offset = %zu, top = %zu\n", value, size, local_id, offset, top);
        break;
      }
      case I_STORE64: {
        size_t b = STACK_POP(1);
        size_t* a = (size_t*)STACK_POP(1);
        *a = b;
        verbose_printf("\n");
        break;
      }
      case I_STORE32: {
        size_t b = STACK_POP(1);
        size_t* a = (size_t*)STACK_POP(1);
        *(u32*)a = (u32)b;
        verbose_printf("\n");
        break;
      }
      case I_STORE16: {
        size_t b = STACK_POP(1);
        size_t* a = (size_t*)STACK_POP(1);
        *(u16*)a = (u16)b;
        verbose_printf("\n");
        break;
      }
      case I_STORE8: {
        size_t b = STACK_POP(1);
        size_t* a = (size_t*)STACK_POP(1);
        *(u8*)a = (u8)b;
        verbose_printf("\n");
        break;
      }
      case I_LOAD64: {
        size_t* value = (size_t*)STACK_POP(1);
        STACK_PUSH(*value);
        verbose_printf("\n");
        break;
      }
      case I_LOAD32: {
        size_t* value = (size_t*)STACK_POP(1);
        STACK_PUSH(*(u32*)value);
        verbose_printf("\n");
        break;
      }
      case I_LOAD16: {
        size_t* value = (size_t*)STACK_POP(1);
        STACK_PUSH(*(u16*)value);
        verbose_printf("\n");
        break;
      }
      case I_LOAD8: {
        size_t* value = (size_t*)STACK_POP(1);
        STACK_PUSH(*(u8*)value);
        verbose_printf("\n");
        break;
      }
      // <x, id, x>
      case I_PUSH_ADDR_OF: {
        const size_t id = op->src0;
        Symbol* symbol = &c->symbols[id];
        switch (symbol->sym_type) {
          case SYM_FUNC: {
            Function func = symbol->value.func;
            const size_t addr = (size_t)(size_t*)&c->ins[func.ir_address];
            STACK_PUSH(addr);
            break;
          }
          case SYM_GLOBAL_VAR: {
            const size_t addr = (size_t)(size_t*)&c->imm[symbol->imm];
            STACK_PUSH(addr);
            break;
          }
          default: {
            ASSERT(!"can not take the address of this kind of symbol");
            break;
          }
        }
        verbose_printf("\n");
        break;
      }
      // <x, local_id, x>
      case I_PUSH_LOCAL_ADDR_OF: {
        size_t local_id = op->src0;
        const size_t offset = (bsp - 1) + (local_id / sizeof(size_t));
        size_t* addr = &stack[offset];
        STACK_PUSH((size_t)addr);
        verbose_printf("\n");
        break;
      }
      // <type, id, size>
      case I_PUSH: {
        const size_t type = op->dest;
        const size_t id   = op->src0;
        if (type == TypeCString) {
          // TODO(lucas): working with strings does not work fully yet, put cstrings at another location and format them properly
          const size_t imm = c->cstrings[id] + sizeof(size_t);
          size_t* addr = (size_t*)&c->imm[imm];
          STACK_PUSH((size_t)addr);
        }
        else if (type == TypeFunc) {
          Symbol* symbol = &c->symbols[id];
          Function func = symbol->value.func;
          size_t* addr = (size_t*)&c->ins[func.ir_address];
          verbose_printf("ir_address = %zu, %zu", func.ir_address, *addr);
          STACK_PUSH((size_t)addr);
        }
        else {
          const size_t size = op->src1;
          const Symbol* symbol = &c->symbols[id];
          const size_t imm = symbol->imm;
          const size_t* value = (size_t*)&c->imm[imm];
          size_t mask = ~0;
          mask >>= 64 - (size * 8);
          STACK_PUSH(*value & mask);
        }
        verbose_printf("\n");
        break;
      }
      // <size, local_id, x>
      case I_PUSH_LOCAL: {
        const size_t size = op->dest;
        const size_t local_id = op->src0;
        const size_t offset = (bsp - 1) + (local_id / sizeof(size_t));
        const size_t value = stack[offset];
        size_t mask = ~0;
        mask >>= 64 - (size * 8);
        STACK_PUSH(value & mask);
        size_t top = STACK_TOP(0);
        (void)top;
        verbose_printf("value = %zu, size = %zu, local_id = %zu, offset = %zu, top = %zu\n", value, size, local_id, offset, top);
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
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a || b);
        verbose_printf("\n");
        break;
      }
      case I_XOR: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a ^ b);
        verbose_printf("\n");
        break;
      }
      case I_EQ: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a == b);
        verbose_printf("\n");
        break;
      }
      case I_NEQ: {
        size_t b = STACK_POP(1);
        size_t a = STACK_POP(1);
        STACK_PUSH(a != b);
        verbose_printf("\n");
        break;
      }
      // <x, frame_size, x>
      case I_RET: {
        const size_t frame_size = op->src0;
        const size_t frame_count = (frame_size / sizeof(size_t));
        Register* r = REG_FROM_TYPE(RAX);
        r->v64 = STACK_POP(1);
        sp -= frame_count;
        verbose_printf("frame_size = %zu, frame_count = %zu, rax = %zu\n", frame_size, frame_count, r->v64);
        return;
      }
      // <x, frame_size, x>
      case I_NORET: {
        const size_t frame_size = op->src0;
        const size_t frame_count = (frame_size / sizeof(size_t));
        sp -= frame_count;
        verbose_printf("frame_size = %zu, frame_count = %zu\n", frame_size, frame_count);
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
        const size_t label = op->dest;
        const size_t argc = op->src0;
        const size_t has_return_value = op->src1 != NONE;
        Symbol* symbol = &c->symbols[label];
        Function func = symbol->value.func;
        size_t ip_jump = func.ir_address;
        for (size_t i = 0; i < argc; ++i) {
          Register* r = REG_FROM_TYPE(func_call_regs[i]);
          r->v64 = STACK_POP(1);
          verbose_printf("%s = %zu, ", regs_str[func_call_regs[i]], r->v64);
        }
        verbose_printf("%s (addr: %zu)\n", symbol->name, ip_jump);
        interpret(c, ip_jump, depth + 1);
        if (has_return_value) {
          STACK_PUSH(*REG_VALUE_FROM_TYPE(RAX, 64));
        }
        break;
      }
      // <x, argc, rtype>
      case I_ADDR_CALL: {
        const size_t argc = op->src0;
        const size_t has_return_value = op->src1 != NONE;
        const size_t* addr = (size_t*)STACK_POP(1);
        const size_t ip_jump = ((size_t)addr - (size_t)&c->ins[0]) / sizeof(Op);
        for (size_t i = 0; i < argc; ++i) {
          Register* r = REG_FROM_TYPE(func_call_regs[i]);
          r->v64 = STACK_POP(1);
          verbose_printf("%s = %zu, ", regs_str[func_call_regs[i]], r->v64);
        }
        verbose_printf("ip: %zu, addr: %zu\n", ip, *addr);
        interpret(c, ip_jump, depth + 1);
        if (has_return_value) {
          STACK_PUSH(*REG_VALUE_FROM_TYPE(RAX, 64));
        }
        break;
      }

      // <label, positive_offset, negative_offset>
      case I_JMP: {
        size_t positive_offset = op->src0;
        size_t negative_offset = op->src1;
        if (positive_offset != NONE) {
          ip += positive_offset - 1;
          verbose_printf("+%zu, %zu -> %zu\n", positive_offset, ip - positive_offset + 1, ip + 1);
          break;
        }
        ip -= negative_offset + 1;
        verbose_printf("-%zu, %zu -> %zu\n", negative_offset, ip + negative_offset + 1, ip + 1);
        break;
      }
      // <label, offset, x>
      case I_JZ: {
        size_t offset = op->src0;
        size_t value = STACK_POP(1);
        if (value == 0) {
          ip += offset - 1;
        }
        verbose_printf("%zu == 0, offset = %zu, %zu -> %zu\n", value, offset, ip - (size_t)(value == 0) * (offset - 1), ip + (size_t)(value == 0));
        break;
      }
      // <x, argc, frame_size>
      case I_BEGIN_FUNC: {
        const size_t argc = op->src0;
        const size_t frame_size = op->src1;
        const size_t frame_count = frame_size / sizeof(size_t);
        bsp = sp;
        for (size_t i = 0; i < argc; ++i) {
          const Register* r = REG_FROM_TYPE(func_call_regs[i]);
          STACK_PUSH(r->v64);
          verbose_printf("mov %s, %zu; push %s; ", regs_str[func_call_regs[i]], r->v64, regs_str[func_call_regs[i]]);
        }
        sp = bsp + frame_count;
        verbose_printf("\n");
        break;
      }
      case I_LOOP_LABEL: {
        verbose_printf("\n");
        break;
      }
      case I_SYSCALL0: {
        const size_t code = STACK_POP(1);
        *REG_VALUE_FROM_TYPE(RAX, 64) = code;
        size_t rvalue = syscall(code);
        STACK_PUSH(rvalue);
        verbose_printf("syscall(%zu)\n", code);
        break;
      }
      case I_SYSCALL1: {
        const size_t code = STACK_POP(1);
        *REG_VALUE_FROM_TYPE(RAX, 64) = code;
        for (size_t i = 0; i < 1; ++i) {
          Register* r = REG_FROM_TYPE(func_call_regs[i]);
          r->v64 = STACK_POP(1);
        }
        size_t rvalue = syscall(
          code,
          *REG_VALUE_FROM_TYPE(func_call_regs[0], 64)
        );
        STACK_PUSH(rvalue);
        verbose_printf("syscall(%zu)\n", code);
        break;
      }
      case I_SYSCALL2: {
        const size_t code = STACK_POP(1);
        *REG_VALUE_FROM_TYPE(RAX, 64) = code;
        for (size_t i = 0; i < 2; ++i) {
          Register* r = REG_FROM_TYPE(func_call_regs[i]);
          r->v64 = STACK_POP(1);
        }
        size_t rvalue = syscall(
          code,
          *REG_VALUE_FROM_TYPE(func_call_regs[0], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[1], 64)
        );
        STACK_PUSH(rvalue);
        verbose_printf("syscall(%zu)\n", code);
        break;
      }
      case I_SYSCALL3: {
        const size_t code = STACK_POP(1);
        *REG_VALUE_FROM_TYPE(RAX, 64) = code;
        for (size_t i = 0; i < 3; ++i) {
          Register* r = REG_FROM_TYPE(func_call_regs[i]);
          r->v64 = STACK_POP(1);
        }
        size_t rvalue = syscall(
          code,
          *REG_VALUE_FROM_TYPE(func_call_regs[0], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[1], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[2], 64)
        );
        STACK_PUSH(rvalue);
        verbose_printf("syscall(%zu)\n", code);
        break;
      }
      case I_SYSCALL4: {
        const size_t code = STACK_POP(1);
        *REG_VALUE_FROM_TYPE(RAX, 64) = code;
        for (size_t i = 0; i < 4; ++i) {
          Register* r = REG_FROM_TYPE(func_call_regs[i]);
          r->v64 = STACK_POP(1);
        }
        size_t rvalue =syscall(
          code,
          *REG_VALUE_FROM_TYPE(func_call_regs[0], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[1], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[2], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[3], 64)
        );
        STACK_PUSH(rvalue);
        verbose_printf("syscall(%zu)\n", code);
        break;
      }
      case I_SYSCALL5: {
        const size_t code = STACK_POP(1);
        *REG_VALUE_FROM_TYPE(RAX, 64) = code;
        for (size_t i = 0; i < 5; ++i) {
          Register* r = REG_FROM_TYPE(func_call_regs[i]);
          r->v64 = STACK_POP(1);
        }
        size_t rvalue = syscall(
          code,
          *REG_VALUE_FROM_TYPE(func_call_regs[0], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[1], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[2], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[3], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[4], 64)
        );
        STACK_PUSH(rvalue);
        verbose_printf("syscall(%zu)\n", code);
        break;
      }
      case I_SYSCALL6: {
        const size_t code = STACK_POP(1);
        *REG_VALUE_FROM_TYPE(RAX, 64) = code;
        for (size_t i = 0; i < 6; ++i) {
          Register* r = REG_FROM_TYPE(func_call_regs[i]);
          r->v64 = STACK_POP(1);
        }
        size_t rvalue = syscall(
          code,
          *REG_VALUE_FROM_TYPE(func_call_regs[0], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[1], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[2], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[3], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[4], 64),
          *REG_VALUE_FROM_TYPE(func_call_regs[5], 64)
        );
        STACK_PUSH(rvalue);
        verbose_printf("syscall(%zu)\n", code);
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
