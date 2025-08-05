# MXVM

**Still Under Development.**

MXVM is a custom virtual machine and compiler/interpreter project written in C++. It provides a small, RISC-style bytecode language, a parser/AST frontend, an interpreter, and an x86_64 code generator — all designed as a sandbox for learning and experimentation.

## Project Purpose

> **Not for production use.**  
> Use MXVM to explore:
> - VM architecture & instruction execution  
> - Code parsing & AST generation  
> - Modular design & dynamic loading of external functions  
> - C++ code organization (headers, sources, CMake)  
> - Cross-platform build & dependency management  
> - Debugging, code generation, memory management  

## Features

- **Custom VM**  
  A small instruction set (arithmetic, control flow, memory, string, etc.) implemented in C++.

- **Parser & AST**  
  A grammar for the MXVM language; builds an abstract syntax tree and emits intermediate code.

- **Interpreter & JIT**  
  Run directly in a stack-based VM or generate native x86_64 assembly.

- **Modular System**  
  Load “modules” (IO, string, math, etc.) as shared libraries via `invoke`.

- **Debugging Tools**  
  HTML debug output, memory dumps, verbose error reporting.

- **CMake Build**  
  Cross-platform, modular CMake scripts for Linux, macOS, Windows.

---

## RISC-Style Bytecode Design

MXVM’s bytecode follows a **RISC** philosophy:

1. **Load/Store Architecture**  
   - Only `load` and `store` touch memory; all other ops work on registers or locals.

2. **Simple, Orthogonal Instructions**  
   - One operation per opcode (`add`, `jmp`, `cmp`, etc.), no complex “super” instructions.

3. **Uniform, Fixed Format**  
   - One instruction per line, fixed operand order (destination first).

4. **Explicit Control Flow**  
   - Conditional flags set by `cmp`, branches via `je`, `jne`, `jl`, etc.

---

## Language Overview

A typical `.mxvm` program:

```mxvm
program MyApp {
  section data {
    int    counter = 0
    string msg     = "Hello, MXVM!\n"
  }

  section module { io, string }

  section code {
  start:
    print msg
    add counter, counter, 1
    cmp counter, 10
    jl  start
    done
  }
}
```
- **`program <Name> { … }`**  
  Top-level container.

- **`section data { … }`**  
  Declare storage:  
  - `int` (64-bit), `ptr`, `byte`, `string <name>, <maxlen>`  
  - Initializers only here.

- **`section module { … }`**  
  List modules whose functions can be `invoke`d.

- **`section code { … }`**  
  Sequence of labeled instructions.  
  - **Operands**: first is always a destination (variable/register), following are sources (vars or immediates).  
  - **No inline literals** of type string—only numbers or hex in code; strings live in `data`.  
---

## Instruction Set Reference

### Arithmetic & Logic

| Instruction | Operands                        | Description                     |
|-------------|---------------------------------|---------------------------------|
| `mov`       | `dest, src`                     | Copy `src` → `dest`             |
| `add`       | `dest, a, b`                    | `dest = a + b`                  |
| `sub`       | `dest, a, b`                    | `dest = a - b`                  |
| `mul`       | `dest, a, b`                    | `dest = a * b`                  |
| `div`       | `dest, a, b`                    | `dest = a / b`                  |
| `mod`       | `dest, a, b`                    | `dest = a % b`                  |
| `neg`       | `dest, a`                       | `dest = -a`                     |
| `or`        | `dest, a, b`                    | Bitwise OR                      |
| `and`       | `dest, a, b`                    | Bitwise AND                     |
| `xor`       | `dest, a, b`                    | Bitwise XOR                     |
| `not`       | `dest, a`                       | Bitwise NOT                     |

### Comparison & Branching

| Instruction | Operands          | Description                        |
|-------------|-------------------|------------------------------------|
| `cmp`       | `a, b`            | Compare `a – b`, set flags         |
| `jmp`       | `label`           | Unconditional jump                 |
| `je`        | `label`           | Jump if equal                      |
| `jne`       | `label`           | Jump if not equal                  |
| `jl`        | `label`           | Jump if less (signed)              |
| `jle`       | `label`           | Jump if ≤                           |
| `jg`        | `label`           | Jump if greater (signed)           |
| `jge`       | `label`           | Jump if ≥                           |
| `jz`        | `label`           | Jump if zero flag set              |
| `jnz`       | `label`           | Jump if zero flag clear            |
| `ja`        | `label`           | Jump if above (unsigned)           |
| `jb`        | `label`           | Jump if below (unsigned)           |

### Memory & Stack

| Instruction   | Operands                        | Description                                    |
|---------------|---------------------------------|------------------------------------------------|
| `load`        | `dest, ptr, index, size`        | `dest = *(ptr + index*size)`                   |
| `store`       | `src, ptr, index, size`         | `*(ptr + index*size) = src`                    |
| `alloc`       | `ptr, bytesize, count`          | Allocate `count` items each `bytesize` bytes   |
| `free`        | `ptr`                           | Release allocated memory                       |
| `push`        | `src`                           | Push value onto VM stack                       |
| `pop`         | `dest`                          | Pop value from VM stack                        |
| `stack_load`  | `dest, offset`                  | Load from stack at offset                      |
| `stack_store` | `src, offset`                   | Store to stack at offset                       |
| `stack_sub`   | `dest, offset, bytes`           | Sub-stack address arithmetic                   |

### I/O & Modules

| Instruction     | Operands                 | Description                                         |
|-----------------|--------------------------|-----------------------------------------------------|
| `print`         | `fmt, args…`             | Print formatted output                              |
| `getline`       | `ptr`                    | Read a line from stdin into buffer                  |
| `invoke`        | `func, args…`            | Call an external (module) function                  |
| `call`          | `Module.Func`            | Call an internal MXVM function                      |
| `ret`           | —                        | Return from MXVM function                           |
| `done`          | —                        | End program                                         |
| `exit`          | `code`                   | Terminate VM with status                            |

### Conversion & Misc

| Instruction  | Operands          | Description                         |
|--------------|-------------------|-------------------------------------|
| `to_int`     | `dest, ptr`       | Convert string → integer            |
| `to_float`   | `dest, ptr`       | Convert string → float              |
---

## Module System
Place compiled “.so”/“.dll” modules in `modules/`. Use:

```mxvm
section module { io, string, math }
// …
invoke fopen, filename, mode
invoke strlen, mystr
```

---

## Project Structure

```
MXVM-main/
├── CMakeLists.txt        # Build rules
├── LICENSE               # Apache-2.0
├── README.md             # (this file)
├── about.html            # Browser demo & docs
├── include/mxvm/         # Public headers
├── src/                  # VM + codegen implementation
├── modules/              # Built-in extension modules
└── mxvm_src/             # Example .mxvm programs
```

---

## Building

```bash
git clone https://github.com/lostjared/MXVM.git
cd MXVM
mkdir build && cd build
cmake ..
make
```

- Requires a C++20 compiler, CMake 3.10+, and standard build tools.
- Optional modules (e.g. WebAssembly) may need Emscripten or additional SDKs.

## Running

- **Interpreter mode**  
  ```bash
  mxvmc ../mxvm_src/hello_world.mxvm --path /usr/local/lib
  ```
- **Compile ➔ to Assemlby**  
  ```bash
  mxvmc ../mxvm_src/fibonacci.mxvm --path /usr/local/lib --object-path . --action translate
  ```

---

## Contributing

1. Fork the repo  
2. Create a branch (`git checkout -b feature/foo`)  
3. Commit your changes (`git commit -am "Add feature"`)  
4. Push (`git push origin feature/foo`)  
5. Open a Pull Request  

---



