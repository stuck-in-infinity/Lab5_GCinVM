#include "opcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void write_int(FILE *out, int v)
{
  fwrite(&v, sizeof(int), 1, out);
}

static void write_byte_checked(FILE *out, int v, const char *op)
{
  if (v < 0 || v > 255)
  {
    fprintf(stderr,
            "Assembler error: %s operand out of range (0–255): %d\n",
            op, v);
    exit(1);
  }
  uint8_t b = (uint8_t)v;
  fwrite(&b, 1, 1, out);
}

int main(int argc, char **argv)
{
  if (argc < 3)
  {
    printf("Usage: %s input.asm output.bc\n", argv[0]);
    return 1;
  }

  FILE *in = fopen(argv[1], "r");
  FILE *out = fopen(argv[2], "wb");

  if (!in || !out)
  {
    perror("File error");
    return 1;
  }

  char op[32];
  int val;

  while (fscanf(in, "%s", op) == 1)
  {
    if (!strcmp(op, "PUSH"))
    {
      fscanf(in, "%d", &val);
      fputc(OP_PUSH, out);
      write_int(out, val);
    }
    else if (!strcmp(op, "POP"))
      fputc(OP_POP, out);
    else if (!strcmp(op, "DUP"))
      fputc(OP_DUP, out);

    else if (!strcmp(op, "ADD"))
      fputc(OP_ADD, out);
    else if (!strcmp(op, "SUB"))
      fputc(OP_SUB, out);
    else if (!strcmp(op, "MUL"))
      fputc(OP_MUL, out);
    else if (!strcmp(op, "DIV"))
      fputc(OP_DIV, out);
    else if (!strcmp(op, "CMP"))
      fputc(OP_CMP, out);

    else if (!strcmp(op, "JMP"))
    {
      fscanf(in, "%d", &val);
      fputc(OP_JMP, out);
      write_int(out, val);
    }
    else if (!strcmp(op, "JZ"))
    {
      fscanf(in, "%d", &val);
      fputc(OP_JZ, out);
      write_int(out, val);
    }
    else if (!strcmp(op, "JNZ"))
    {
      fscanf(in, "%d", &val);
      fputc(OP_JNZ, out);
      write_int(out, val);
    }

    else if (!strcmp(op, "STORE"))
    {
      fscanf(in, "%d", &val);
      fputc(OP_STORE, out);
      write_byte_checked(out, val, "STORE");
    }
    else if (!strcmp(op, "LOAD"))
    {
      fscanf(in, "%d", &val);
      fputc(OP_LOAD, out);
      write_byte_checked(out, val, "LOAD");
    }

    else if (!strcmp(op, "CALL"))
    {
      fscanf(in, "%d", &val);
      fputc(OP_CALL, out);
      write_int(out, val);
    }
    else if (!strcmp(op, "RET"))
      fputc(OP_RET, out);

    else if (!strcmp(op, "HALT"))
      fputc(OP_HALT, out);

    else
    {
      fprintf(stderr, "Unknown instruction: %s\n", op);
      exit(1);
    }
  }

  fclose(in);
  fclose(out);
  return 0;
}
