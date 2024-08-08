#pragma once

#include "stf_inst_reader.hpp"
#include "stf_decoder.hpp"

namespace stf {
    /**
     * \class STFTracepointIterator
     * \brief Wrapper class for STFInstReader::iterator that only returns instructions between tracepoints
     */
    class STFTracepointIterator {
        private:
            STFInstReader::iterator it_;
            STFInstReader::iterator end_it_;
            std::unique_ptr<STFDecoder> owned_decoder_;
            STFDecoder* decoder_ = nullptr;
            bool found_roi_ = false;

            bool isStartTracepoint_() const {
                return decoder_->decode(it_->opcode()).isStartTracepoint();
            }

            bool isStopTracepoint_() const {
                return decoder_->decode(it_->opcode()).isStopTracepoint();
            }

            void updateROI_() {
                if(found_roi_ && isStopTracepoint_()) {
                    found_roi_ = false;
                }
            }

            void findROI_() {
                if(found_roi_) {
                    return;
                }

                for(; it_ != end_it_; ++it_) {
                    if(isStartTracepoint_()) {
                        found_roi_ = true;
                        break;
                    }
                }
            }

        public:
            /**
             * Constructs an STFTracepointIterator from an STFInstReader. This version will construct and own its own STFDecoder.
             */
            explicit STFTracepointIterator(STFInstReader& reader) :
                it_(reader.begin()),
                end_it_(reader.end()),
                owned_decoder_(std::make_unique<STFDecoder>(reader.getInitialIEM())),
                decoder_(owned_decoder_.get())
            {
                findROI_();
            }

            /**
             * Constructs an STFTracepointIterator from an STFInstReader and an STFDecoder. This version will NOT own the STFDecoder, and
             * the decoder object MUST outlive the iterator. This variant is useful for tools that require an STFDecoder as part of their operation
             */
            STFTracepointIterator(STFInstReader& reader, STFDecoder& decoder) :
                it_(reader.begin()),
                end_it_(reader.end()),
                decoder_(&decoder)
            {
                findROI_();
            }

            /**
             * Copy constructor. Note that this is much more expensive if the iterator owns a decoder.
             */
            STFTracepointIterator(const STFTracepointIterator& rhs) :
                it_(rhs.it_),
                end_it_(rhs.end_it_)
            {
                if(rhs.owned_decoder_) {
                    owned_decoder_ = std::make_unique<STFDecoder>(*rhs.owned_decoder_);
                    decoder_ = owned_decoder_.get();
                }
                else {
                    decoder_ = rhs.decoder_;
                }
            }

            STFTracepointIterator(STFTracepointIterator&&) = default;

            /**
             * Copy assignment operator. Note that this is much more expensive if the iterator owns a decoder.
             */
            STFTracepointIterator& operator=(const STFTracepointIterator& rhs) {
                it_ = rhs.it_;
                end_it_ = rhs.end_it_;

                if(rhs.owned_decoder_) {
                    owned_decoder_ = std::make_unique<stf::STFDecoder>(*rhs.owned_decoder_);
                    decoder_ = owned_decoder_.get();
                }
                else {
                    owned_decoder_.reset();
                    decoder_ = rhs.decoder_;
                }

                found_roi_ = rhs.found_roi_;
                return *this;
            }

            STFTracepointIterator& operator=(STFTracepointIterator&&) = default;

            STFTracepointIterator& operator++() {
                updateROI_();
                ++it_;
                findROI_();
                return *this;
            }

            STFTracepointIterator operator++(int) {
                STFTracepointIterator temp(*this);
                ++(*this);
                return temp;
            }

            STFTracepointIterator& operator+=(const size_t num_items) {
                for(size_t i = 0; i < num_items && it_ != end_it_; ++i) {
                    ++(*this);
                }
                return *this;
            }

            bool operator==(const STFTracepointIterator& rhs) const {
                return it_ == rhs.it_ && end_it_ == rhs.end_it_;
            }

            bool operator!=(const STFTracepointIterator& rhs) const {
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
     * Gets a start iterator of the specified type. Currently specialized for STFInstReader::iterator and STFTracepointIterator
     */
    template<typename IteratorType>
    IteratorType getStartIterator(STFInstReader& reader, const size_t skip_count);

    /**
     * Alternative version that takes an external decoder object as an additional parameter
     */
    template<typename IteratorType>
    IteratorType getStartIterator(STFInstReader& reader, STFDecoder& decoder, const size_t skip_count);

    /**
     * Specialization for STFInstReader::iterator. Simply returns the begin() iterator, advanced by the given skip count
     */
    template<>
    inline STFInstReader::iterator getStartIterator<STFInstReader::iterator>(STFInstReader& reader,
                                                                             const size_t skip_count) {
        return reader.begin(skip_count);
    }

    /**
     * This variant is identical for STFInstReader::iterator because it does not use an STFDecoder anywhere
     */
    template<>
    inline STFInstReader::iterator getStartIterator(STFInstReader& reader,
                                                    STFDecoder&,
                                                    const size_t skip_count) {
        return getStartIterator<STFInstReader::iterator>(reader, skip_count);
    }

    /**
     * Specialization for STFTracepointIterator. First seeks to the first start tracepoint, then advances by skip_count instructions
     */
    template<>
    inline STFTracepointIterator getStartIterator<STFTracepointIterator>(STFInstReader& reader,
                                                                         const size_t skip_count) {
        auto it = STFTracepointIterator(reader);
        it += skip_count;
        return it;
    }

    /**
     * Specialization for STFTracepointIterator with an external STFDecoder. First seeks to the first start tracepoint,
     * then advances by skip_count instructions
     */
    template<>
    inline STFTracepointIterator getStartIterator<STFTracepointIterator>(STFInstReader& reader,
                                                                         STFDecoder& decoder,
                                                                         const size_t skip_count) {
        auto it = STFTracepointIterator(reader, decoder);
        it += skip_count;
        return it;
    }
}

