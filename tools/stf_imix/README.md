# stf_imix

Counts the instruction mix in a trace, breaking down by Mavis instruction class or mnemonic

```
usage: stf_imix [options] <trace>
    -h, --help                      show this message
    -V, --version                   show the STF version the tool is built with
    -o <output>                     output file (defaults to stdout)
    -s                              sort output
    -m                              categorize by mnemonic
    -e                              categorize by ISA extension
    -w <warmup>                     number of warmup instructions
    -u                              only count user-mode instructions
    -r <run_length>                 limit to the first run_length instructions (includes warmup). Default is 0, for no limit.
    -T                              only include region of interest between tracepoints. If specified, the -w and -r arguments apply to the ROI between tracepoints.
    --roi-start-opcode <opcode>     override the tracepoint ROI start opcode
    --roi-stop-opcode <opcode>      override the tracepoint ROI stop opcode
    --roi-start-pc <opcode>         start ROI at specified PC instead of a tracepoint opcode
    --roi-stop-pc <opcode>          stop ROI at specified PC instead of a tracepoint opcode
    <trace>                         trace in STF format
```
