# MXVM -- Virtual Machine, Compiler & Code Generator {#mainpage}

**MXVM** is a custom virtual machine and compiler suite written in modern C++20.
It provides a RISC-style bytecode language with an interpreter, a Pascal-subset
compiler frontend, and native x86-64 code generators targeting Linux (System V ABI),
macOS (Darwin), and Windows (Win64).

> **Experimental / educational project -- not intended for production use.**

> **[Download PDF Reference Manual](https://lostsidedead.biz/MXVM/doc/MXVM_Reference.pdf)**

---

## Architecture Overview

| Layer | Component | Description |
|-------|-----------|-------------|
| **Frontend** | Scanner / Lexer | Tokenises `.mxvm` and `.pas` source files |
| **Frontend** | Parser / AST | Builds a typed abstract syntax tree |
| **Frontend** | Validator | Scope-based semantic analysis and type checking |
| **Middle** | Code Generator | Emits MXVM bytecode from the AST |
| **Backend** | Interpreter | Stack-based execution engine for bytecode |
| **Backend** | x86-64 Codegen | Native assembly output (System V, Win64, Darwin) |
| **Backend** | Peephole Optimizer | Post-generation assembly cleanup passes |
| **Runtime** | Module System | Dynamically loaded C shared libraries (`io`, `std`, `string`, `sdl`) |

@dot
digraph pipeline {
    rankdir=TB;
    node [shape=box, style=filled, fillcolor="#E8F0FE", fontname="Helvetica", fontsize=12];
    edge [color="#555555"];

    source [label=".pas / .mxvm source", shape=note, fillcolor="#FFF9C4"];
    scanner [label="Scanner"];
    parser [label="Parser"];
    validator [label="Validator"];
    codegen [label="CodeGen (IR)", fillcolor="#D4EDDA"];
    interp [label="Interpreter", fillcolor="#F8D7DA"];
    sysv [label="SysV x64", fillcolor="#F8D7DA"];
    win64 [label="Win64 x64", fillcolor="#F8D7DA"];

    source -> scanner;
    scanner -> parser;
    parser -> validator;
    validator -> codegen;
    codegen -> interp;
    codegen -> sysv;
    codegen -> win64;
}
@enddot

---

## Building

```bash
git clone https://github.com/lostjared/MXVM.git
cd MXVM && mkdir build && cd build
cmake .. && make -j$(nproc)
```

Requires: C++20 compiler, CMake >= 3.10.\n
Optional: SDL2 + SDL2_ttf (for the SDL module), Emscripten (WebAssembly target).

---

## Running

| Mode | Command |
|------|---------|
| **Interpret** | `mxvmc program.mxvm --path /usr/local/lib` |
| **Compile -> Assembly** | `mxvmc program.mxvm --path /usr/local/lib --action translate` |
| **Compile -> Executable** | `mxvmc program.mxvm --path /usr/local/lib --action compile` |

---

# MXVM Bytecode Language Reference {#bytecode}

## Program Structure

Every `.mxvm` file is wrapped in a `program` (or `object`) block containing
three sections: **data**, **module**, and **code**.

```
program MyApp {

  section data {
    int    counter = 0
    float  pi      = 3.14159
    string msg     = "Hello, MXVM!\n"
    ptr    buffer  = null
    byte   flags   = 0
  }

  section module { io, string, std }

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

### Data Section -- Variable Types

| Type | Size | Description |
|------|------|-------------|
| `int` | 64-bit | Signed integer |
| `float` | 64-bit | Double-precision floating point |
| `string` | varies | NUL-terminated string with optional max-length: `string name, 256` |
| `ptr` | 64-bit | Opaque pointer (for `alloc`/module use) |
| `byte` | 8-bit | Unsigned byte |
| `label` | -- | Code address (for indirect jumps) |

- Variables are declared with an optional initialiser: `int x = 42`.
- All string literals must live in the data section -- inline string constants
  are **not** allowed in the code section.
- Hex immediates are supported in code: `mov x, 0xFF`.

### Module Section

```
section module { io, string, std }
```

Lists the C shared-library modules whose functions may be called via `invoke`.
See the @ref modules "Module Reference" below.

### Code Section

The code section is a flat sequence of labeled instructions.

- **One instruction per line.**
- **Destination-first operand order** (RISC convention): `add dest, src1, src2`.
- Labels end with `:` and may be marked as functions with `function`:
  ```
  my_func: function
      ; ... body ...
      ret
  ```

---

## Instruction Set {#instructions}

### Arithmetic & Logic

| Instruction | Operands | Effect |
|-------------|----------|--------|
| `mov` | `dest, src` | `dest <- src` |
| `add` | `dest, a, b` | `dest <- a + b` (2-operand form: `dest += a`) |
| `sub` | `dest, a, b` | `dest <- a - b` |
| `mul` | `dest, a, b` | `dest <- a * b` |
| `div` | `dest, a, b` | `dest <- a / b` (integer division for int operands) |
| `mod` | `dest, a, b` | `dest <- a mod b` |
| `neg` | `dest, a` | `dest <- -a` |
| `or`  | `dest, a, b` | Bitwise OR |
| `and` | `dest, a, b` | Bitwise AND |
| `xor` | `dest, a, b` | Bitwise XOR |
| `not` | `dest, a` | Bitwise NOT |

### Comparison & Branching

| Instruction | Operands | Effect |
|-------------|----------|--------|
| `cmp` | `a, b` | Set flags from `a - b` |
| `fcmp` | `a, b` | Floating-point compare, set flags |
| `jmp` | `label` | Unconditional jump |
| `je` / `jne` | `label` | Jump if equal / not equal |
| `jl` / `jle` | `label` | Jump if less / less-or-equal (signed) |
| `jg` / `jge` | `label` | Jump if greater / greater-or-equal (signed) |
| `jz` / `jnz` | `label` | Jump if zero flag set / clear |
| `ja` / `jb` | `label` | Jump if above / below (unsigned) |
| `jae` / `jbe` | `label` | Jump if above-or-equal / below-or-equal (unsigned) |
| `jc` / `jnc` | `label` | Jump if carry / no carry |
| `jp` / `jnp` | `label` | Jump if parity / no parity |
| `jo` / `jno` | `label` | Jump if overflow / no overflow |
| `js` / `jns` | `label` | Jump if sign / no sign |

### Memory & Pointers

| Instruction | Operands | Effect |
|-------------|----------|--------|
| `load` | `dest, ptr, index, size` | `dest <- *(ptr + index * size)` |
| `store` | `src, ptr, index, size` | `*(ptr + index * size) <- src` |
| `lea` | `dest, base, offset` | Load effective address |
| `alloc` | `ptr, elem_size, count` | `ptr <- calloc(count, elem_size)` |
| `free` | `ptr` | Release allocated memory |

### Stack Operations

| Instruction | Operands | Effect |
|-------------|----------|--------|
| `push` | `src` | Push value onto the VM stack |
| `pop` | `dest` | Pop top of stack into `dest` |
| `stack_load` | `dest, offset` | Read stack slot at `offset` |
| `stack_store` | `src, offset` | Write to stack slot at `offset` |
| `stack_sub` | `amount` | Reserve stack space |

### Control Flow & Calls

| Instruction | Operands | Effect |
|-------------|----------|--------|
| `call` | `label` | Call an internal function (or `Object.Function`) |
| `ret` | -- | Return from function |
| `invoke` | `func, args...` | Call an external module function (C ABI) |
| `return` | `var` | Capture the return value of the last `invoke` |
| `done` | -- | Normal program termination |
| `exit` | `code` | Terminate with exit code |

### I/O

| Instruction | Operands | Effect |
|-------------|----------|--------|
| `print` | `fmt, args...` | Printf-style formatted output |
| `string_print` | `ptr` | Print raw string at pointer |
| `getline` | `ptr` | Read a line from stdin into buffer |

### Type Conversion

| Instruction | Operands | Effect |
|-------------|----------|--------|
| `to_int` | `dest, ptr` | Parse string -> integer |
| `to_float` | `dest, ptr` | Parse string -> float |

### Calling Convention -- `invoke` / `return`

External functions loaded from modules are called with `invoke`.
The return value is captured into a variable with the `return` pseudo-instruction
immediately following the `invoke`:

```
invoke fopen, filename, mode
return file_handle

invoke strlen, my_string
return len
```

---

## Object System {#objects}

MXVM supports **objects** -- separately compiled units that can be
linked into a main program.

```
object Utils {
  section data { ... }
  section code {
    print_line: function
        print fmt
        ret
  }
}
```

The main program references object functions with dot notation:

```
call Utils.print_line
```

Objects can be **inline** (defined in the same file) or loaded from separate files
via `--object-path`.

---

# MXVM Pascal Frontend {#pascal}

MXVM includes a **Pascal-subset compiler** that parses Pascal source code,
builds a typed AST, validates semantics with scope-based analysis, and emits
MXVM bytecode.

## Supported Language Features

### Program Structure

```pascal
program HelloWorld;
uses io, std, string;

const
  MAX = 100;

type
  PInt = ^integer;
  Point = record
    x: integer;
    y: integer;
  end;
  IntArray = array[0..9] of integer;

var
  i: integer;
  name: string;
  flag: boolean;
  pt: Point;
  nums: IntArray;
  p: PInt;

procedure Greet(msg: string);
begin
  writeln(msg);
end;

function Add(a, b: integer): integer;
begin
  Add := a + b;
end;

begin
  name := 'MXVM Pascal';
  writeln('Hello from ', name);
  writeln('Sum = ', Add(3, 4));
  Greet('Welcome!');
end.
```

### Data Types

| Type | Description |
|------|-------------|
| `integer` | 64-bit signed integer |
| `real` | 64-bit double-precision float |
| `boolean` | Stored as integer (0 or 1); `true` / `false` literals |
| `char` | Stored as integer (character ordinal value) |
| `string` | Heap-managed, pointer-based string |
| `^Type` | Typed pointer (e.g. `^integer`, `^Point`) |
| `array[lo..hi] of T` | Fixed-size array with arbitrary integer bounds |
| `record ... end` | Named record (struct) with typed fields |

### Constants

```pascal
const
  MAX     = 100;
  PI      = 3.14159;
  GREETING = 'Hello';
  ENABLED = true;
```

Built-in constants: `true`, `false`, `maxint`, `nil`.

### Type Declarations

```pascal
type
  Counter   = integer;         { type alias }
  PInt      = ^integer;        { pointer type }
  IntArray  = array[1..10] of integer;
  Matrix    = array[0..3] of array[0..3] of real;  { multi-dimensional }
  Point = record
    x: integer;
    y: integer;
  end;
```

### Variable Declarations

```pascal
var
  a, b, c: integer;           { multiple identifiers }
  name: string;
  pi: real;
  items: array[0..99] of integer;
  pt: Point;
  p: ^integer;
```

Variables may include initialisers: `var x: integer = 42;`

### Operators

| Category | Operators |
|----------|-----------|
| **Arithmetic** | `+`  `-`  `*`  `/`  `div`  `mod` |
| **Relational** | `=`  `<>`  `<`  `<=`  `>`  `>=` |
| **Logical** | `and`  `or`  `not` |
| **String** | `+` (concatenation) |
| **Pointer** | `^` (dereference, postfix), `@` (address-of, prefix) |
| **Unary** | `+`  `-`  `not` |

### Control Flow

```pascal
{ If-Then-Else }
if x > 0 then
  writeln('positive')
else
  writeln('non-positive');

{ While }
while i < 10 do
begin
  writeln(i);
  i := i + 1;
end;

{ For }
for i := 1 to 10 do
  writeln(i);

for i := 10 downto 1 do
  writeln(i);

{ Repeat-Until }
repeat
  readln(x);
until x = 0;

{ Case }
case ch of
  1, 2, 3: writeln('small');
  4..6:    writeln('medium');
else
  writeln('other');
end;

{ Flow control }
break;       { exit loop }
continue;    { next iteration }
exit;        { exit current scope }
```

### Procedures & Functions

```pascal
procedure Swap(var a, b: integer);
var tmp: integer;
begin
  tmp := a;
  a := b;
  b := tmp;
end;

function Factorial(n: integer): integer;
begin
  if n <= 1 then
    Factorial := 1
  else
    Factorial := n * Factorial(n - 1);
end;
```

- **Parameters**: by value (default) or by reference (`var` keyword).
- **Return values**: assigned by writing to the **function name** (`Factorial := ...`).
- **Nested** procedures and functions are supported.
- **Recursion** is fully supported.

### Records

```pascal
type
  Point = record
    x: integer;
    y: integer;
  end;

var pt: Point;
begin
  pt.x := 10;
  pt.y := 20;
  writeln(pt.x, ', ', pt.y);
end.
```

Records may contain fields of any type including arrays, strings,
pointers, and other record types.

### Arrays

```pascal
var
  nums: array[0..9] of integer;
  grid: array[0..3] of array[0..3] of real;
begin
  nums[0] := 42;
  grid[1][2] := 3.14;
end.
```

- Arbitrary integer bounds (not restricted to 0-based).
- Multi-dimensional arrays via nested `array[...] of array[...]`.
- Runtime bounds checking is enabled by default.

### Pointers

```pascal
type PInt = ^integer;
var
  p: PInt;
  x: integer;
begin
  new(p);          { allocate }
  p^ := 42;        { dereference and assign }
  x := p^;         { dereference and read }
  dispose(p);      { free }

  p := @x;          { address-of }
end.
```

### String Operations

```pascal
var
  s: string;
  n: integer;
begin
  s := 'Hello' + ' World';       { concatenation }
  n := length(s);                 { length }
  writeln(copy(s, 1, 5));         { substring }
  writeln(pos('World', s));       { search }
  writeln(inttostr(42));          { int -> string }
  writeln(strtoint('123'));       { string -> int }
end.
```

### Built-in Procedures & Functions

#### I/O

| Routine | Description |
|---------|-------------|
| `write(args...)` | Print values (no trailing newline) |
| `writeln(args...)` | Print values with trailing newline |
| `readln(var)` | Read a line from stdin; auto-converts to int/float |

#### Math

`abs`, `sqrt`, `pow`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`,
`sinh`, `cosh`, `tanh`, `exp`, `log`, `log10`, `log2`, `exp2`,
`fmod`, `ceil`, `floor`, `fabs`, `round`, `trunc`, `hypot`, `atan2`

#### Conversion

`float()`, `float_to_int()`, `int_to_float()`, `atoi()`, `atof()`

#### Character Classification

`toupper()`, `tolower()`, `isalpha()`, `isdigit()`, `isspace()`

#### String Operations

`length()`, `pos()`, `copy()`, `insert()`, `delete()`, `inttostr()`, `strtoint()`

#### Memory

`new(ptr)`, `dispose(ptr)`, `malloc()`, `calloc()`, `free()`,
`memcpy()`, `memcmp()`, `memmove()`, `memset()`

#### System

`halt(code)`, `system(cmd)`, `rand()`, `srand()`,
`seed_random()`, `rand_number(max)`, `argc()`, `argv(index)`

---

## Limitations & Known Quirks {#limitations}

The Pascal frontend implements a practical subset of standard Pascal.
The following limitations apply:

| Area | Limitation |
|------|------------|
| **Function calls** | Functions **require parentheses** even when called with no arguments: `x := MyFunc()` -- not `x := MyFunc`. |
| **No units/modules** | Only single-file `program` compilation. No `unit`, `interface`, `implementation` keywords. |
| **No enumerated types** | Only scalar types, records, arrays, and pointers. |
| **No dynamic arrays** | Only static `array[lo..hi]` with compile-time bounds. |
| **No variant records** | Records have flat fields only -- no `case` variant parts. |
| **No sets** | The `set` keyword is reserved but not implemented. |
| **No `file` type** | File I/O is done via module functions (`fopen`, `fread`, etc.), not Pascal `file of`. |
| **No `with` statement** | The keyword is reserved but code generation is not implemented. |
| **No `goto`** | `label` and `goto` are reserved but not implemented. |
| **No `packed`** | Reserved but has no effect. |
| **No `in` operator** | Reserved but not implemented (no set support). |
| **String escapes** | Strings use C-style escapes (`\n`, `\t`, `\\`, `\"`) rather than Pascal doubled-quote convention. |
| **Return convention** | Functions return values by assigning to the function name (`Func := val`), not via a `result` variable. |
| **`forward`** | Parsed and validated but codegen depends on declaration order. |
| **Operator precedence** | Follows standard Pascal precedence: `not` > `* / div mod and` > `+ - or` > relational. |

---

# Runtime Module Reference {#modules}

External functions are called from bytecode with `invoke func, args...` and
from Pascal with regular function-call syntax. Return values are captured
with `return var` (bytecode) or assigned normally (Pascal).

## Module: io {#mod_io}

File I/O, random numbers.

| Function | Params | Returns | Description |
|----------|--------|---------|-------------|
| `fopen` | 2 | POINTER | Open a file (filename, mode). Returns `FILE*` handle. |
| `fclose` | 1 | INTEGER | Close a file handle. |
| `fread` | 4 | INTEGER | Read from file into buffer (dst, size, count, fh). |
| `fwrite` | 4 | INTEGER | Write buffer to file (src, size, count, fh). |
| `fseek` | 3 | INTEGER | Seek within file (fh, offset, whence). |
| `fsize` | 1 | INTEGER | Get file size in bytes. |
| `fprintf` | >= 2 | -- | Formatted write to file (fh, fmt, ...). |
| `rand_number` | 1 | INTEGER | Random integer in [0, max). |
| `seed_random` | 0 | INTEGER | Seed RNG with current time. |

## Module: std {#mod_std}

Standard library -- math, memory, string conversion, system calls.

### Math Functions

| Function | Params | Returns |
|----------|--------|---------|
| `abs` | 1 | INTEGER |
| `fabs` | 1 | FLOAT |
| `sqrt`, `exp`, `exp2`, `log`, `log10`, `log2` | 1 | FLOAT |
| `sin`, `cos`, `tan`, `asin`, `acos`, `atan` | 1 | FLOAT |
| `sinh`, `cosh`, `tanh` | 1 | FLOAT |
| `ceil`, `floor`, `round`, `trunc` | 1 | FLOAT |
| `pow`, `fmod`, `atan2`, `hypot` | 2 | FLOAT |

### Memory Functions

| Function | Params | Returns | Description |
|----------|--------|---------|-------------|
| `malloc` | 1 | POINTER | Allocate bytes. |
| `calloc` | 2 | POINTER | Allocate zeroed memory (count, size). |
| `free` | 1 | -- | Release pointer. |
| `memcpy` | 3 | POINTER | Copy memory (dest, src, n). |
| `memmove` | 3 | POINTER | Move memory (dest, src, n). |
| `memset` | 3 | POINTER | Fill memory (dest, byte, n). |
| `memcmp` | 3 | INTEGER | Compare memory (a, b, n). |

### Conversion & Classification

| Function | Params | Returns | Description |
|----------|--------|---------|-------------|
| `atoi` | 1 | INTEGER | String -> int. |
| `atof` | 1 | FLOAT | String -> float. |
| `toupper`, `tolower` | 1 | INTEGER | Case conversion. |
| `isalpha`, `isdigit`, `isspace` | 1 | INTEGER | Character classification. |

### System

| Function | Params | Returns | Description |
|----------|--------|---------|-------------|
| `system` | 1 | INTEGER | Execute shell command. |
| `exit` | 1 | -- | Terminate program. |
| `rand` | 0 | INTEGER | Random integer. |
| `srand` | 1 | -- | Seed random. |
| `argc` | 0 | INTEGER | Argument count. |
| `argv` | 1 | POINTER | Argument string by index. |

## Module: string {#mod_string}

String manipulation.

| Function | Params | Returns | Description |
|----------|--------|---------|-------------|
| `strlen` | 1 | INTEGER | String length. |
| `strcmp` | 2 | INTEGER | Compare two strings. Returns <0, 0, >0. |
| `strncpy` | 3 | INTEGER | Copy up to n characters. |
| `strncat` | 3 | INTEGER | Append up to n characters. |
| `snprintf` | >= 4 | INTEGER | Printf into buffer (dest, size, fmt, ...). |
| `strfind` | 3 | INTEGER | Find substring (haystack, needle, start). Returns index or -1. |
| `substr` | 5 | INTEGER | Extract substring (dest, maxsize, src, pos, len). |
| `strat` | 2 | INTEGER | Character code at index. |

## Module: sdl {#mod_sdl}

SDL2 / SDL2_ttf bindings for graphics, events, audio, and text rendering.

### Core & Window

| Function | Description |
|----------|-------------|
| `init` | Initialise SDL2 subsystems. |
| `quit` | Shut down SDL2. |
| `create_window` | Create a window (title, x, y, w, h, flags). |
| `destroy_window` | Destroy a window. |
| `set_window_title` | Set window title text. |
| `set_window_position` | Reposition a window. |
| `get_window_size` | Query window dimensions. |
| `set_window_fullscreen` | Toggle fullscreen mode. |
| `set_window_icon` | Set window icon from image file. |

### Renderer & Drawing

| Function | Description |
|----------|-------------|
| `create_renderer` | Create a hardware renderer. |
| `destroy_renderer` | Destroy a renderer. |
| `set_draw_color` | Set RGBA draw colour. |
| `clear` | Clear with current draw colour. |
| `present` | Flip back buffer to screen. |
| `draw_point` | Draw a single pixel. |
| `draw_line` | Draw a line. |
| `draw_rect` | Draw a rectangle outline. |
| `fill_rect` | Draw a filled rectangle. |

### Textures & Surfaces

| Function | Description |
|----------|-------------|
| `create_texture` | Create a blank texture. |
| `destroy_texture` | Destroy a texture. |
| `load_texture` | Load texture from image file. |
| `render_texture` | Render src rect -> dst rect. |
| `update_texture` | Upload pixel data to texture. |
| `lock_texture` / `unlock_texture` | Direct pixel access. |
| `create_rgb_surface` / `free_surface` | Surface management. |
| `blit_surface` | Blit surface to surface. |
| `create_render_target` / `set_render_target` / `destroy_render_target` | Off-screen render targets. |
| `present_scaled` / `present_stretched` | Present render target with scaling. |

### Events & Input

| Function | Description |
|----------|-------------|
| `poll_event` | Poll for pending event. |
| `get_event_type` | Event type of last polled event. |
| `get_key_code` | Key code from keyboard event. |
| `get_mouse_x` / `get_mouse_y` | Mouse coordinates. |
| `get_mouse_button` | Mouse button from event. |
| `get_mouse_state` | Current mouse position + buttons. |
| `get_relative_mouse_state` | Relative mouse delta. |
| `is_key_pressed` | Check if a scancode is pressed. |
| `get_keyboard_state` | Full keyboard state array. |
| `set_clipboard_text` / `get_clipboard_text` | Clipboard access. |
| `show_cursor` | Show or hide cursor. |

### Timing

| Function | Description |
|----------|-------------|
| `get_ticks` | Milliseconds since SDL init. |
| `delay` | Sleep for N milliseconds. |

### Audio

| Function | Description |
|----------|-------------|
| `open_audio` | Open audio device (freq, format, channels, samples). |
| `close_audio` | Close audio device. |
| `pause_audio` | Pause or resume playback. |
| `load_wav` | Load WAV file. |
| `free_wav` | Free loaded WAV buffer. |
| `queue_audio` | Queue audio data for playback. |
| `get_queued_audio_size` | Bytes of queued audio. |
| `clear_queued_audio` | Flush audio queue. |

### Text Rendering (SDL_ttf)

| Function | Description |
|----------|-------------|
| `init_text` | Initialise TTF subsystem. |
| `quit_text` | Shut down TTF. |
| `load_font` | Load a TrueType font (path, pt_size). |
| `draw_text` | Render text (renderer, font, text, x, y, r, g, b, a). |

---

# Code Generation & Optimisation {#codegen}

## Native x86-64 Targets

| Platform | ABI | Entry Point | Notes |
|----------|-----|-------------|-------|
| **Linux** | System V | `_start` / `main` | Default target. Uses `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9` for args. |
| **macOS** | System V (Darwin) | `_main` | Underscore-prefixed symbols. PIC relocations. |
| **Windows** | Win64 | `main` | Uses `rcx`, `rdx`, `r8`, `r9` + shadow space. |

## Register Allocation

The code generator performs a register allocation pass that maps
frequently-used variables to callee-saved registers:

- **System V**: `r12`-`r15`, `rbx` (integer); `xmm6`-`xmm15` (float).
- **Win64**: `r12`-`r15`, `rbx`, `rdi`, `rsi` (integer); extended XMM set.

Variables not assigned to registers are kept in memory (BSS/data section)
and accessed via RIP-relative addressing.

## Peephole Optimiser

A post-generation pass (`gen_optimize`) applies pattern-based rewrites:

- **Redundant load elimination**: removes `mov reg, [var]` immediately
  followed by `mov [var], reg` (and vice versa).
- **Dead store removal**: eliminates stores to variables that are
  immediately overwritten.
- **Strength reduction**: replaces multiply/divide by powers of 2
  with shift instructions.
- **Redundant move elimination**: removes `mov rax, rax` and similar
  no-op register-to-register moves.

---

# Examples {#examples}

## MXVM Bytecode -- Hello World

```
program HelloWorld {
  section data {
    string msg = "Hello, World!\n"
  }
  section code {
    print msg
    done
  }
}
```

## MXVM Bytecode -- Fibonacci

```
program Fibonacci {
  section data {
    int a = 0
    int b = 1
    int temp = 0
    int count = 0
    string fmt = "%d\n"
  }
  section code {
  loop:
    print fmt, a
    mov temp, b
    add b, a, b
    mov a, temp
    add count, count, 1
    cmp count, 20
    jl loop
    done
  }
}
```

## Pascal -- Recursive Factorial

```pascal
program FactorialDemo;

function Factorial(n: integer): integer;
begin
  if n <= 1 then
    Factorial := 1
  else
    Factorial := n * Factorial(n - 1);
end;

var i: integer;
begin
  for i := 1 to 12 do
    writeln(i, '! = ', Factorial(i));
end.
```

## Pascal -- Records & Pointers

```pascal
program RecordDemo;

type
  Point = record
    x: integer;
    y: integer;
  end;

var
  p: ^Point;

begin
  new(p);
  p^.x := 10;
  p^.y := 20;
  writeln('Point: (', p^.x, ', ', p^.y, ')');
  dispose(p);
end.
```

---

# Project Layout {#layout}

```
MXVM/
|---- include/mxvm/          Core VM headers (parser, icode, instruct, etc.)
|---- src/                    Implementation sources
|   |---- frontend/           Pascal parser, validator, AST, codegen
|   |   `---- include/        Frontend-specific headers
|   |---- scanner/            Tokeniser / lexer
|   |   `---- include/scanner/ Scanner headers
|   |---- vm/                 Interpreter, argument parsing
|   |   `---- include/        VM headers (argz.hpp)
|   `---- webasm/             WebAssembly target (experimental)
|---- modules/                Runtime C modules
|   |---- io/                 File I/O, random numbers
|   |---- std/                Math, memory, conversion, system
|   |---- string/             String manipulation
|   `---- sdl/                SDL2 + SDL_ttf bindings
|---- docs/                   HTML reference pages
|---- mxvm_src/               Example .mxvm programs
|---- CMakeLists.txt          Top-level build script
`---- Doxyfile                Doxygen configuration
```

---

*Generated documentation -- see the [source repository](https://github.com/lostjared/MXVM) for the latest code.*
