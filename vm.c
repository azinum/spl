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
#define NUM_SYSCALL (7)

#define ARR_SIZE(ARR) (sizeof(ARR) / sizeof(ARR[0]))
#define HERE printf("%s:%d: HERE\n", __FUNCTION__, __LINE__)

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

typedef enum Register {
  RAX = 0,
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

  MAX_REGS,
} Register;

// maps to regs
typedef enum Func_call_regs {
  F0 = RDI,
  F1 = RSI,
  F2 = RDX,
  F3 = RCX,
  F4 = R8,
  F5 = R9,

  MAX_FUNC_CALL_REGS,
} Func_call_regs;

#define MAX_STACK 512

typedef struct Vm {
  u8* ir;
  u32 ir_size;

  u64 regs[MAX_REGS];

  u8* imm;
  u32 imm_size;

  u8* cstrings;
  u32 cstring_size;

  u8* data;
  u32 data_size;

  u8* bss;
  u32 bss_size;

  Op* ins;
  u32 ins_count;

  u64 stack[MAX_STACK];

  // TODO(lucas): create mappings between labels and addresses for not-too-slow-execution
} Vm;

const u8 ir_magic[] = {'S', 'P', 'L', '0'};

typedef struct Buffer {
  u8* data;
  u32 size;
} Buffer;

static i32 vm_init(Vm* vm);
static i32 vm_load_ir(Vm* vm, Buffer* b);
static i32 vm_exec(Vm* vm);
static u8* vm_id_to_address_from_bss(Vm* vm, i32 id);
static u8* vm_id_to_address_from_data(Vm* vm, i32 id);
static u8* vm_id_to_address_from_cstring_data(Vm* vm, i32 id);
static i32 read_entire_file(char* path, Buffer* b);
static void buffer_free(Buffer* b);
static u64 stack_push(Vm* vm, u32 v);
static u64 stack_pop(Vm* vm, u64* r);
static void stack_trace(Vm* vm);
static u64* reg(Vm* vm, Register r);

i32 main(i32 argc, char** argv) {
  (void)compile_type_size; // unused
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
      u64* rsp = reg(&vm, RSP);
      u32 stack_delta = MAX_STACK - *rsp;
      if (stack_delta > 1) {
        fprintf(stderr, "error: unhandled data on the stack\n");
      }
      (void)stack_trace;
    }
    buffer_free(&b);
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}

i32 vm_init(Vm* vm) {
  vm->ir = NULL;
  vm->ir_size = 0;

  memset(&vm->regs[0], 0, sizeof(u64) * MAX_REGS);

  vm->imm = NULL;
  vm->imm_size = 0;

  vm->cstrings = NULL;
  vm->cstring_size = 0;

  vm->data = NULL;
  vm->data_size = 0;

  vm->bss = NULL;
  vm->bss_size = 0;

  vm->ins = NULL;
  vm->ins_count = 0;

  u64* rsp = reg(vm, RSP);
  u64* rbp = reg(vm, RBP);
  *rbp = MAX_STACK;
  *rsp = *rbp;

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
  r(&vm->cstring_size, sizeof(vm->cstring_size));
  r(&vm->data_size, sizeof(vm->data_size));
  r(&vm->bss_size, sizeof(vm->bss_size));
  r(&vm->ins_count, sizeof(vm->ins_count));

  vm->imm = &it[0];
  it += vm->imm_size;

  vm->cstrings = &it[0];
  it += vm->cstring_size;

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
    "ir_size       = %d\n"
    "imm_size      = %d\n"
    "cstring_size  = %d\n"
    "data_size     = %d\n"
    "bss_size      = %d\n"
    "ins_count     = %d\n"
    ,
    vm->ir_size,
    vm->imm_size,
    vm->cstring_size,
    vm->data_size,
    vm->bss_size,
    vm->ins_count
  );
#endif
#undef r
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
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rax);
        break;
      }
      case I_MOVE: {
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rax);
        u8* address = vm_id_to_address_from_bss(vm, op->dest);
        if (!address) {
          address = vm_id_to_address_from_data(vm, op->dest);
        }
        assert(address != NULL);
        *(u64*)address = *rax;
        break;
      }
      case I_STORE64: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        u64* rax_ptr = (u64*)*rax;
        *rax_ptr = *rbx;
        break;
      }
      case I_STORE32: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        u64* rax_ptr = (u64*)*rax;
        *rax_ptr = *rbx & 0xffffffff;
        break;
      }
      case I_STORE16: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        u64* rax_ptr = (u64*)*rax;
        *rax_ptr = *rbx & 0xffff;
        break;
      }
      case I_STORE8: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        u64* rax_ptr = (u64*)*rax;
        *rax_ptr = *rbx & 0xff;
        break;
      }
      case I_LOAD64: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rax);
        *rbx = *(u64*)*rax;
        stack_push(vm, *rbx);
        break;
      }
      case I_LOAD32: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rax);
        *rbx = *(u64*)*rax & 0xffffffff;
        stack_push(vm, *rbx);
        break;
      }
      case I_LOAD16: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rax);
        *rbx = *(u64*)*rax & 0xffff;
        stack_push(vm, *rbx);
        break;
      }
      case I_LOAD8: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rax);
        *rbx = *(u64*)*rax & 0xff;
        stack_push(vm, *rbx);
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
        u8* address = vm_id_to_address_from_bss(vm, op->src0);
        if (!address) {
          address = vm_id_to_address_from_data(vm, op->src0);
        }
        u64* rax = reg(vm, RAX);
        *rax = (u64)address;
        stack_push(vm, *rax);
        assert(address != NULL);
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
          u8* address = vm_id_to_address_from_bss(vm, op->src0);
          if (!address) {
            address = vm_id_to_address_from_data(vm, op->src0);
          }
          switch (op->dest) {
            case TypeUnsigned64: {
              assert(address != NULL);
              u64 value = *(u64*)address;
              u64* rax = reg(vm, RAX);
              *rax = value;
              stack_push(vm, *rax);
              break;
            }
            case TypeCString: {
              address = vm_id_to_address_from_cstring_data(vm, op->src0);
              assert(address != NULL);
              u64* value = (u64*)address;
              u64* rax = reg(vm, RAX);
              *rax = (u64)value;
              stack_push(vm, *rax);
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
              u64 imm = vm->imm[op->src1];
              u64* rax = reg(vm, RAX);
              *rax = imm;
              stack_push(vm, *rax);
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
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax + *rbx;
        stack_push(vm, *rax);
        break;
      }
      case I_SUB: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax - *rbx;
        stack_push(vm, *rax);
        break;
      }
      case I_MUL: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax * *rbx;
        stack_push(vm, *rax);
        break;
      }
      case I_DIV: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax / *rbx;
        stack_push(vm, *rax);
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
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax % *rbx;
        stack_push(vm, *rax);
        break;
      }
      case I_LT: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax < *rbx;
        stack_push(vm, *rax);
        break;
      }
      case I_GT: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax > *rbx;
        stack_push(vm, *rax);
        break;
      }
      case I_AND: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax & *rbx;
        stack_push(vm, *rax);
        break;
      }
      case I_LOGICAL_NOT: {
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rax);
        *rax = !(*rax);
        stack_push(vm, *rax);
        break;
      }
      case I_OR: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax | *rbx;
        stack_push(vm, *rax);
        break;
      }
      case I_EQ: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax == *rbx;
        stack_push(vm, *rax);
        break;
      }
      case I_NEQ: {
        u64* rbx = reg(vm, RBX);
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rbx);
        stack_pop(vm, rax);
        *rax = *rax != *rbx;
        stack_push(vm, *rax);
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
        u64* rax = reg(vm, RAX);
        u64 a = stack_pop(vm, rax);
        printf("%ld\n", a);
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
        u64* rax = reg(vm, RAX);
        stack_pop(vm, rax);
        *rax = syscall(*rax);
        stack_push(vm, *rax);
        break;
      }
      case I_SYSCALL1: {
        u64* rax = reg(vm, RAX);
        u64* rdi = reg(vm, RDI);
        stack_pop(vm, rax);
        stack_pop(vm, rdi);
        *rax = syscall(*rax, *rdi);
        stack_push(vm, *rax);
        break;
      }
      case I_SYSCALL2: {
        u64* rax = reg(vm, RAX);
        u64* rdi = reg(vm, RDI);
        u64* rsi = reg(vm, RSI);
        stack_pop(vm, rax);
        stack_pop(vm, rdi);
        stack_pop(vm, rsi);
        *rax = syscall(*rax, *rdi, *rsi);
        stack_push(vm, *rax);
        break;
      }
      case I_SYSCALL3: {
        u64* rax = reg(vm, RAX);
        u64* rdi = reg(vm, RDI);
        u64* rsi = reg(vm, RSI);
        u64* rdx = reg(vm, RDX);
        stack_pop(vm, rax);
        stack_pop(vm, rdi);
        stack_pop(vm, rsi);
        stack_pop(vm, rdx);
        *rax = syscall(*rax, *rdi, *rsi, *rdx);
        stack_push(vm, *rax);
        break;
      }
      case I_SYSCALL4: {
        u64* rax = reg(vm, RAX);
        u64* rdi = reg(vm, RDI);
        u64* rsi = reg(vm, RSI);
        u64* rdx = reg(vm, RDX);
        u64* r10 = reg(vm, R10);
        stack_pop(vm, rax);
        stack_pop(vm, rdi);
        stack_pop(vm, rsi);
        stack_pop(vm, rdx);
        stack_pop(vm, r10);
        *rax = syscall(*rax, *rdi, *rsi, *rdx, *r10);
        stack_push(vm, *rax);
        break;
      }
      case I_SYSCALL5: {
        u64* rax = reg(vm, RAX);
        u64* rdi = reg(vm, RDI);
        u64* rsi = reg(vm, RSI);
        u64* rdx = reg(vm, RDX);
        u64* r10 = reg(vm, R10);
        u64* r8 = reg(vm, R8);
        stack_pop(vm, rax);
        stack_pop(vm, rdi);
        stack_pop(vm, rsi);
        stack_pop(vm, rdx);
        stack_pop(vm, r10);
        stack_pop(vm, r8);
        *rax = syscall(*rax, *rdi, *rsi, *rdx, *r10, *r8);
        stack_push(vm, *rax);
        break;
      }
      case I_SYSCALL6: {
        u64* rax = reg(vm, RAX);
        u64* rdi = reg(vm, RDI);
        u64* rsi = reg(vm, RSI);
        u64* rdx = reg(vm, RDX);
        u64* r10 = reg(vm, R10);
        u64* r8 = reg(vm, R8);
        u64* r9 = reg(vm, R9);
        stack_pop(vm, rax);
        stack_pop(vm, rdi);
        stack_pop(vm, rsi);
        stack_pop(vm, rdx);
        stack_pop(vm, r10);
        stack_pop(vm, r8);
        stack_pop(vm, r9);
        *rax = syscall(*rax, *rdi, *rsi, *rdx, *r10, *r8, *r9);
        stack_push(vm, *rax);
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

// TODO(lucas): this is slow, create mapping from ids to addresses
u8* vm_id_to_address_from_bss(Vm* vm, i32 id) {
  i32 current_id = NUM_SYSCALL + 1; // ids 0-7 are reserved for syscalls
  u8* memory_area = &vm->bss[0];
  u32 memory_area_size = vm->bss_size;
  for (u32 i = 0; i < memory_area_size; ++current_id) {
    Compile_type type = *(Compile_type*)&memory_area[i];
    i += sizeof(type);
    u32 size = *(Compile_type*)&memory_area[i];
    i += sizeof(size);
    if (current_id == id) {
      return &memory_area[i];
    }
    i += size;
  }
  return NULL;
}

// TODO(lucas): this is slow, create mapping from ids to addresses
u8* vm_id_to_address_from_data(Vm* vm, i32 id) {
  i32 current_id = NUM_SYSCALL + 1; // ids 0-7 are reserved for syscalls
  u8* memory_area = &vm->data[0];
  u32 memory_area_size = vm->data_size;
  for (u32 i = 0; i < memory_area_size; ++current_id) {
    Compile_type type = *(Compile_type*)&memory_area[i];
    i += sizeof(type);
    u32 size = *(Compile_type*)&memory_area[i];
    i += sizeof(size);
    if (current_id == id) {
      return &memory_area[i];
    }
    i += size;
  }
  return NULL;
}

// TODO(lucas): this is slow, create mapping from ids to addresses
u8* vm_id_to_address_from_cstring_data(Vm* vm, i32 id) {
  i32 current_id = 0;
  u8* memory_area = &vm->cstrings[0];
  u32 memory_area_size = vm->cstring_size;
  for (u32 i = 0; i < memory_area_size; ++current_id) {
    u32 size = *(Compile_type*)&memory_area[i];
    i += sizeof(size);
    if (current_id == id) {
      return &memory_area[i];
    }
    i += size;
  }
  return NULL;
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

u64 stack_push(Vm* vm, u32 v) {
  u64* rsp = reg(vm, RSP);
  if (*rsp > 0) {
    return (vm->stack[--(*rsp)] = v);
  }
  assert("stack overflow" && 0);
  return -1;
}

u64 stack_pop(Vm* vm, u64* r) {
  u64* rsp = reg(vm, RSP);
  if (*rsp < MAX_STACK) {
    u64 result = vm->stack[(*rsp)++];
    if (r) {
      *r = result;
    }
    return result;
  }
  assert("stack underflow" && 0);
  return -1;
}

// NOTE(lucas): not really a conventional stack trace, but just a way of printing the stack contents
void stack_trace(Vm* vm) {
  FILE* fp = stdout;
  fprintf(fp, "%s:\n", __FUNCTION__);
  u64* rsp = reg(vm, RSP);
  for (u32 i = *rsp; i < MAX_STACK; ++i) {
    u64 value = vm->stack[i];
    fprintf(fp, "%ld\n", value);
  }
}

u64* reg(Vm* vm, Register r) {
  return &vm->regs[r];
}
