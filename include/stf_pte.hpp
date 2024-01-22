#pragma once

#include <set>
#include <unordered_map>
#include <vector>

#include "format_utils.hpp"
#include "stf_exception.hpp"
#include "stf_record_map.hpp"
#include "stf_record_types.hpp"
#include "stf_writer.hpp"
#include "util.hpp"

namespace stf {
    /**
     * \class STF_PTE
     *
     * Class used to track PTEs in an STF
     */
    class STF_PTE {
        private:
            /**
             * \struct Data
             *
             * Tracks PID and whether a given PTE is used in the trace
             */
            struct Data {
                uint32_t pid = 0; /**< PID for the PTE */
                using pointer_type = const PageTableWalkRecord*;
                pointer_type walk_info_; /**< Walk record containing the PTE */
                bool used = false; /**< Whether the PTE is used in the trace */

                Data() = default;

                Data(uint32_t pid, const pointer_type& walk_info, bool used) :
                    pid(pid),
                    walk_info_(walk_info),
                    used(used)
                {
                }

                bool hasDesc() const { return walk_info_ && walk_info_->getNumPTEs(); }
            };

            /**
             * \typedef PTE
             * Map used to store PTEs
             */
            using PTE = std::unordered_map<uint64_t, Data>;

            /**
             * \typedef PIDPTEs
             * Maps PIDs to PTEs
             */
            using PIDPTEs = std::unordered_map<uint32_t, PTE>;

            /**
             * \typedef PSizes
             * Tracks page sizes
             */
            using PSizes = std::set<uint64_t>;

            /**
             * \typedef PIDPageSizes
             * Tracks per-PID page sizes
             */
            using PIDPageSizes = std::unordered_map<uint32_t, PSizes>;

            std::shared_ptr<STFWriter> writer_; /**< STFWriter, may be nullptr if writing is not enabled */
            std::shared_ptr<STFWriter> pte_writer_; /**< PTE-only STFWriter, may be nullptr if writing is not enabled */
            const bool ignore_pid_mismatch_; /**< Whether PID mismatches should be ignored */

            PIDPTEs ptemap_; /**< Maps PIDs to PTEs */
            PIDPageSizes page_sizes_; /**< Maps PIDs to page sizes */

            /**
             * \class PTENotFoundException
             * Used to inform callers of checkPTE_ if a PTE was not found
             */
            class PTENotFoundException : public std::exception {
            };

            /**
             * Checks whether a given PTE already exists, and marks it used if it is - throws a PTENotFoundException if it was not
             *
             * \param pid PID
             * \param vaddr Virtual address
             * \param paddr Physical address
             * \param page_size Page size
             */
            uint64_t checkPTE_(uint32_t pid, uint64_t vaddr, uint64_t paddr, uint64_t page_size) {
                const uint64_t page_mask = page_size - 1;
                const uint64_t vpage = vaddr & ~page_mask;
                try {
                    Data &d = ptemap_.at(pid).at(vpage);
                    if (!d.used) {
                        d.used = true;
                        if (writer_) {
                            *writer_ << *d.walk_info_;
                        }
                        if (pte_writer_) {
                            *pte_writer_ << *d.walk_info_;
                        }
                    }
                    if (d.walk_info_->getPhysicalPageAddr() == (paddr & ~page_mask)) {
                        return d.walk_info_->getPhysicalPageAddr() | (vaddr & page_mask);
                    }
                    return page_utils::INVALID_PHYS_ADDR;
                }
                catch(const std::out_of_range&) {
                    throw PTENotFoundException();
                }
            }

        public:
            /**
             * Constructs an STF_PTE
             * \param writer STFWriter to use
             * \param pte_writer PTE-specific STFWriter to use
             * \param ignore_pid_mismatch If true, ignore PID mismatches
             */
            STF_PTE(std::shared_ptr<STFWriter> writer,
                    std::shared_ptr<STFWriter> pte_writer,
                    bool ignore_pid_mismatch = false) :
                writer_(std::move(writer)),
                pte_writer_(std::move(pte_writer)),
                ignore_pid_mismatch_(ignore_pid_mismatch)
            {
            }

            /**
             * Update the PTE and also mark it as used
             * This is used when the trace does not have PTEs and thus we need to construct
             * the PTEs from vaddr:paddr pair
             *
             * \param pid PID
             * \param walk_info PageTableWalkRecord to update with
             */
            uint64_t UpdateAndMarkPTE(uint32_t pid,
                                      const PageTableWalkRecord* walk_info) {
                const uint64_t vpage = walk_info->getVA();
                const uint64_t paddr = walk_info->getPhysicalPageAddr();
                if (!UpdatePTE(pid, walk_info)) {
                    return MarkPTE(pid, vpage, paddr);
                }

                return paddr;
            }

            /**
             * Update the PTEs
             *
             * \param pid PID
             * \param walk_info Page table walk record
             */
            bool UpdatePTE(uint32_t pid, const PageTableWalkRecord* walk_info) {
                const uint64_t page_size_mask = static_cast<uint64_t>(walk_info->getPageSize()) - 1;
                stf_assert((walk_info->getVA() & page_size_mask) == 0,
                           "Virtual page address is not page-aligned: " << std::hex << walk_info->getVA());

                stf_assert((walk_info->getPhysicalPageAddr() & page_size_mask) == 0,
                           "Physical page address is not page-aligned: " << std::hex << walk_info->getPhysicalPageAddr());

                uint32_t ppid = pid;
                page_sizes_[ppid].insert(walk_info->getPageSize());

                if (ptemap_.find(ppid) != ptemap_.end()) {
                    if (ptemap_.at(ppid).find(walk_info->getVA()) != ptemap_.at(ppid).end()) {
                        Data &d = ptemap_.at(ppid).at(walk_info->getVA());

                        if(d.walk_info_) {
                            // same vpage different attributes
                            if ((d.pid != pid) || (*d.walk_info_ != *walk_info)) {

                                d.pid = pid;
                                d.walk_info_ = walk_info;
                                d.used = false;
                            }
                        }

                        return d.used;
                    }

                    // check overlappings only when vpage is not found in ptemap_
                    std::vector<uint64_t> keys;
                    for (const auto& it : ptemap_.at(ppid)) {
                        const uint64_t evpage = it.first;
                        const uint64_t epsize = it.second.walk_info_->getPageSize();

                        if (((walk_info->getVA() + walk_info->getPageSize()) > evpage) && ((evpage + epsize) > walk_info->getVA())) {
                            keys.push_back(it.first);
                        }
                    }

                    for (const auto& key : keys) {
                        ptemap_.at(ppid).erase(key);
                    }
                }

                ptemap_[ppid].emplace(walk_info->getVA(), Data(pid, walk_info, false));

                return false;
            }

            /**
             * Gets the appropriate page size mask for a given PID and VA
             *
             * \param pid PID
             * \param vaddr Virtual address
             */
            uint64_t GetPageMask(uint32_t pid, uint64_t vaddr) {
                static constexpr uint64_t PAGE4K_MASK = 0X0000000000000FFFULL;
                static constexpr uint64_t PAGE2M_MASK = 0X00000000001FFFFFULL;
                static constexpr uint64_t PAGE1G_MASK = 0X000000003FFFFFFFULL;

                try {
                    uint64_t mask = PAGE4K_MASK;
                    const auto& ptemap = ptemap_.at(pid);
                    if(ptemap.find(vaddr & ~mask) == ptemap.end()) {
                        mask = PAGE2M_MASK;
                        if(ptemap.find(vaddr & ~mask) == ptemap.end()) {
                            mask = PAGE1G_MASK;
                            if(ptemap.find(vaddr & ~mask) == ptemap.end()) {
                                return page_utils::INVALID_PAGE_SIZE;
                            }
                        }
                    }
                    return mask;
                }
                catch(const std::out_of_range&) {
                }
                return page_utils::INVALID_PAGE_SIZE;
            }

            /**
             * Marks all pages as unused
             */
            void ResetUsage() {
                for(auto &pid : ptemap_) {
                    for(auto &vpage : pid.second) {
                        vpage.second.used = false;
                    }
                }
            }

            /**
             * Marks a PTE used if it exists for the given VA->PA mapping
             *
             * \param pid PID
             * \param vaddr Virtual address
             * \param paddr Physical address
             * \returns The physical page address if the PTE exists, otherwise INVALID_PHYS_ADDR
             */
            uint64_t MarkPTE(uint32_t pid, uint64_t vaddr, uint64_t paddr) {
                for (const auto& page_size: page_sizes_[pid]) {
                    try {
                        return checkPTE_(pid, vaddr, paddr, page_size);
                    }
                    catch(const PTENotFoundException&) {
                    }
                }

                if (ignore_pid_mismatch_) {
                    // try other PIDs' memory spaces
                    for (const auto& a: ptemap_) {
                        uint32_t ppid = a.first;
                        if (ppid == pid) {
                            continue;
                        }
                        for (const auto& page_size: page_sizes_[ppid]) {
                            try {
                                return checkPTE_(ppid, vaddr, paddr, page_size);
                            }
                            catch(const PTENotFoundException&) {
                            }
                        }
                    }
                    std::cerr << "ERROR: Expected PTE entry 'PTE ";
                    format_utils::formatHex(std::cerr, vaddr);
                    std::cerr << ':';
                    format_utils::formatHex(std::cerr, paddr);
                    std::cerr << " PID ";
                    format_utils::formatHex(std::cerr, pid);
                    std::cerr << "' is not found in any address space or page table map." << std::endl;
                } else {
                    std::cerr << "ERROR: Expected PTE entry 'PTE ";
                    format_utils::formatHex(std::cerr, vaddr);
                    std::cerr << ':';
                    format_utils::formatHex(std::cerr, paddr);
                    std::cerr << " PID ";
                    format_utils::formatHex(std::cerr, pid);
                    std::cerr << "' is not found in the page table map for the given PID." << std::endl;
                    std::cerr << "     set ignore_pid_mismatch to try other address spaces." << std::endl;
                }

                return page_utils::INVALID_PHYS_ADDR;
            }

            /**
             * \brief Function to write existing live page table entres into
             *  stf file;
             *  return number of PTE entries written into stf file;
             *
             * \param stf_writer the stf output file object;
             */
            uint32_t DumpPTEtoSTF(STFWriter& stf_writer) {
                uint32_t pte_count = 0;
                if (!stf_writer) {
                    return pte_count;
                }

                for (const auto &pid : ptemap_) {
                    for (const auto &pte : pid.second) {
                        stf_writer << *pte.second.walk_info_;
                        ++pte_count;
                    }
                }

                return pte_count;
            }

            /**
             * Checks whether a PTE exists, and dumps it if it does
             * \param stf_writer STFWriter to use
             * \param pid PID
             * \param vaddr Virtual address
             * \param inst_offset Instruction index at which this PTE becomes valid
             * \param size page size
             */
            bool CheckAndDumpNewPTESingle(STFWriter& stf_writer,
                                          uint32_t pid,
                                          uint64_t vaddr,
                                          uint64_t inst_offset,
                                          uint32_t size = 0) {
                bool retval = false;
                if(!stf_writer) {
                    std::cerr << "exiting :: stf_writer == NULL" << std::endl;
                    return false;
                }

                uint64_t mask = GetPageMask(pid, vaddr);
                if(mask != page_utils::INVALID_PAGE_SIZE) {
                    try {
                        Data &pte = ptemap_.at(pid).at(vaddr & ~mask);
                        if(!pte.used) {
                            PageTableWalkRecord new_rec = *pte.walk_info_;
                            new_rec.setFirstAccessIndex(inst_offset + 1);
                            stf_writer << new_rec;
                            pte.used = true;
                            retval = true;
                        }

                        if (!size) {
                            return retval;
                        }

                        //Check for page crossing
                        if(((vaddr + size - 1) & ~mask) != (vaddr & ~mask)) {
                            uint64_t nextPageVaddr = (vaddr + size - 1) & ~mask;
                            Data &pte2 = ptemap_.at(pid).at(nextPageVaddr);
                            if(!pte2.used) {
                                PageTableWalkRecord new_rec2 = *pte2.walk_info_;
                                new_rec2.setFirstAccessIndex(inst_offset + 1);
                                stf_writer << new_rec2;
                                pte2.used = true;
                                retval = true;
                            }
                        }
                    }
                    catch(const std::out_of_range&)
                    {
                    }
                }

                return retval;
            }
    };
} // end namespace stf
