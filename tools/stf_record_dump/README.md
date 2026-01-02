# stf_record_dump

Dumps the individual records in a trace

```
usage: stf_record_dump [options] <trace>
    -h, --help             show this message
    -V, --version          show the STF version the tool is built with
    -u                     only dump user-mode instructions
    -p                     show physical addresses
    -P                     show the PTE entries
    -A                     use aliases for disassembly
    -m                     Enables cross checking trace instruction opcode against opcode from symbol table file <*_symTab.yaml>. Applicable only when '-y' flag is enabled
    -s <N>                 start dumping at N-th instruction
    -e <M>                 end dumping at M-th instruction
    -S <N>                 start dumping at N-th record
    -E <M>                 end dumping at M-th record
    -y <*_symTab.yaml>     YAML symbol table file to show annotation
    <trace>                trace in STF format
```
