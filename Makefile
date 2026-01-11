all:
	gcc src/vm.c -o vm
	gcc src/assembler.c -o asm

clean:
	rm -f vm asm bytecode/*.bc
