# stf_merge

Merges multiple traces together into a single trace

```
usage: stf_merge [options]
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -b <N>            starting from the n instruction (inclusive) of input trace. Default is 1.
    -e <M>            ending at the nth instruction (exclusive) of input trace
    -r <K>            repeat the specified intruction interval [N, M) K times. Default is 1.
    -f <trace>        filename of input trace
    -o <trace>        output trace filename. stdout is default

-b, -e, -r, and -f flags can be specified multiple times for merging multiple traces
```
