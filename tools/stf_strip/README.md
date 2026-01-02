# stf_strip

Removes specified record types from a trace

```
usage: stf_strip [options] <trace> <output>
    -h, --help        show this message
    -V, --version     show the STF version the tool is built with
    -r <type>         Record type to remove. Can be specified multiple times.
    -f                Allow overwriting the output file
    -c <#>            Compression level (ZSTD: 1-22, default 3)
    <trace>           Trace in STF format
    <output>          Output filename

Allowed record types:
    STF_RESERVED
    STF_END_HEADER
    STF_TRACE_INFO_FEATURE
    RESERVED_END
    STF_VLEN_CONFIG
    STF_VERSION
    STF_ISA
    STF_IDENTIFIER
    STF_INST_MICROOP
    STF_INST_IEM
    STF_INST_READY_REG
    STF_INST_REG
    STF_INST_OPCODE16
    STF_INST_OPCODE32
    STF_COMMENT
    STF_INST_MEM_ACCESS
    STF_INST_MEM_CONTENT
    STF_PROCESS_ID_EXT
    STF_EVENT
    STF_TRACE_INFO
    STF_INST_PC_TARGET
    STF_PAGE_TABLE_WALK
    STF_EVENT_PC_TARGET
    STF_BUS_MASTER_ACCESS
    STF_FORCE_PC
    STF_BUS_MASTER_CONTENT

Example:
    stf_strip -r STF_INST_REG -c 22 input.zstf output.zstf
```
