# stf_filter_evt

Filters specified events out of a trace

```
usage: stf_filter_evt [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -s <N>            start trace event filtering at N-th instruction
    -e <M>            end trace event filtering at M-th instruction
    -E <evt>          Event to filter, i.e. -E 0x5 -E 0x2
    -K                filter all kernel activities, not including syscalls
    -S                filter all syscalls, keeping the call instructions
    -o <trace>        output filename. stdout is default
    -v                verbose mode to show message
    <trace>           trace in STF format
```
