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

## RISC-V Toolchain Disassembly Support

[Mavis](https://github.com/sparcians/mavis) is the default disassembly backend. You can optionally use the [RISC-V toolchain](https://github.com/riscv/riscv-gnu-toolchain) instead.

If you are using the trace-env Docker image, the toolchain will be found automatically:
```
mkdir release
cd release
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```
If you have a centrally-installed toolchain with its own `include` and `lib` directories, you can instead do the following:
```
mkdir release
cd release
cmake .. -DCMAKE_BUILD_TYPE=Release -DRISCV_INSTALL_DIR=/path/to/riscv-toolchain-install
make
```
Otherwise, you'll need to provide $RISCV_TOOLCHAIN to point to the build repository of the toolchain (not the install). Once you have the RISC-V toolchain, you can start a build of these tools:
```
mkdir release
cd release
cmake .. -DCMAKE_BUILD_TYPE=Release -DRISCV_TOOLCHAIN=/path/to/riscv-gnu-toolchain
make
```
**NOTE:** Your RISC-V toolchain is assumed to have been built in the top-level of the repo.
If you used a build directory, you will need to modify some of the paths in `cmake/FindDisassemblers.cmake`.

When the trace tools are built with this support, the binutils disassembly backend can be selected by setting the environment variable `STF_DISASM=BINUTILS`.
