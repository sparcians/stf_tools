#pragma once

#include <unordered_map>
#include "stf_descriptor.hpp"

static const auto STF_DESCRIPTOR_NAME_MAP = std::unordered_map<std::string, stf::descriptors::internal::Descriptor>({
    {"STF_INST_REG", stf::descriptors::internal::Descriptor::STF_INST_REG},
    {"STF_INST_OPCODE16", stf::descriptors::internal::Descriptor::STF_INST_OPCODE16},
    {"STF_INST_OPCODE32", stf::descriptors::internal::Descriptor::STF_INST_OPCODE32},
    {"STF_INST_MEM_ACCESS", stf::descriptors::internal::Descriptor::STF_INST_MEM_ACCESS},
    {"STF_INST_MEM_CONTENT", stf::descriptors::internal::Descriptor::STF_INST_MEM_CONTENT},
    {"STF_INST_PC_TARGET", stf::descriptors::internal::Descriptor::STF_INST_PC_TARGET},
    {"STF_EVENT", stf::descriptors::internal::Descriptor::STF_EVENT},
    {"STF_EVENT_PC_TARGET", stf::descriptors::internal::Descriptor::STF_EVENT_PC_TARGET},
    {"STF_PAGE_TABLE_WALK", stf::descriptors::internal::Descriptor::STF_PAGE_TABLE_WALK},
    {"STF_BUS_MASTER_ACCESS", stf::descriptors::internal::Descriptor::STF_BUS_MASTER_ACCESS},
    {"STF_BUS_MASTER_CONTENT", stf::descriptors::internal::Descriptor::STF_BUS_MASTER_CONTENT},
    {"STF_COMMENT", stf::descriptors::internal::Descriptor::STF_COMMENT},
    {"STF_FORCE_PC", stf::descriptors::internal::Descriptor::STF_FORCE_PC},
    {"STF_INST_READY_REG", stf::descriptors::internal::Descriptor::STF_INST_READY_REG},
    {"STF_PROCESS_ID_EXT", stf::descriptors::internal::Descriptor::STF_PROCESS_ID_EXT},
    {"STF_INST_MICROOP", stf::descriptors::internal::Descriptor::STF_INST_MICROOP},
    {"STF_IDENTIFIER", stf::descriptors::internal::Descriptor::STF_IDENTIFIER},
    {"STF_ISA", stf::descriptors::internal::Descriptor::STF_ISA},
    {"STF_INST_IEM", stf::descriptors::internal::Descriptor::STF_INST_IEM},
    {"STF_TRACE_INFO", stf::descriptors::internal::Descriptor::STF_TRACE_INFO},
    {"STF_TRACE_INFO_FEATURE", stf::descriptors::internal::Descriptor::STF_TRACE_INFO_FEATURE},
    {"STF_VERSION", stf::descriptors::internal::Descriptor::STF_VERSION},
    {"STF_VLEN_CONFIG", stf::descriptors::internal::Descriptor::STF_VLEN_CONFIG},
    {"STF_END_HEADER", stf::descriptors::internal::Descriptor::STF_END_HEADER},
    {"STF_RESERVED", stf::descriptors::internal::Descriptor::STF_RESERVED},
    {"RESERVED_END", stf::descriptors::internal::Descriptor::__RESERVED_END}
});
