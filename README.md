# LC3 Virtual Machine

A simple **LC-3 (Little Computer 3)** Virtual Machine implemented in C.
This project emulates the LC-3 architecture — a 16-bit educational computer designed to teach low-level programming and computer organization concepts.

## Overview

This VM executes **LC-3 object files** (`.obj`) by simulating:

- **Registers**: R0–R7, PC, COND
- **Memory**: 65,536 words (16-bit each)
- **Instruction Set**: ADD, AND, NOT, BR, JMP, JSR, LD, LDI, LDR, LEA, ST, STI, STR, TRAP
- **I/O Handling**: keyboard input and console output

## Features

- Full LC-3 instruction set support
- Keyboard interrupt simulation
- Memory-mapped I/O
- Endianness-correct `.obj` loading

## Features
```
lc3
|--- Makefile
|--- src
  |--- lc3.c
|--- 2048.obj
```


## Build & Run

### 1. Build

```bash
make
```

### 2. Run

```bash
./lc3 <program.obj>
```

## Future Improvements

- Interactive debugger
- Instruction tracing / logging
- Assembler support
