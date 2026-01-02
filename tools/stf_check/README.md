# stf_check

Checks traces for compliance with the [STF Specification](https://github.com/sparcians/stf_spec)

```
usage: stf_check [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -u                skip non-user code
    -c                continue on error
    -d                print out memory point to zero errors
    -p                print trace info
    -P                check prefetch memory address
    -n                skip checking for physical address
    -v                always print error counts at the end
    -e <M>            end checking at M-th instruction
    -i <err>          ignore the specified error type
    <trace>           trace in STF format
```
