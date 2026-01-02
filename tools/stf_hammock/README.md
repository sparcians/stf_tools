# stf_hammock

Lists hammock branches in a trace

```
usage: stf_hammock [options] <trace>
    -h, --help              show this message
    -V, --version           show the STF version the tool is built with
    -H <hammock_length>     How many instructions can be skipped in the hammock. Defaults to 1.
    -e <elf>                ELF file to search for always-skipped instructions
    -b <bin>                Binary file to search for always-skipped instructions
    -o <offset>             VA offset for raw binary files. Defaults to 0x80000000.
    -d                      only report dynamic branches (branches that are not always-taken or never-taken)
    -u                      skip non user-mode instructions
    -D                      show disassembly of hammocks
    -j                      enable JSON output
    <trace>                 trace in STF format
```
