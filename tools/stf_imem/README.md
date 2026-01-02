# stf_imem

Reconstitutes a trace into assembly code blocks with trip counts

```
usage: stf_imem [options] <trace>
    -h, --help                      show this message
    -V, --version                   show the STF version the tool is built with
    -p                              show the inst count percentage
    -A                              use aliases for disassembly
    -s <n>                          skip the first n instructions
    -t <n>                          Count all records with thread ID <n>
    -g <n>                          Count all records with process ID <n>
    -c <n>                          Count all records with hardware thread ID <n>
    -j                              Java
    -P                              Show physical address
    -w <n>                          warmup for trace
    -r <n>                          runlength for trace (including warmup)
    -o <filename>                   write output to <filename>. Defaults to stdout.
    -S                              sort output from largest to smallest instruction count
    -u                              skip non-user mode instructions
    -l                              local history data for branches & load/store strides
    -T                              only include region of interest between tracepoints. If specified, the -s/-w and -r arguments apply to the ROI between tracepoints.
    --roi-start-opcode <opcode>     override the tracepoint ROI start opcode
    --roi-stop-opcode <opcode>      override the tracepoint ROI stop opcode
    --roi-start-pc <opcode>         start ROI at specified PC instead of a tracepoint opcode
    --roi-stop-pc <opcode>          stop ROI at specified PC instead of a tracepoint opcode
    <trace>                         trace in STF format
```
