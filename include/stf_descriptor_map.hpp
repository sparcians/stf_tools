#pragma once

#include <sstream>
#include <unordered_map>
#include "stf_descriptor.hpp"

using DescriptorMap = std::map<std::string, stf::descriptors::internal::Descriptor>;

static const auto STF_DESCRIPTOR_NAME_MAP = [] {
    DescriptorMap desc_map;

    for(auto it = stf::descriptors::iterators::sorted_internal_iterator();
             it != stf::descriptors::iterators::SORTED_INTERNAL_ITERATOR_END; ++it) {
        const auto enum_val = *it;
        std::ostringstream os;
        os << enum_val;
        desc_map.emplace(std::move(os).str(), enum_val);
    }

    return desc_map;
}();
