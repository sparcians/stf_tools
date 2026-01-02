# stf_transaction_extract

Extracts part of an existing transaction trace into a new trace

```
usage: stf_transaction_extract [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -s <n>            skip the first n transactions
    -k <n>            keep only the first n transactions
    -t <n>            output trace files each containing n transactions
    -o <trace>        output filename. stdout is default
    <trace>           trace in STF format

common usages:
    -s <n> -k <m> -o <output> <input> -- skip the first <n> transactions and write the next <m> to <output>
    -s <n> -o <output> <input> -- skip the first <n> transactions and write the rest to <output>
    -s <n> -t <m> <output> <input> -- skip the first <n> transactions and write every <m> transactions to <output.xxxxx.zstf>
    -t <m> -o <output> <input> -- write every <m> transactions to <output.xxxxx.zstf>
```
