#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

// Memory mapped registers
enum
{
  MR_KBSR = 0xFE00,
  MR_KBDR = 0xFE02
};

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

// Defining memory
#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];
uint16_t reg[R_COUNT];

// Conditional flags
enum
{
  FL_POS = 1 << 0,
  FL_ZRO = 1 << 1,
  FL_NEG = 1 << 2,
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

// Enable/Disable buffer
struct termios original_tio;

void disable_input_buffering()
{
  tcgetattr(STDIN_FILENO, &original_tio);
  struct termios new_tio = original_tio;
  new_tio.c_lflag &= ~ICANON & ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key()
{
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

// Handle interrupt
void handle_interrupt(int signal)
{
  restore_input_buffering();
  printf("\n");
  exit(-2);
}

// Sign extend for negative numbers
uint16_t sign_extend(uint16_t x, int bit_count)
{
  if ((x >> (bit_count - 1)) & 1)
  {                             // Check if negative
    x |= (0xFFFF << bit_count); // Add 1s before the number
  }
  return x;
}

// Swap from little endian to big endian
uint16_t swap16(uint16_t x)
{
  return (x << 8) | (x >> 8);
}

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

// Read image file
void read_image_file(FILE *file)
{
  uint16_t origin;
  fread(&origin, sizeof(origin), 1, file);
  origin = swap16(origin);

  uint16_t max_read = MEMORY_MAX - origin;
  uint16_t *p = memory + origin;
  size_t read = fread(p, sizeof(uint16_t), max_read, file);

  while (read-- > 0)
  {
    *p = swap16(*p);
    ++p;
  }
}

int read_image(const char *image_path)
{
  FILE *file = fopen(image_path, "rb");
  if (!file)
  {
    return 0;
  }
  read_image_file(file);
  fclose(file);
  return 1;
}

// Memory access
void mem_write(uint16_t address, uint16_t value)
{
  memory[address] = value;
}

uint16_t mem_read(uint16_t address)
{
  if (address == MR_KBSR)
  {
    if (check_key())
    {
      memory[MR_KBSR] = (1 << 15);
      memory[MR_KBDR] = getchar();
    }
    else
    {
      memory[MR_KBSR] = 0;
    }
  }
  return memory[address];
}


int main(int argc, const char *argv[])
{
  if (argc < 2)
  {
    printf("lc3 [image file] ...\n");
    exit(2);
    ;
  }
  signal(SIGINT, handle_interrupt);
  disable_input_buffering();

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
      uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
      reg[r0] = mem_read(reg[R_PC] + pc_offset);
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
      uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
      reg[r0] = mem_read(reg[R_PC] + pc_offset);
      update_flags(r0);
    }
    break;
    case OP_ST:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
      mem_write(reg[R_PC] + pc_offset, reg[r0]);
    }
    break;
    case OP_STI:
    {
      uint16_t r0 = (instr >> 9) & 0x7;
      uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
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
        putc((char)reg[R_R0], stdout);
        fflush(stdout);
      }
      break;
      case TRAP_PUTS:
      {
        uint16_t *c = memory + reg[R_R0];
        while (*c)
        {
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
      break;
      case TRAP_PUTSP:
      {
        uint16_t *c = memory + reg[R_R0];
        while (*c)
        {
          char char1 = (*c) & 0xFF;
          putc(char1, stdout);
          char char2 = (*c) >> 8;
          if (char2)
            putc(char2, stdout);
          ++c;
        }
        fflush(stdout);
      }
      break;
      case TRAP_HALT:
      {
        puts("HALT");
        fflush(stdout);
        running = 0;
      }
      break;
      }
    }
    break;
    case OP_RES:
    case OP_RTI:
    default:
      abort();
    }
  }
  restore_input_buffering();
  return 0;
}
