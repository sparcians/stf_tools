# stf_address_sequence

Analyzes temporal locality of memory accesses in a trace

```
usage: stf_address_sequence [options] <trace>
    -h, --help                   show this message
    -V, --version                show the STF version the tool is built with
    -o <output>                  output file (defaults to stdout), if named batch, auto name based on trace, w/o top row in summary
    -a <alignment>               align addresses to the specified number of bytes
    -u                           only dump user-mode instructions
    -i                           count instruction PCs
    -I                           only count instruction PCs (implies -i)
    -R                           exclude reads
    -W                           exclude writes
    -C <max_distance_access>     restrict report to repeated accesses with at MOST this distance. Each memory transaction(RD or WR) counts one access
    -A <max_distance_stream>     restrict report to repeated streams with at MOST this distance. Each gather counts one. Access pattern ABA counts 3, AABB counts 2
    <trace>                      trace in STF format

Terminology:
    Repeated access  - The access pattern AAAAA doesn't count. AABAAAAA counts 5 repeated accesses to A. ABABABAB counts 3 repeated accesses to A, and 3 to B
    Repeated stream  - The access pattern AAAAA doesn't count. AABAAAAA counts 1 repeated stream to A. ABABABAB counts 3 repeated streams to A, and 3 to B, even if each stream consists of only one access

Background:
    This program is created to examine temporal locality. So the distance constraints/filters are defined as max distance. If the 2 memory transactions are far apart, separated by other memory transactions, the 2nd one won't be considered as repeated access/stream, even though it accesses the same aligned address
    The access pattern AABBAA has 6 accesses, 3 streams. The distance is calculated based on ID shown below. From the second first A to the second last A, the access distance is 3, the stream distance is 2
                       --
                       strm0, access0, access1
                         --
                         strm1, access2, access3
                           --
                           strm2, access4, access5

Example:
    List all addresses, aligned to an 8-byte address
        stf_address_sequence -a 8 trace.zstf

Note:
    !!!warm-up isn't supported!!!
```
