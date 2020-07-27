#pragma once

#include <cstdint>

/**
 * \brief Branch target
 *
 */
struct BranchTarget {
    /** branch target */
    uint64_t branch_target;
    /** taken branch index */
    uint64_t inst_count;
};
