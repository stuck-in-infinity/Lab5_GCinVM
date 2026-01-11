#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

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
    bool marked;      // For GC Mark phase
    struct Obj* next; // Linked list of all allocated objects
} Obj;

typedef struct {
    Obj obj; // Header
    Value head;
    Value tail;
} ObjPair;

// Macros for ease of use
#define IS_OBJ(v) ((v).type == VAL_OBJ)
#define IS_INT(v) ((v).type == VAL_INT)
#define AS_OBJ(v) ((v).as.obj)
#define AS_INT(v) ((v).as.integer)

#define VAL_OBJ(o) ((Value){VAL_OBJ, {.obj = (Obj*)o}})
#define VAL_INT(i) ((Value){VAL_INT, {.integer = i}})


typedef struct {
    // Stack and Memory now hold Values
    Value stack[STACK_SIZE]; 
    Value memory[MEM_SIZE];
    
    int32_t callstack[CALLSTACK_SIZE];
    uint8_t code[CODE_SIZE];

    int sp;
    int pc;
    int csp;
    int running;
    int error;

    // GC State
    Obj* objects;   // Head of the linked list of all heap objects
    int numObjects; // Current count of objects
    int maxObjects; 

   
    unsigned long instr_count;
    unsigned long byte_count;

} VM;

void vm_init(VM *vm);
void vm_load(VM *vm, const char *filename);
void vm_run(VM *vm);

Obj* new_pair(VM* vm, Value head, Value tail);

#endif
