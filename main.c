#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "./src/vm.h"

int count_live_objects(VM* vm) {
    int count = 0;
    Obj* obj = vm->objects;
    while (obj) {
        count++;
        obj = obj->next;
    }
    return count;
}

// Basic Reachability
void test_reachability(VM* vm) {
    printf("Test 1: Basic Reachability... \n");
    vm_init(vm);
    vm->debug_gc = true;
    Obj* a = new_pair(vm, VAL_INT(0), VAL_INT(0));
    push(vm, VAL_OBJ(a));
    
    gc(vm);
    
    assert(count_live_objects(vm) == 1); 
    printf("PASSED\n");
    vm_free(vm);
}

// Unreachable Object Collection
void test_unreachable(VM* vm) {
    printf("Test 2: Unreachable Object... \n");
    vm_init(vm);
    vm->debug_gc = true;
    new_pair(vm, VAL_INT(0), VAL_INT(0)); 
    
    gc(vm);
    
    assert(count_live_objects(vm) == 0); 
    printf("PASSED\n");
    vm_free(vm);
}

// Transitive Reachability
void test_transitive(VM* vm) {
    printf("Test 3: Transitive Reachability... \n");
    vm_init(vm);
    vm->debug_gc = true;
    Obj* a = new_pair(vm, VAL_INT(1), VAL_INT(1));
    Obj* b = new_pair(vm, VAL_OBJ(a), VAL_INT(2));
    push(vm, VAL_OBJ(b)); // Stack -> b -> a
    
    gc(vm);
    
    // Both survive
    assert(count_live_objects(vm) == 2); 
    printf("PASSED\n");
    vm_free(vm);
}

// Cyclic References
void test_cycles(VM* vm) {
    printf("Test 4: Cyclic References... ");
    vm_init(vm);
    vm->debug_gc = true;
    Obj* a = new_pair(vm, VAL_INT(0), VAL_INT(0));
    Obj* b = new_pair(vm, VAL_OBJ(a), VAL_INT(0));
    
    // Create cycle: a->tail = b
    ObjPair* pairA = (ObjPair*)a;
    pairA->tail = VAL_OBJ(b);

    push(vm, VAL_OBJ(a)); // Root -> a <-> b
    
    gc(vm);
    
    assert(count_live_objects(vm) == 2);
    printf("PASSED\n");
    vm_free(vm);
}

// Deep Object Graph
void test_deep_graph(VM* vm) {
    printf("Test 5: Deep Object Graph... \n");
    vm_init(vm);
    
    Obj* root = new_pair(vm, VAL_INT(0), VAL_INT(0));
    
    push(vm, VAL_OBJ(root)); 
    
    Obj* cur = root;
    
   
    for (int i = 0; i < 10000; i++) {
        Obj* next = new_pair(vm, VAL_INT(0), VAL_INT(0));
        ((ObjPair*)cur)->tail = VAL_OBJ(next);
        cur = next;
    }
    
    
    gc(vm);
    
    int count = count_live_objects(vm);
    if (count != 10001) {
        printf("FAILED: Expected 10001 objects, found %d\n", count);
        assert(count == 10001); 
    }
    
    printf("PASSED\n");
    vm_free(vm);
}

// Closure Capture 
void test_closure(VM* vm) {
    printf("Test 6: Closure Capture... \n");
    vm_init(vm);
    vm->debug_gc = true;
    Obj* env = new_pair(vm, VAL_INT(10), VAL_INT(20));
    ObjFunction* fn = new_function(vm);
    ObjClosure* cl = new_closure(vm, fn, env);
    
    push(vm, VAL_OBJ(cl));
    gc(vm);
    
    assert(count_live_objects(vm) == 3);
    printf("PASSED\n");
    vm_free(vm);
}

// Stress Allocation
void test_stress(VM* vm) {
  printf("Test 7: Stress Allocation... ");
    fflush(stdout); 
    
    vm_init(vm);
    // vm->debug_gc = true;
    
    clock_t start = clock();
    
    // Allocation Loop
    for (int i = 0; i < 100000; i++) {
        new_pair(vm, VAL_INT(i), VAL_INT(i));
    }
    
    gc(vm); // Final cleanup
    
    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    
    assert(count_live_objects(vm) == 0);
    
    printf("PASSED\n");
    
    printf("   [Perf Report] Time: %.4fs | Total Freed: %lu objs\n\n", 
           time_spent,  
           vm->total_freed);
           
    vm_free(vm);
}

int main() {
    VM vm;
    printf("=== Starting Lab 5 GC Tests ===\n");
    
    test_reachability(&vm);
    test_unreachable(&vm);
    test_transitive(&vm);
    test_cycles(&vm);
    test_deep_graph(&vm);
    test_closure(&vm);
    test_stress(&vm);
    
    printf("=== All Tests Passed ===\n");
    return 0;
}