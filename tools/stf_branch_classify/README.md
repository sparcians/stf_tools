# stf_branch_classify

Classifies branch types, targets, and taken/not-taken counts

```
usage: stf_branch_classify [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -v                verbose mode (prints indirect branch targets)
    -t                only report taken branches (branches that are taken at least once)
    -d                only report dynamic branches (branches that are not always-taken or never-taken)
    -u                skip non user-mode instructions
    <trace>           trace in STF format
```
