// vm.c
// virtual machine that executes spl ir code

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

#define Error (-1)
#define NoError (0)

#define ARR_SIZE(ARR) (sizeof(ARR) / sizeof(ARR[0]))

typedef enum Ir_code {
  I_NOP = 0,
  I_POP,
  I_MOVE,
  I_STORE64,
  I_STORE32,
  I_STORE16,
  I_STORE8,
  I_LOAD64,
  I_LOAD32,
  I_LOAD16,
  I_LOAD8,
  I_PUSH_ADDR_OF, // <type, id, x>
  I_PUSH_LOCAL_ADDR_OF, // <type, id, x>
  I_PUSH, // <type, id, x> -- push argument value
  I_PUSH_LOCAL, // <type, id, imm>
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
  I_RET, // <x, argc, x>
  I_NORET, // <x, argc, x>
  I_PRINT,
  I_LABEL,
  I_CALL, // <label, argc, rtype>
  I_ADDR_CALL, // <rax, argc, rtype>
  I_JMP,
  I_JZ,
  I_BEGIN_FUNC, // <x, argc, x>
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

typedef enum Compile_type {
  TypeNone = 0,
  TypeAny,
  TypeUnsigned64,
  TypePointer,
  TypeCString,
  TypeFunc,
  TypeSyscallFunc,

  MAX_COMPILE_TYPE,
} Compile_type;

static u64 compile_type_size[] = {
  0,
  sizeof(u64),
  sizeof(u64),
  sizeof(u64),
  sizeof(u64),
  sizeof(u64),
  sizeof(u64),
};

typedef struct Op {
  Ir_code i;
  i32 dest;
  i32 src0;
  i32 src1;
} Op;

typedef enum Func_call_regs {
  RDI = 0,
  RSI,
  RDX,
  RCX,
  R8,
  R9,

  MAX_FUNC_CALL_REGS,
} Func_call_regs;

typedef struct Vm {
  u8* ir;
  u32 ir_size;

  u64 func_call_regs_x86_64[MAX_FUNC_CALL_REGS];

  u8* imm;
  u32 imm_size;

  u8* data;
  u32 data_size;

  u8* bss;
  u32 bss_size;

  Op* ins;
  u32 ins_count;
} Vm;

const u8 ir_magic[] = {'S', 'P', 'L', '0'};

typedef struct Buffer {
  u8* data;
  u32 size;
} Buffer;

static i32 vm_init(Vm* vm);
static i32 vm_load_ir(Vm* vm, Buffer* b);
static i32 vm_exec(Vm* vm);
static i32 read_entire_file(char* path, Buffer* b);
static void buffer_free(Buffer* b);

i32 main(i32 argc, char** argv) {
  if (argc < 2) {
    printf("Usage; %s <ir filename>\n", *argv);
    return EXIT_SUCCESS;
  }
  ++argv;
  char* filename = *argv;
  Buffer b;
  Vm vm;
  if (read_entire_file(filename, &b) == NoError) {
    vm_init(&vm);
    if (vm_load_ir(&vm, &b) == NoError) {
      vm_exec(&vm);
    }
    buffer_free(&b);
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}

i32 vm_init(Vm* vm) {
  vm->ir = NULL;
  vm->ir_size = 0;

  memset(&vm->func_call_regs_x86_64[0], 0, sizeof(u64) * MAX_FUNC_CALL_REGS);

  vm->imm = NULL;
  vm->imm_size = 0;

  vm->data = NULL;
  vm->data_size = 0;

  vm->bss = NULL;
  vm->bss_size = 0;

  vm->ins = NULL;
  vm->ins_count = 0;
  return NoError;
}

i32 vm_load_ir(Vm* vm, Buffer* b) {
  vm->ir = b->data;
  vm->ir_size = b->size;
  u8* it = &vm->ir[0];
  u8* end = &vm->ir[b->size];
#define r(_data, _size) memcpy(_data, it, _size); it += _size; assert(it <= end);
  u8 magic[4] = {0};
  r(magic, ARR_SIZE(magic));
  if (strncmp((void*)magic, (void*)ir_magic, ARR_SIZE(ir_magic)) != 0) {
    fprintf(stderr, "error: bad ir magic `%.*s`, should be `%.*s`\n", (i32)ARR_SIZE(ir_magic), magic, (i32)ARR_SIZE(ir_magic), ir_magic);
    return Error;
  }

  r(&vm->imm_size, sizeof(vm->imm_size));
  r(&vm->data_size, sizeof(vm->data_size));
  r(&vm->bss_size, sizeof(vm->bss_size));
  r(&vm->ins_count, sizeof(vm->ins_count));

  vm->imm = &it[0];
  it += vm->imm_size;

  vm->data = &it[0];
  it += vm->data_size;

  vm->bss = &it[0];
  it += vm->bss_size;

  vm->ins = (Op*)&it[0];
  it += vm->ins_count;

  if (it > end) {
    fprintf(stderr, "error: corrupt ir file\n");
    return Error;
  }

#if 0
  printf(
    "ir_size   = %d\n"
    "imm_size  = %d\n"
    "data_size = %d\n"
    "bss_size  = %d\n"
    "ins_count = %d\n"
    ,
    vm->ir_size,
    vm->imm_size,
    vm->data_size,
    vm->bss_size,
    vm->ins_count
  );
#endif
  return NoError;
}

i32 vm_exec(Vm* vm) {
  for (u32 i = 0; i < vm->ins_count; ++i) {
    Op* op = &vm->ins[i];
    switch (op->i) {
      case I_NOP: {
        break;
      }
      case I_POP: {
        break;
      }
      case I_MOVE: {
        break;
      }
      case I_STORE64: {
        break;
      }
      case I_STORE32: {
        break;
      }
      case I_STORE16: {
        break;
      }
      case I_STORE8: {
        break;
      }
      case I_LOAD64: {
        break;
      }
      case I_LOAD32: {
        break;
      }
      case I_LOAD16: {
        break;
      }
      case I_LOAD8: {
        break;
      }
      case I_PUSH_ADDR_OF: {
        switch (op->dest) {
          case TypeAny: // TODO(lucas): temp
          case TypeUnsigned64: {
            break;
          }
          default: {
            assert("I_PUSH_ADDR_OF: type not implemented yet" && 0);
          }
        }
        break;
      }
      case I_PUSH_LOCAL_ADDR_OF: {
        break;
      }
      case I_PUSH: {
        switch (op->dest) {
          case TypeAny: // TODO(lucas): temp
          case TypeUnsigned64: {
            break;
          }
          default: {
            assert("I_PUSH: type not implemented yet" && 0);
            break;
          }
        }
        break;
      }
      case I_PUSH_LOCAL: {
        if (op->src0 >= 0) {
          switch (op->dest) {
            case TypeUnsigned64: {
              break;
            }
            case TypeCString: {
              break;
            }
            case TypeFunc: {
              break;
            }
            default: {
              assert("I_PUSH_LOCAL: type not implemented yet" && 0);
              break;
            }
          }
        }
        else if (op->src1 >= 0) { // immediate value
          switch (op->dest) {
            case TypeUnsigned64: {
              break;
            }
            default: {
              assert("I_PUSH_LOCAL: imm type not implemented yet" && 0);
              break;
            }
          }
        }
        break;
      }
      case I_ADD: {
        break;
      }
      case I_SUB: {
        break;
      }
      case I_MUL: {
        break;
      }
      case I_DIV: {
        break;
      }
      case I_LSHIFT: {
        assert("I_LSHIFT instruction not implemented yet" && 0);
        break;
      }
      case I_RSHIFT: {
        assert("I_RSHIFT instruction not implemented yet" && 0);
        break;
      }
      case I_DIVMOD: {
        break;
      }
      case I_LT: {
        break;
      }
      case I_GT: {
        break;
      }
      case I_AND: {
        break;
      }
      case I_LOGICAL_NOT: {
        break;
      }
      case I_OR: {
        break;
      }
      case I_EQ: {
        break;
      }
      case I_NEQ: {
        break;
      }
      case I_RET: {
        i32 argc = op->src0;
        if (argc > 0) {
        }
        break;
      }
      case I_NORET: {
        i32 argc = op->src0;
        if (argc > 0) {
        }
        break;
      }
      case I_PRINT: {
        break;
      }
      case I_LABEL: {
        break;
      }
      case I_CALL: {
        assert(op->dest >= 0);
        i32 argc = op->src0;
        for (i32 arg = 0; arg < argc; ++arg) {
        }
        if (op->src1 >= 0) {
        }
        break;
      }
      case I_ADDR_CALL: {
        assert(op->dest >= 0);
        i32 argc = op->src0;
        for (i32 arg = 0; arg < argc; ++arg) {
        }
        if (op->src1 >= 0) {
        }
        break;
      }
      case I_JMP: {
        break;
      }
      case I_JZ: { // jump if zero
        break;
      }
      case I_BEGIN_FUNC: {
        break;
      }
      case I_LOOP_LABEL: {
        break;
      }
      case I_SYSCALL0: {
        break;
      }
      case I_SYSCALL1: {
        break;
      }
      case I_SYSCALL2: {
        break;
      }
      case I_SYSCALL3: {
        break;
      }
      case I_SYSCALL4: {
        break;
      }
      case I_SYSCALL5: {
        break;
      }
      case I_SYSCALL6: {
        break;
      }
      default: {
        fprintf(stderr, "instruction `%d` not implemented\n", op->i);
        assert(0);
        break;
      }
    }
  }
  return NoError;
}

i32 read_entire_file(char* path, Buffer* b) {
  FILE* fp = fopen(path, "rb");
  if (!fp) {
    fprintf(stderr, "[error]: failed to open file `%s`\n", path);
    return Error;
  }
  fseek(fp, 0, SEEK_END);
  b->size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  b->data = (u8*)malloc(b->size);
  if (!b->data) {
    fprintf(stderr, "[error]: failed to allocate memory for file `%s`\n", path);
    fclose(fp);
    b->size = 0;
    return Error;
  }
  i32 read_size = fread(&b->data[0], 1, b->size, fp);
  (void)read_size;  // ignore warning
  fclose(fp);
  return NoError;
}

void buffer_free(Buffer* b) {
  free(b->data);
  b->size = 0;
  
}
