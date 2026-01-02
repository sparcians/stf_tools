# stf_trace_info

Checks if a trace supports certain feature flags

```
usage: stf_trace_info [options] <trace>
    -h, --help         show this message
    -V, --version      show the STF version the tool is built with
    -o <output>         Generate output trace filename with specified trace info. If not specified, show existing trace info.
    -g <generator>     Specify decimal trace generator for output. 1-QEMU, 2-Android Emulator, 3-GEM5
    -v <gen_ver>       Specify trace info generator version for output
    -c <comment>       Specify trace info comment for output
    -f <feature>       Specify 32bit hex value for trace info feature for output
    -d                 Show the detailed info, such as trace version, comments etc.
    <trace>            trace in STF format

Trace Feature Codes:
    STF_CONTAIN_PHYSICAL_ADDRESS        0x00001
    STF_CONTAIN_DATA_ATTRIBUTE          0x00002
    STF_CONTAIN_OPERAND_VALUE           0x00004
    STF_CONTAIN_EVENT                   0x00008
    STF_CONTAIN_SYSTEMCALL_VALUE        0x00010
    STF_CONTAIN_INT_DIV_OPERAND_VALUE   0x00040
    STF_CONTAIN_SAMPLING                0x00080
    STF_CONTAIN_EMBEDDED_PTE            0x00100
    STF_CONTAIN_SIMPOINT                0x00200
    STF_CONTAIN_PROCESS_ID              0x00400
    STF_CONTAIN_PTE_ONLY                0x00800
    STF_NEED_POST_PROCESS               0x01000
    STF_CONTAIN_REG_STATE               0x02000
    STF_CONTAIN_MICROOP                 0x04000
    STF_CONTAIN_MULTI_THREAD            0x08000
    STF_CONTAIN_MULTI_CORE              0x10000
    STF_CONTAIN_PTE_HW_AD               0x20000
    STF_CONTAIN_VEC                     0x40000
    STF_CONTAIN_EVENT64                 0x80000
```
