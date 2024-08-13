#pragma once

#include "command_line_parser.hpp"
#include "stf_inst_reader.hpp"
#include "stf_decoder.hpp"

namespace stf {
    template<typename DerivedClass>
    class STFRegionIterator {
        protected:
            STFInstReader::iterator it_;
            STFInstReader::iterator end_it_;
            bool found_roi_ = false;

            void updateROI_() {
                if(found_roi_ && static_cast<const DerivedClass*>(this)->isEndOfROI_()) {
                    found_roi_ = false;
                }
            }

            void findROI_() {
                if(found_roi_) {
                    return;
                }

                for(; it_ != end_it_; ++it_) {
                    if(static_cast<const DerivedClass*>(this)->isStartOfROI_()) {
                        found_roi_ = true;
                        break;
                    }
                }
            }

        public:
            explicit STFRegionIterator(STFInstReader& reader) :
                it_(reader.begin()),
                end_it_(reader.end())
            {
                // Note that this does not call findROI_. The derived class must call it as part of
                // its constructor.
            }

            DerivedClass& operator++() {
                updateROI_();
                ++it_;
                findROI_();
                return *static_cast<DerivedClass*>(this);
            }

            DerivedClass operator++(int) {
                DerivedClass temp(*static_cast<const DerivedClass*>(this));
                ++(*this);
                return temp;
            }

            DerivedClass& operator+=(const size_t num_items) {
                for(size_t i = 0; i < num_items && it_ != end_it_; ++i) {
                    ++(*this);
                }
                return *static_cast<DerivedClass*>(this);
            }

            bool operator==(const DerivedClass& rhs) const {
                return it_ == rhs.it_ && end_it_ == rhs.end_it_;
            }

            bool operator!=(const DerivedClass& rhs) const {
                return !(*this == rhs);
            }

            bool operator==(const STFInstReader::iterator& rhs) const {
                return it_ == rhs;
            }

            bool operator!=(const STFInstReader::iterator& rhs) const {
                return !(*this == rhs);
            }

            const STFInst* operator->() const {
                return it_.operator->();
            }

            const STFInst& operator*() const {
                return *it_;
            }
    };

    /**
     * \class STFTracepointIterator
     * \brief Wrapper class for STFInstReader::iterator that only returns instructions between tracepoints
     */
    class STFTracepointIterator : public STFRegionIterator<STFTracepointIterator> {
        private:
            using ParentClass = STFRegionIterator<STFTracepointIterator>;
            friend ParentClass;

            const uint32_t start_opcode_ = 0;
            const uint32_t stop_opcode_ = 0;

            bool isStartOfROI_() const {
                return it_->opcode() == start_opcode_;
            }

            bool isEndOfROI_() const {
                return it_->opcode() == stop_opcode_;
            }

            static inline std::tuple<uint32_t, uint32_t> initOpcodes_(const STFInstReader& reader,
                                                                      uint32_t start_opcode,
                                                                      uint32_t stop_opcode) {
                if(start_opcode == 0 || stop_opcode == 0) {
                    STFDecoder decoder(reader.getInitialIEM());

                    if(start_opcode == 0) {
                        start_opcode = decoder.getStartTracepointOpcode();
                    }

                    if(stop_opcode == 0) {
                        stop_opcode = decoder.getStopTracepointOpcode();
                    }
                }

                return std::make_tuple(start_opcode, stop_opcode);
            }

            STFTracepointIterator(STFInstReader& reader,
                                  std::tuple<uint32_t, uint32_t>&& opcodes) :
                STFRegionIterator<STFTracepointIterator>(reader),
                start_opcode_(std::get<0>(opcodes)),
                stop_opcode_(std::get<1>(opcodes))
            {
                findROI_();
            }

        public:
            /**
             * Constructs an STFTracepointIterator from an STFInstReader.
             */
            explicit STFTracepointIterator(STFInstReader& reader,
                                           const uint32_t start_opcode = 0,
                                           const uint32_t stop_opcode = 0) :
                STFTracepointIterator(reader, initOpcodes_(reader, start_opcode, stop_opcode))
            {
            }
    };

    /**
     * \class STFPCIterator
     * \brief Wrapper class for STFInstReader::iterator that only returns instructions between tracepoints
     */
    class STFPCIterator : public STFRegionIterator<STFPCIterator> {
        private:
            using ParentClass = STFRegionIterator<STFPCIterator>;
            friend ParentClass;

            const uint64_t start_pc_ = 0;
            const uint64_t stop_pc_ = 0;

            bool isStartOfROI_() const {
                return (start_pc_ == 0) || (it_->pc() == start_pc_);
            }

            bool isEndOfROI_() const {
                return (stop_pc_ != 0) && (it_->pc() == stop_pc_);
            }

        public:
            /**
             * Constructs an STFPCIterator from an STFInstReader.
             */
            explicit STFPCIterator(STFInstReader& reader,
                                   const uint64_t start_pc,
                                   const uint64_t stop_pc) :
                STFRegionIterator<STFPCIterator>(reader),
                start_pc_(start_pc),
                stop_pc_(stop_pc)
            {
                findROI_();
            }
    };

    template<typename IteratorType, typename... IteratorArgs>
    std::enable_if_t<std::is_same_v<IteratorType, STFInstReader::iterator>, STFInstReader::iterator>
    getStartIterator(STFInstReader& reader, const size_t skip_count, IteratorArgs&&...) {
        return reader.begin(skip_count);
    }

    template<typename IteratorType, typename... IteratorArgs>
    std::enable_if_t<std::is_base_of_v<STFRegionIterator<IteratorType>, IteratorType>, IteratorType>
    getStartIterator(STFInstReader& reader, const size_t skip_count, IteratorArgs&&... args) {
        auto it = IteratorType(reader, std::forward<IteratorArgs>(args)...);
        it += skip_count;
        return it;
    }
}

namespace trace_tools {
    inline void addTracepointCommandLineArgs(CommandLineParser& parser, const char* start_arg = nullptr, const char* stop_arg = nullptr) {
        std::ostringstream help_msg_ss;

        help_msg_ss << "only include region of interest between tracepoints";

        if(start_arg || stop_arg) {
            help_msg_ss << ". If specified, the ";
            if(start_arg && stop_arg) {
                help_msg_ss << start_arg << " and " << stop_arg << " arguments apply";
            }
            else {
                if(start_arg) {
                    help_msg_ss << start_arg;
                }
                else {
                    help_msg_ss << stop_arg;
                }
                help_msg_ss << " argument applies";
            }

            help_msg_ss << " to the ROI between tracepoints.";
        }

        parser.addFlag('T', help_msg_ss.str());
        parser.addFlag("roi-start-opcode", "opcode", "override the tracepoint ROI start opcode");
        parser.addFlag("roi-stop-opcode", "opcode", "override the tracepoint ROI stop opcode");
        parser.addFlag("roi-start-pc", "opcode", "start ROI at specified PC instead of a tracepoint opcode");
        parser.addFlag("roi-stop-pc", "opcode", "stop ROI at specified PC instead of a tracepoint opcode");
        parser.setMutuallyExclusive("roi-start-opcode", "roi-start-pc");
        parser.setMutuallyExclusive("roi-start-opcode", "roi-stop-pc");
        parser.setMutuallyExclusive("roi-stop-opcode", "roi-start-pc");
        parser.setMutuallyExclusive("roi-stop-opcode", "roi-stop-pc");
        parser.setDependentArgument("roi-start-opcode", 'T');
        parser.setDependentArgument("roi-stop-opcode", 'T');
        parser.setDependentArgument("roi-start-pc", 'T');
        parser.setDependentArgument("roi-stop-pc", 'T');
    }

    inline void getTracepointCommandLineArgs(const CommandLineParser& parser,
                                             bool& use_tracepoint_roi,
                                             uint32_t& roi_start_opcode,
                                             uint32_t& roi_stop_opcode,
                                             bool& use_pc_roi,
                                             uint64_t& roi_start_pc,
                                             uint64_t& roi_stop_pc) {
        use_tracepoint_roi = parser.hasArgument('T');
        parser.getArgumentValue<uint32_t, 16>("roi-start-opcode", roi_start_opcode);
        parser.getArgumentValue<uint32_t, 16>("roi-stop-opcode", roi_stop_opcode);

        const bool has_start_pc = parser.getArgumentValue<uint64_t, 16>("roi-start-pc", roi_start_pc);
        const bool has_stop_pc = parser.getArgumentValue<uint64_t, 16>("roi-stop-pc", roi_stop_pc);
        use_pc_roi = has_start_pc || has_stop_pc;
    }
}
