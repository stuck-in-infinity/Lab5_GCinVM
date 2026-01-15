#include "vm.h"
#include "opcode.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


Value pop(VM *vm) {
  if (vm->sp <= 0) {
    fprintf(stderr, "Error: Stack Underflow\n");
    vm->error = 1;
    vm->running = 0;
    return VAL_INT(0);
  }
  return vm->stack[--vm->sp];
}

void push(VM *vm, Value v) {
  if (vm->sp >= STACK_SIZE) {
    fprintf(stderr, "Error: Stack Overflow\n");
    vm->error = 1;
    vm->running = 0;
    return;
  }
  vm->stack[vm->sp++] = v;
}

// GC & Allocator

static Obj* allocateObj(VM* vm, size_t size, ObjType type) {
    if (vm->numObjects >= vm->maxObjects) {
        #ifdef DEBUG_GC
        printf("Max objects reached, triggering GC...\n");
        #endif
        gc(vm);
        
        if (vm->numObjects >= vm->maxObjects) {
            vm->maxObjects *= 2;
        }
    }

    Obj* object = (Obj*)malloc(size);
    if (object == NULL) {
        fprintf(stderr, "Fatal: Heap Allocation Failed\n");
        exit(1);
    }
    
    object->type = type;
    object->marked = false;
    
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

ObjFunction* new_function(VM* vm) {
    ObjFunction* func = (ObjFunction*)allocateObj(vm, sizeof(ObjFunction), OBJ_FUNCTION);
    func->arity = 0;
    return func;
}

ObjClosure* new_closure(VM* vm, ObjFunction* func, Obj* upvalues) {
    ObjClosure* closure = (ObjClosure*)allocateObj(vm, sizeof(ObjClosure), OBJ_CLOSURE);
    closure->function = func;
    closure->upvalues = upvalues;
    return closure;
}

// Mark-Sweep

static void mark_object(VM* vm, Obj* obj) {
    if (obj == NULL || obj->marked) return;

    obj->marked = true;

    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = vm->grayCapacity < 8 ? 8 : vm->grayCapacity * 2;
        vm->grayStack = (Obj**)realloc(vm->grayStack, sizeof(Obj*) * vm->grayCapacity);
    }
    
    vm->grayStack[vm->grayCount++] = obj;
}

void mark_value(VM* vm, Value v) {
    if (IS_OBJ(v)) mark_object(vm, AS_OBJ(v));
}

static void blacken_object(VM* vm, Obj* obj) {
    switch (obj->type) {
        case OBJ_PAIR: {
            ObjPair* pair = (ObjPair*)obj;
            mark_value(vm, pair->head);
            mark_value(vm, pair->tail);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)obj;
            mark_object(vm, (Obj*)closure->function);
            mark_object(vm, closure->upvalues);
            break;
        }
        case OBJ_FUNCTION: {
            break;
        }
    }
}

static void trace_references(VM* vm) {
    while (vm->grayCount > 0) {
        Obj* obj = vm->grayStack[--vm->grayCount];
        blacken_object(vm, obj);
    }
}

static void mark_roots(VM* vm) {
    for (int i = 0; i < vm->sp; i++) mark_value(vm, vm->stack[i]);
    for (int i = 0; i < MEM_SIZE; i++) mark_value(vm, vm->memory[i]);
}

static void sweep(VM* vm) {
    Obj** object = &vm->objects;
    while (*object) {
        Obj* curr = *object;
        if (curr->marked) {
            curr->marked = false;
            object = &curr->next;
        } else {
            Obj* unreached = curr;
            *object = unreached->next; 
            free(unreached);
            vm->numObjects--;
        }
    }
}


void gc(VM* vm) {
    vm->gc_run_count++; 
    
    int objectsBefore = vm->numObjects;
    
    mark_roots(vm);
    trace_references(vm); 
    sweep(vm);

    int objectsAfter = vm->numObjects;
    int objectsFreed = objectsBefore - objectsAfter;
    
    vm->total_freed += objectsFreed;

    // Only print if flag is true
    if (vm->debug_gc && objectsFreed > 0) {
        printf("[GC] Cycle %d: Collected %d objects (from %d to %d)\n", 
               vm->gc_run_count, objectsFreed, objectsBefore, objectsAfter);
    }
    
}

// VM Implementation

void vm_init(VM *vm) {
  vm->sp = vm->pc = vm->csp = 0;
  vm->running = 1;
  vm->error = 0;
  vm->instr_count = 0;
  vm->byte_count = 0;

  vm->objects = NULL;
  vm->numObjects = 0;
  vm->maxObjects = 100; // Threshold

  vm->grayCount = 0;
  vm->grayCapacity = 0;
  vm->grayStack = NULL;

  vm->debug_gc = false;      // Default we donot print each object's status
  vm->total_freed = 0;
  vm->gc_run_count = 0;

  memset(vm->stack, 0, sizeof(vm->stack));
  memset(vm->memory, 0, sizeof(vm->memory));
  memset(vm->callstack, 0, sizeof(vm->callstack));
  memset(vm->code, 0, sizeof(vm->code));
}

void vm_free(VM *vm) {
    
    if (vm->grayStack) free(vm->grayStack);
    
    // Free all objects
    Obj* current = vm->objects;
    while (current) {
        Obj* next = current->next;
        free(current);
        current = next;
    }
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
      push(vm, VAL_INT(v)); 
      break;
    }
    case OP_POP: pop(vm); break;
    case OP_DUP:
      if (vm->sp <= 0) {
        fprintf(stderr, "Error: Stack Underflow on DUP\n");
        vm->error = 1; vm->running = 0;
      } else {
        push(vm, vm->stack[vm->sp - 1]);
      }
      break;
    case OP_ADD: {
      Value b = pop(vm); Value a = pop(vm);
      if (IS_INT(a) && IS_INT(b)) push(vm, VAL_INT(AS_INT(a) + AS_INT(b)));
      else { vm->error = 1; vm->running = 0; }
      break;
    }
    case OP_SUB: {
      Value b = pop(vm); Value a = pop(vm);
      if (IS_INT(a) && IS_INT(b)) push(vm, VAL_INT(AS_INT(a) - AS_INT(b)));
      else { vm->error = 1; vm->running = 0; }
      break;
    }
    case OP_MUL: {
      Value b = pop(vm); Value a = pop(vm);
      if (IS_INT(a) && IS_INT(b)) push(vm, VAL_INT(AS_INT(a) * AS_INT(b)));
      else { vm->error = 1; vm->running = 0; }
      break;
    }
    case OP_DIV: {
      Value b = pop(vm); Value a = pop(vm);
      if (!IS_INT(a) || !IS_INT(b) || AS_INT(b) == 0) { vm->error = 1; vm->running = 0; }
      else push(vm, VAL_INT(AS_INT(a) / AS_INT(b)));
      break;
    }
    case OP_CMP: {
      Value b = pop(vm); Value a = pop(vm);
      if (IS_INT(a) && IS_INT(b)) push(vm, VAL_INT(AS_INT(a) < AS_INT(b)));
      else { vm->error = 1; vm->running = 0; }
      break;
    }
    case OP_JMP: vm->pc = *(int32_t *)&vm->code[vm->pc]; vm->byte_count += 4; break;
    case OP_JZ: {
      int addr = *(int32_t *)&vm->code[vm->pc]; vm->pc += 4; vm->byte_count += 4;
      Value v = pop(vm);
      if (IS_INT(v) && AS_INT(v) == 0) vm->pc = addr;
      break;
    }
    case OP_JNZ: {
      int addr = *(int32_t *)&vm->code[vm->pc]; vm->pc += 4; vm->byte_count += 4;
      Value v = pop(vm);
      if (!(IS_INT(v) && AS_INT(v) == 0)) vm->pc = addr;
      break;
    }
    case OP_STORE: {
      int idx = vm->code[vm->pc++]; vm->byte_count++;
      if (idx < 0 || idx >= MEM_SIZE) { vm->error = 1; vm->running = 0; }
      else vm->memory[idx] = pop(vm);
      break;
    }
    case OP_LOAD: {
      int idx = vm->code[vm->pc++]; vm->byte_count++;
      if (idx < 0 || idx >= MEM_SIZE) { vm->error = 1; vm->running = 0; }
      else push(vm, vm->memory[idx]);
      break;
    }
    case OP_CALL: {
      int addr = *(int32_t *)&vm->code[vm->pc]; vm->pc += 4; vm->byte_count += 4;
      vm->callstack[vm->csp++] = vm->pc; vm->pc = addr;
      break;
    }
    case OP_RET: vm->pc = vm->callstack[--vm->csp]; break;
    case OP_HALT:
      vm->running = 0;
      if (!vm->error) {
        if (vm->sp > 0) {
            Value v = vm->stack[vm->sp - 1];
            if (IS_INT(v)) printf("VM HALT. Top = %d\n", AS_INT(v));
            else printf("VM HALT. Top = <Object>\n");
        } else printf("VM HALT. Stack empty.\n");
      }
      break;
    default:
      fprintf(stderr, "Invalid opcode: 0x%x\n", op);
      vm->error = 1; vm->running = 0;
    }
  }
  
  clock_t end = clock();
  double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
  printf("\n=== VM BENCHMARK RESULTS ===\n");
  printf("Instructions executed : %lu\n", vm->instr_count);
  printf("Bytes executed        : %lu\n", vm->byte_count);
  printf("Execution time (sec)  : %f\n", time_spent);
}
