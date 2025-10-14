#include <stdio.h>
#include <stdint.h>

// Defining memory
#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];

// Registers (8 General Purpose (R0-R7), Program Counter (PC) and Condition Flag (COND)
enum
{
  R_R0 = 0,
  R_R1,
  R_R2,
  R_R3,
  R_R4,
  R_R5,
  R_R6,
  R_R7,
  R_PC,
  R_COND,
  R_COUNT
};
uint16_t reg[R_COUNT];

// Conditional flags
enum
{
  FL_POS = 1 << 0,
  FL_ZRO = 1 << 1,
  FL_NEG = 1 << 2,
};

// Update flags according to result of instruction
void update_flags(uint16_t r)
{
  if (reg[r] == 0)
  {
    reg[R_COND] = FL_ZRO;
  }
  else if (reg[r] >> 15)
  {
    reg[R_COND] = FL_NEG;
  }
  else
  {
    reg[R_COND] = FL_POS;
  }
}

// TRAP Codes
enum
{
  TRAP_GETC = 0x20,
  TRAP_OUT = 0x21,
  TRAP_PUTS = 0x22,
  TRAP_IN = 0x23,
  TRAP_PUTSP = 0x24,
  TRAP_HALT = 0x25
};

// Instruction Set (Opcodes)
enum
{
  OP_BR = 0,
  OP_ADD,
  OP_LD,
  OP_ST,
  OP_JSR,
  OP_AND,
  OP_LDR,
  OP_STR,
  OP_RTI,
  OP_NOT,
  OP_LDI,
  OP_STI,
  OP_JMP,
  OP_RES,
  OP_LEA,
  OP_TRAP,
};

// Sign extend for negative numbers
uint16_t sign_extend(uint16_t x, int bit_count)
{
  if ((x >> (bit_count - 1)) & 1)
  {                             // Check if negative
    x |= (0xFFFF << bit_count); // Add 1s before the number
  }
  return x;
}

int main(int argc, const char *argv[])
{
  if (argc < 2)
  {
    printf("lc3 [image file] ...\n");
    exit(2);
    ;
  }
  for (int arg = 1; arg < argc; ++arg)
  {
    if (!read_image(argv[arg]))
    {
      printf("failed to load image: %s\n", argv[arg]);
      exit(1);
    }
  }

  // Setup
  reg[R_COND] = FL_ZRO;

  enum
  {
    PC_START = 0x3000
  };
  reg[R_PC] = PC_START;

  int running = 1;

  while (running)
  {
    uint16_t instr = mem_read(reg[R_PC]++);
    uint16_t opcode = instr >> 12;
    switch (opcode)
    {
    case OP_ADD:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t r1 = (instr >> 6) & 0x7;
      uint16_t imm_flag = (instr >> 5) & 0x1;

      if (imm_flag)
      {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[r0] = reg[r1] + imm5;
      }
      else
      {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] + reg[r2];
      }
      update_flags(r0);
    }
    break;
    case OP_AND:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t r1 = (instr >> 6) & 0x7;
      uint16_t imm_flag = (instr >> 5) & 0x1;

      if (imm_flag)
      {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[r0] = reg[r1] & imm5;
      }
      else
      {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] & reg[r2];
      }
      update_flags(r0);
    }
    break;
    case OP_NOT:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t r1 = (instr >> 6) & 0x7;

      reg[r0] = ~reg[r1];
      update_flags(r0);
    }
    break;
    case OP_BR:
    {
      uint16_t cond_flag = instr >> 9;
      uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
      if (cond_flag & reg[R_COND])
      {
        reg[R_PC] += pc_offset;
      }
    }
    break;
    case OP_JMP:
    {
      uint16_t r1 = (instr >> 6) & 0x7;
      reg[R_PC] = reg[r1];
    }
    break;
    case OP_JSR:
    {
      reg[R_R7] = reg[R_PC];
      uint16_t flag = (instr >> 11) & 0x1;
      if (flag)
      {
        uint16_t pc_offset = sign_extend(instr & 0x7FF, 11);
        reg[R_PC] += pc_offset;
      }
      else
      {
        uint16_t r1 = (instr >> 6) & 0x7;
        reg[R_PC] = reg[r1];
      }
    }
    break;
    case OP_LD:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t pc_offset = sign_extend(instr && 0x1FF, 9);
      reg[r0] = mem_read(reg[R_PC + pc_offset]);
      update_flags(r0);
    }
    break;
    case OP_LDI:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

      reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
      update_flags(r0);
    }
    break;
    case OP_LDR:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t r1 = (instr >> 6) & 0x7;
      uint16_t offset = sign_extend(instr & 0x3F, 6);
      reg[r0] = mem_read(reg[r1] + offset);
      update_flags(r0);
    }
    break;
    case OP_LEA:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t pc_offset = sign_extend(instr * 0x1FF, 9);
      reg[r0] = mem_read(reg[R_PC] + pc_offset);
      update_flags(r0);
    }
    break;
    case OP_ST:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t pc_offset = sign_extend(instr * 0x1FF, 9);
      mem_write(reg[R_PC] + pc_offset, reg[r0]);
    }
    break;
    case OP_STI:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t pc_offset = sign_extend(instr * 0x1FF, 9);
      mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
    }
    break;
    case OP_STR:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t r1 = (instr >> 6) & 0x7;
      uint16_t offset = sign_extend(instr & 0x3F, 6);
      mem_write(reg[r1] + offset, reg[r0]);
    }
    break;
    case OP_TRAP:
    {
      reg[R_R7] = reg[R_PC];

      switch (instr & 0xFF)
      {
      case TRAP_GETC:
              {
                reg[R_R0] = (uint16_t)getchar();
                update_flags(R_R0);
              }
            break;
      case TRAP_OUT:
              {
                // Could cause issue
                uint16_t c = reg[R_R0];
                putc((char)c,stdout);
                fflush(stdout);
              }
            break;
      case TRAP_PUTS:
              {
                uint16_t* c = memory + reg[R_R0];
                while(*c){
                  putc((char)*c, stdout);
                  ++c;
                }
                fflush(stdout);
              }
            break;
      case TRAP_IN:
              {
                printf("Enter a character: ");
                char c = getchar();
                putc(c, stdout);
                fflush(stdout);
                reg[R_R0] = (uint16_t)c;
                update_flags(R_R0);
              }
      case TRAP_PUTSP:
              {
                uint16_t* c = memory + reg[R_R0];
                while (*c) {
                  char char1 = (*c) & 0xFF;
                  putc(char1, stdout);
                  char char2 = (*c) >> 8;
                  if (char2) putc(char2, stdout);
                  ++c;
                }
                fflush(stdout);
              }
      case TRAP_HALT:
              {
                puts("HALT");
                fflush(stdout);
                running = 0;
              }
      }
    }
    break;
    case OP_RES:
    case OP_RTI:
    default:
      break;
    }
    return 0;
  }
}
