# stf_dump

Dumps an instruction trace in a human-readable format

```
usage: stf_dump [options] <trace>
    -h, --help                      show this message
    -V, --version                   show the STF version the tool is built with
    -c                              concise mode - only dumps PC information and disassembly
    -u                              only dump user-mode instructions
    -p                              show physical addresses
    -P                              show the PTE entries
    -A                              use aliases for disassembly (only used by binutils disassembler)
    -m                              Enables cross checking trace instruction opcode against opcode from symbol table file <*_symTab.yaml>. Applicable only when '-y' flag is enabled
    -s <N>                          start dumping at N-th instruction
    -e <M>                          end dumping at M-th instruction
    -y <*_symTab.yaml>              YAML symbol table file to show annotation
    -H                              omit the header information
    -T                              only include region of interest between tracepoints. If specified, the -s and -e arguments apply to the ROI between tracepoints.
    --roi-start-opcode <opcode>     override the tracepoint ROI start opcode
    --roi-stop-opcode <opcode>      override the tracepoint ROI stop opcode
    --roi-start-pc <opcode>         start ROI at specified PC instead of a tracepoint opcode
    --roi-stop-pc <opcode>          stop ROI at specified PC instead of a tracepoint opcode
    <trace>                         trace in STF format
```
