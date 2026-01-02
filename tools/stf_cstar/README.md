# stf_cstar

Counts all possible conditional operations (conditional forward branch over 1 instruction) in a trace

> [!NOTE]
> Requires an ELF or flat binary for full analysis

```
usage: stf_cstar [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -e <elf>          ELF file to search for always-skipped instructions
    -b <bin>          Binary file to search for always-skipped instructions
    -o <offset>       VA offset for raw binary files. Defaults to 0x80000000.
    -u                skip non user-mode instructions
    <trace>           trace in STF format
```
