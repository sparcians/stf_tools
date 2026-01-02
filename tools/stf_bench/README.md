# stf_bench

Measures the speed of the different STF readers with a given trace

```
usage: stf_bench [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -r <reader>       Reader to test (0 = all, 1 = STFReader, 2 = STFInstReader, 3 = STFBranchReader)
    -u                Skip non-user instructions (will not apply to STFReader)
    -p                Enable page table tracking
    <trace>           STF to test with
```
