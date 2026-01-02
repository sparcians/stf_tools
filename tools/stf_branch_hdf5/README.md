# stf_branch_hdf5

Creates an HDF5 with every branch from a trace, including operand values, branch type, comparison type, history, etc. for further analysis

```
usage: stf_branch_hdf5 [options] <trace> <output>
    -h, --help             show this message
    -V, --version          show the STF version the tool is built with
    -u                     skip non user-mode instructions
    -l <N>                 limit output to the top N most frequent branches
    -U                     use unsigned ({0,1}) encoding for boolean values. The default behavior encodes boolean values to {-1,1}. The taken field is *always* encoded to {0,1}.
    -O                     fill in target opcodes for not-taken branches. If the opcode is unknown, it will be set to a random value.
    -D                     decode target opcodes
    -1                     break up opcodes, register values, and addresses into 1-byte chunks
    -w <wkld_id>           Add a wkld_id field with the specified value
    -L <L>                 Keep a local history of length L (maximum length is 64). If -1 is specified, this field is broken up into single bit fields.
    -X                     exclude loop branches
    -x <exclude_field>     exclude specified field. Can be specified multiple times.
    <trace>                trace in STF format
    <output>               output HDF5
```
