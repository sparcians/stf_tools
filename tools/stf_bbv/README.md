# stf_bbv

Generates basic block vector counts suitable for running Simpoint

```
usage: stf_bbv [options] <trace>
    -h, --help              show this message
    -V, --version           show the STF version the tool is built with
    -o <output>             output filename (defaults to stdout if omitted)
    -u <user_mode_file>     output filename containing whether each interval has non-user code in it
    -s <N>                  start to collect Basic Block Vector info at N-th instruction
    -e <M>                  end basic block vector collection at M-th instruction
    -w <K>                  basic block vector collection in every K instructions
    -m <L>                  ensure a minimum of L instructions have passed before dumping user-mode BBVs
    <trace>                 trace in STF format
```
