# stf_branch_lhr

Displays the local history of every branch in a trace

```
usage: stf_branch_lhr [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -v                verbose mode (prints indirect branch targets)
    -u                skip non user-mode instructions
    <trace>           trace in STF format

Detect and output the local history of all static branches in the specified STF
```
