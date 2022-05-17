# Trace Tools

This is a repo of trace dumping and analysis tools based on the STF format.

These are built for RISC-V use, but can be augmented for other architectures.

## Required Packages

Install all packages needed to build [stf_lib](https://github.com/sparcians/stf_lib).

## Building

```
mkdir release
cd release
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```
Or for debug:
```
mkdir debug
cd debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

## RISC-V Binutils Disassembly Support

[Mavis](https://github.com/sparcians/mavis) is the default disassembly backend. You can optionally use the [RISC-V binutils backend](https://github.com/riscv-collab/riscv-binutils-gdb) instead.

The binutils disassembly backend can be selected by setting the environment variable `STF_DISASM=BINUTILS`.

Building binutils can optionally be disabled by running `cmake` with `-DDISABLE_BINUTILS=1`.
