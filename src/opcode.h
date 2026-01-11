#ifndef OPCODE_H
#define OPCODE_H

#include <stdint.h>

enum
{
  OP_PUSH = 0x01,
  OP_POP = 0x02,
  OP_DUP = 0x03,

  OP_ADD = 0x10,
  OP_SUB = 0x11,
  OP_MUL = 0x12,
  OP_DIV = 0x13,
  OP_CMP = 0x14,

  OP_JMP = 0x20,
  OP_JZ = 0x21,
  OP_JNZ = 0x22,

  OP_STORE = 0x30,
  OP_LOAD = 0x31,

  OP_CALL = 0x40,
  OP_RET = 0x41,

  OP_HALT = 0xFF
};

#endif
