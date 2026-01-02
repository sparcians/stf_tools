# stf_function_histogram

Counts how many times each function is called in a trace

> [!NOTE]
> Requires an ELF compiled with debug info

```
usage: stf_function_histogram [options] <trace>
    -h, --help            show this message
    -V, --version         show the STF version the tool is built with
    -E <elf>              ELF file to analyze (defaults to trace.elf)
    -u                    skip non user-mode instructions
    -e <end_insts>        stop after specified number of instructions
    -p                    profile functions in program
    -s <warmup_insts>     Skip the the specified warmup instructions
    <trace>               trace in STF format
```
