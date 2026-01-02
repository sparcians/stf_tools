# stf_address_map

Summarizes memory accesses from a trace on a user-specified address boundary

```
usage: stf_address_map [options] <trace>
    -h, --help         show this message
    -V, --version      show the STF version the tool is built with
    -o <output>        output file (defaults to stdout)
    -a <alignment>     align addresses to the specified number of bytes
    -m <accesses>      restrict report to addresses with at least this many accesses
    -u                 only dump user-mode instructions
    -i                 count instruction PCs
    -I                 only count instruction PCs (implies -i)
    <trace>            trace in STF format

Example:
    List all addresses accessed at least 32 times, aligned to an 8-byte address
    stf_address_map -a 8 -m 32 trace.zstf
```
