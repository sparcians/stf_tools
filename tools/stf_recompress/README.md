# stf_recompress

Makes a compressed/uncompressed/re-compressed copy of an existing trace

```
usage: stf_recompress [options] <infile> <outfile>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -f                Overwrite existing file
    -c <#>            Compression level (ZSTD: 1-22, default 3)
    -C <#>            Chunk size (default 100000)
    <infile>          STF to recompress
    <outfile>         Output STF file
```
