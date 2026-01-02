# stf_diff

Shows the differences between two traces

```
usage: stf_diff [options] <trace1> <trace2>
    -h, --help          show this message
    -V, --version       show the STF version the tool is built with
    -1 <N>              start diff on Nth instruction of trace1
    -2 <N>              start diff on Nth instruction of trace2
    -l <len>            run diff for <len> instructions
    -k                  ignore kernel instructions
    -A                  ignore addresses
    -M                  compare memory records
    -p                  compare all physical addresses
    -P                  only compare data physical addresses
    -R                  compare register records
    -D                  compare destination register records
    -S                  compare register state records
    -c <N>              Exit after the Nth difference
    -C                  Just report the number of differences
    -u                  run unified diff
    -a                  use register aliases in disassembly
    -m                  begin diff after first markpoint
    -t                  begin diff after first tracepoint
    -W <workaround>     enable specified workaround
    <trace1>            first STF trace to compare
    <trace2>            second STF trace to compare

Workarounds:
    spike_lr_sc: If a mismatch occurs due to a failed LR/SC pair, realign the traces and continue
```
