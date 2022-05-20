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

OS X users may need to add the following to their `cmake` invocation if Python was installed via Homebrew:
```
-DPYTHON_LIBRARY=$(python3-config --prefix)/lib/libpython3.8.dylib -DPYTHON_INCLUDE_DIR=$(python3-config --prefix)/include/python3.8
```

## RISC-V Disassembly Support

[RISC-V binutils](https://github.com/riscv-collab/riscv-binutils-gdb) is the default disassembly backend. You can optionally use the [Mavis](https://github.com/sparcians/mavis) backend instead.

Different RISC-V ISA extensions can be activated by setting the `STF_DISASM_ISA` environment variable. It defaults to `rv64gcv_zba_zbb`.

The Mavis disassembly backend can be selected by setting the environment variable `STF_DISASM=MAVIS`.

Building binutils can optionally be disabled by running `cmake` with `-DDISABLE_BINUTILS=1`. In this case the tools will automatically default to using Mavis.
