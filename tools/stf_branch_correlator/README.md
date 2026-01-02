# stf_branch_correlator

Correlates conditional branch behavior with global history or pairwise with other branches

```
usage: stf_branch_correlator [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -u                skip non user-mode instructions
    -p                Output branch pair correlation data
    <trace>           trace in STF format

Detect and output correlation data for static branches in trace.  Correlation can be to global history (history correlation) or other branches (pair correlation).  Default output is history correlation.
```
