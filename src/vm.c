#include "vm.h"
#include "opcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


static Value pop(VM *vm) {
  if (vm->sp <= 0) {
    fprintf(stderr, "Error: Stack Underflow\n");
    vm->error = 1;
    vm->running = 0;
    return VAL_INT(0);
  }
  return vm->stack[--vm->sp];
}

static void push(VM *vm, Value v) {
  if (vm->sp >= STACK_SIZE) {
    fprintf(stderr, "Error: Stack Overflow\n");
    vm->error = 1;
    vm->running = 0;
    return;
  }
  vm->stack[vm->sp++] = v;
}


static Obj* allocateObj(VM* vm, size_t size, ObjType type) {
    Obj* object = (Obj*)malloc(size);
    if (object == NULL) {
        fprintf(stderr, "Fatal: Heap Allocation Failed\n");
        exit(1);
    }
    
    object->type = type;
    object->marked = false;
    
    // Insert into global object list (allocator requirement)
    object->next = vm->objects;
    vm->objects = object;
    
    vm->numObjects++;
    return object;
}

Obj* new_pair(VM* vm, Value head, Value tail) {
    ObjPair* pair = (ObjPair*)allocateObj(vm, sizeof(ObjPair), OBJ_PAIR);
    pair->head = head;
    pair->tail = tail;
    return (Obj*)pair;
}


void vm_init(VM *vm) {
  vm->sp = vm->pc = vm->csp = 0;
  vm->running = 1;
  vm->error = 0;

  vm->instr_count = 0;
  vm->byte_count = 0;

  // Initialize GC state
  vm->objects = NULL;
  vm->numObjects = 0;
  vm->maxObjects = 100; // Trigger threshold (arbitrary for now)

  // Clear memory
  memset(vm->stack, 0, sizeof(vm->stack));
  memset(vm->memory, 0, sizeof(vm->memory));
  memset(vm->callstack, 0, sizeof(vm->callstack));
  memset(vm->code, 0, sizeof(vm->code));
}

void vm_load(VM *vm, const char *filename) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    perror("Failed to load bytecode");
    exit(1);
  }
  fread(vm->code, 1, CODE_SIZE, f);
  fclose(f);
}

void vm_run(VM *vm) {
  clock_t start = clock();

  while (vm->running) {
    uint8_t op = vm->code[vm->pc++];
    vm->instr_count++;
    vm->byte_count++;

    switch (op) {
    case OP_PUSH: {
      int32_t v = *(int32_t *)&vm->code[vm->pc];
      vm->pc += 4;
      vm->byte_count += 4;
      push(vm, VAL_INT(v)); // Wrap raw int into Value
      break;
    }

    case OP_POP:
      pop(vm);
      break;

    case OP_DUP:
      if (vm->sp <= 0) {
        fprintf(stderr, "Error: Stack Underflow on DUP\n");
        vm->error = 1;
        vm->running = 0;
      } else {
        push(vm, vm->stack[vm->sp - 1]);
      }
      break;

    // --- Arithmetic Ops (Unbox integers) ---
    case OP_ADD: {
      Value b = pop(vm);
      Value a = pop(vm);
      if (IS_INT(a) && IS_INT(b)) {
          push(vm, VAL_INT(AS_INT(a) + AS_INT(b)));
      } else {
          fprintf(stderr, "Error: ADD requires integers\n");
          vm->error = 1; vm->running = 0;
      }
      break;
    }
    case OP_SUB: {
      Value b = pop(vm);
      Value a = pop(vm);
      if (IS_INT(a) && IS_INT(b)) {
          push(vm, VAL_INT(AS_INT(a) - AS_INT(b)));
      } else {
          fprintf(stderr, "Error: SUB requires integers\n");
          vm->error = 1; vm->running = 0;
      }
      break;
    }
    case OP_MUL: {
      Value b = pop(vm);
      Value a = pop(vm);
      if (IS_INT(a) && IS_INT(b)) {
          push(vm, VAL_INT(AS_INT(a) * AS_INT(b)));
      } else {
          fprintf(stderr, "Error: MUL requires integers\n");
          vm->error = 1; vm->running = 0;
      }
      break;
    }
    case OP_DIV: {
      Value b = pop(vm);
      Value a = pop(vm);
      if (!IS_INT(a) || !IS_INT(b)) {
           fprintf(stderr, "Error: DIV requires integers\n");
           vm->error = 1; vm->running = 0;
      } else if (AS_INT(b) == 0) {
        fprintf(stderr, "Error: Division by zero\n");
        vm->error = 1; vm->running = 0;
      } else {
        push(vm, VAL_INT(AS_INT(a) / AS_INT(b)));
      }
      break;
    }
    case OP_CMP: {
      Value b = pop(vm);
      Value a = pop(vm);
      if (IS_INT(a) && IS_INT(b)) {
          push(vm, VAL_INT(AS_INT(a) < AS_INT(b)));
      } else {
          fprintf(stderr, "Error: CMP requires integers\n");
          vm->error = 1; vm->running = 0;
      }
      break;
    }

    // --- Jumps ---
    case OP_JMP:
      vm->pc = *(int32_t *)&vm->code[vm->pc];
      vm->byte_count += 4;
      break;

    case OP_JZ: {
      int addr = *(int32_t *)&vm->code[vm->pc];
      vm->pc += 4;
      vm->byte_count += 4;
      Value v = pop(vm);
      // Assume 0 (integer) is false, anything else true
      if (IS_INT(v) && AS_INT(v) == 0)
        vm->pc = addr;
      break;
    }

    case OP_JNZ: {
      int addr = *(int32_t *)&vm->code[vm->pc];
      vm->pc += 4;
      vm->byte_count += 4;
      Value v = pop(vm);
      if (!(IS_INT(v) && AS_INT(v) == 0))
        vm->pc = addr;
      break;
    }

    // --- Memory ---
    case OP_STORE: {
      int idx = vm->code[vm->pc++];
      vm->byte_count++;
      if (idx < 0 || idx >= MEM_SIZE) {
        fprintf(stderr, "Error: Memory Store OOB\n");
        vm->error = 1; vm->running = 0;
      } else {
        vm->memory[idx] = pop(vm);
      }
      break;
    }

    case OP_LOAD: {
      int idx = vm->code[vm->pc++];
      vm->byte_count++;
      if (idx < 0 || idx >= MEM_SIZE) {
        fprintf(stderr, "Error: Memory Load OOB\n");
        vm->error = 1; vm->running = 0;
      } else {
        push(vm, vm->memory[idx]);
      }
      break;
    }

    // --- Functions ---
    case OP_CALL: {
      int addr = *(int32_t *)&vm->code[vm->pc];
      vm->pc += 4;
      vm->byte_count += 4;
      vm->callstack[vm->csp++] = vm->pc;
      vm->pc = addr;
      break;
    }

    case OP_RET:
      vm->pc = vm->callstack[--vm->csp];
      break;

    case OP_HALT:
      vm->running = 0;
      if (!vm->error) {
        if (vm->sp > 0) {
            Value v = vm->stack[vm->sp - 1];
            if (IS_INT(v)) printf("VM HALT. Top = %d\n", AS_INT(v));
            else printf("VM HALT. Top = <Object>\n");
        }
        else printf("VM HALT. Stack empty.\n");
      }
      break;

    default:
      fprintf(stderr, "Invalid opcode: 0x%x\n", op);
      vm->error = 1;
      vm->running = 0;
    }
  }

  clock_t end = clock();
  double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

  printf("\n=== VM BENCHMARK RESULTS ===\n");
  printf("Instructions executed : %lu\n", vm->instr_count);
  printf("Bytes executed        : %lu\n", vm->byte_count);
  printf("Execution time (sec)  : %f\n", time_spent);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s program.bc\n", argv[0]);
    return 1;
  }
  VM vm;
  vm_init(&vm);
  vm_load(&vm, argv[1]);
  vm_run(&vm);
  
  return 0;
}
