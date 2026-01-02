# stf_extract

Extracts part of an existing trace into a new trace

```
usage: stf_extract [options] <trace>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -s <n>            skip the first n instructions
    -k <n>            keep only the first n instructions
    -t <n>            output trace files each containing n instructions
    -o <trace>        output filename. stdout is default
    -f <off>          offset by which to shift inst_pc
    -l                filter out kernel code while extracting
    -d                for slicing (-s/-k/-t) output PTE records on demand
    -u                only count user-mode instructions for -s/-k/-t parameters. Non-user instructions will still be included in the extracted trace unless -l is specified.
    <trace>           trace in STF format

common usages:
    -s <n> -k <m> -o <output> <input> -- skip the first <n> instructions and write the next <m> to <output>
    -s <n> -o <output> <input> -- skip the first <n> instructions and write the rest to <output>
    -s <n> -t <m> <output> <input> -- skip the first <n> instructions and write every <m> instructions to <output.xxxxx.zstf>
    -t <m> -o <output> <input> -- write every <m> instructions to <output.xxxxx.zstf>
```
