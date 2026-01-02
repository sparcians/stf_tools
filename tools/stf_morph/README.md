# stf_morph

Replaces instructions in a trace with user-specified instructions

```
usage: stf_morph [options] <trace>
    -h, --help                                                                           show this message
    -V, --version                                                                        show the STF version the tool is built with
    -o <trace>                                                                           output filename
    -s <N>                                                                               start at instruction N
    -e <M>                                                                               end at instruction M
    -A <address>                                                                         assume all LS ops access the given address
    -S <size>                                                                            assume all LS ops have the given size
    --stride <stride>                                                                    increment all LS ops' addresses by the given stride after each instance
    -C                                                                                   allow STFID and PC-based morphs to collide. STFID morphs will take precedence.
    -a <pc=opcode1[@addr1:size1[+stride1]][,opcode2[@addr2:size2[+stride2]],...]>        morph instruction(s) starting at pc to specified opcode(s). LS instructions can have target addresses and access sizes (and an optional stride) specified with `opcode@addr:size+stride` syntax
    -i <stfid=opcode1[@addr1:size1[+stride1]][,opcode2[@addr2:size2[+stride2]],...]>     morph instruction(s) starting at stfid to specified opcode(s). LS instructions can have target addresses and access sizes (and an optional stride) specified with `opcode@addr:size+stride` syntax
    <trace>                                                                              trace in STF format
```
