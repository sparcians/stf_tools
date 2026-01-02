# stf_cmov

Counts conditional move fusion opportunities in a trace

> [!NOTE]
> Requires an ELF or flat binary for full analysis

```
usage: stf_cmov [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -e <elf>          ELF file to search for always-skipped move instructions
    -b <bin>          Binary file to search for always-skipped move instructions
    -o <offset>       VA offset for raw binary files. Defaults to 0x80000000.
    -d                only report dynamic branches (branches that are not always-taken or never-taken)
    -u                skip non user-mode instructions
    -D                show disassembly of branch/mov pairs
    <trace>           trace in STF format
```
