# stf_find

Finds accesses to specified instruction or data addresses

```
usage: stf_find [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -u                skip non-user instructions
    -a <addr>         find instructions at the specified address
    -m <addr>         find memory accesses at this address
    -p <addr>         find memory accesses at this physical address
    -M <mask>         mask to apply to memory accesses
    -e                print extra info (e.g., memory address/data for load/store).
    -s                summary:  show first and last tin, and total count.
    -S <inst>         skip to instruction # <inst>
    -E <inst>         stop after instruction # <inst>
    -I                per-Iter:  show instructions between these markers, per iter, concisely.
    -c <count>        stop after <count> matches
    <trace>           trace in STF format
```
