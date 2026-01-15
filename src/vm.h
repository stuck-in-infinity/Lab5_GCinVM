#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define STACK_SIZE 1024
#define MEM_SIZE 256
#define CODE_SIZE 4096
#define CALLSTACK_SIZE 256

typedef enum {
    VAL_INT,
    VAL_OBJ
} ValueType;

typedef enum {
    OBJ_PAIR,
    OBJ_FUNCTION, 
    OBJ_CLOSURE   
} ObjType;

struct Obj; 

typedef struct {
    ValueType type;
    union {
        int32_t integer;
        struct Obj* obj;
    } as;
} Value;


typedef struct Obj {
    ObjType type;
    bool marked;      
    struct Obj* next; 
} Obj;

typedef struct {
    Obj obj;
    Value head;
    Value tail;
} ObjPair;

typedef struct {
    Obj obj;
    int arity;
} ObjFunction;

typedef struct {
    Obj obj;
    ObjFunction* function;
    struct Obj* upvalues; 
} ObjClosure;

#define IS_OBJ(v) ((v).type == VAL_OBJ)
#define IS_INT(v) ((v).type == VAL_INT)
#define AS_OBJ(v) ((v).as.obj)
#define AS_INT(v) ((v).as.integer)

#define VAL_OBJ(o) ((Value){VAL_OBJ, {.obj = (Obj*)o}})
#define VAL_INT(i) ((Value){VAL_INT, {.integer = i}})

typedef struct {
    Value stack[STACK_SIZE]; 
    Value memory[MEM_SIZE];
    int32_t callstack[CALLSTACK_SIZE];
    uint8_t code[CODE_SIZE];

    int sp;
    int pc;
    int csp;
    int running;
    int error;
    bool debug_gc;             
    unsigned long total_freed; 
    int gc_run_count;

    // GC State
    Obj* objects;   
    int numObjects; 
    int maxObjects; 

    Obj** grayStack;
    int grayCount;
    int grayCapacity;

    unsigned long instr_count;
    unsigned long byte_count;
} VM;

void vm_init(VM *vm);
void vm_free(VM *vm); 
void vm_load(VM *vm, const char *filename);
void vm_run(VM *vm);

void gc(VM* vm);
void push(VM *vm, Value v); 
Value pop(VM *vm);          

Obj* new_pair(VM* vm, Value head, Value tail);
ObjFunction* new_function(VM* vm);
ObjClosure* new_closure(VM* vm, ObjFunction* func, Obj* upvalues);

#endif
