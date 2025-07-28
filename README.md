# MXVM

MXVM is a custom virtual machine and compiler/interpreter project designed as a personal learning tool. The goal of this project is to help me gain hands-on experience with building a VM, designing an instruction set, writing a parser, and integrating with C++ build systems and module architectures.

## Project Purpose

This project is **not** intended for production use. Instead, it serves as a sandbox for experimenting with:
- Virtual machine architecture and instruction execution
- Code parsing and AST generation
- Modular design with dynamic loading of external functions
- C++ code organization, including header/source separation and CMake integration
- Cross-platform build and dependency management
- Debugging, code generation, and memory management

## Features

- **Custom VM**: Implements a set of instructions (arithmetic, control flow, stack, string, etc.) in C++.
- **Modular System**: Supports loading external modules (e.g., string and IO libraries) via shared libraries.
- **Parser and AST**: Parses a custom language into an abstract syntax tree and intermediate code.
- **Code Generation**: Generates x86_64 assembly output for Linux targets.
- **Debugging Tools**: Includes memory dumps, HTML debug output, and verbose error reporting.
- **CMake Build**: Uses CMake for dependency checks, platform-specific configuration, and modular builds.
