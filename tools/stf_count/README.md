# stf_count

Counts the number of records and instructions in a trace

```
usage: stf_count [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -v                multi-line output
    -u                only count user-mode instructions. When this is enabled, -i/-s/-e parameters will be in terms of user-mode instructions.
    -S                short output - only output instruction count
    -c                output in csv format
    -C                make CSV output cumulative
    -i <interval>     dump CSV on this instruction interval
    -s <N>            start counting at Nth instruction
    -e <M>            stop counting at Mth instruction
    <trace>           trace in STF format
```
