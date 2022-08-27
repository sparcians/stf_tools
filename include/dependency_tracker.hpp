#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include "boost_wrappers/small_vector.hpp"

#include "stf_inst.hpp"

template<typename DerivedT, typename DependencyT>
class DependencyTracker {
    protected:
        class Consumer {
            private:
                uint64_t index_;

            public:
                explicit Consumer(const uint64_t index) :
                    index_(index)
                {
                }

                inline uint64_t getIndex() const {
                    return index_;
                }
        };

        using ConsumerMap = std::unordered_multimap<DependencyT, Consumer>;

        class Producer {
            private:
                std::vector<typename ConsumerMap::iterator> consumers_;
                uint64_t index_;

            public:
                explicit Producer(const uint64_t index) :
                    index_(index)
                {
                }

                inline void addConsumer(const typename ConsumerMap::iterator& consumer) {
                    consumers_.emplace_back(consumer);
                }

                inline const auto& getConsumers() const {
                    return consumers_;
                }

                inline uint64_t getIndex() const {
                    return index_;
                }
        };

        using ProducerMap = std::unordered_map<DependencyT, Producer>;

        const uint64_t max_distance_ = 0;
        ProducerMap producers_;
        ConsumerMap consumers_;

    private:
        inline auto removeProducer_(const typename ProducerMap::iterator producer_it) {
            for(const auto& it: producer_it->second.getConsumers()) {
                consumers_.erase(it);
            }
            return producers_.erase(producer_it);
        }

    protected:
        template<typename U = DependencyT>
        static inline typename std::enable_if<!std::is_same<U, stf::Registers::STF_REG>::value, bool>::type
        ignoreValue_(const U dep_value) {
            (void)dep_value;
            return false;
        }

        template<typename U = DependencyT>
        static inline typename std::enable_if<std::is_same<U, stf::Registers::STF_REG>::value, bool>::type
        ignoreValue_(const U dep_value) {
            return dep_value == stf::Registers::STF_REG::STF_REG_X0;
        }

        inline void addConsumer_(const DependencyT dep_value, const stf::STFInst& inst) {
            if(STF_EXPECT_FALSE(ignoreValue_(dep_value))) {
                return;
            }
            const auto consumer_it = consumers_.emplace(dep_value, inst.index());
            const auto producer_it = producers_.find(dep_value);
            //stf_assert(producer_it != producers_.end(), "Couldn't find producer for new consumer");
            if(producer_it != producers_.end() &&
               ((consumer_it->second.getIndex() - producer_it->second.getIndex()) <= max_distance_)) {
                producer_it->second.addConsumer(consumer_it);
            }
        }

        inline void addProducer_(const DependencyT dep_value, const stf::STFInst& inst) {
            if(STF_EXPECT_FALSE(ignoreValue_(dep_value))) {
                return;
            }
            auto result = producers_.try_emplace(dep_value, inst.index());
            if(!result.second) {
                const auto next_it = removeProducer_(result.first);
                const auto new_it = producers_.try_emplace(next_it, dep_value, inst.index());
                stf_assert(new_it != next_it, "Failed to insert producer");
            }
        }

        inline void removeProducer_(const DependencyT dep_value) {
            if(STF_EXPECT_FALSE(ignoreValue_(dep_value))) {
                return;
            }
            const auto it = producers_.find(dep_value);
            if(it != producers_.end()) {
                removeProducer_(it);
            }
        }

    public:
        using DistanceMap = std::map<uint64_t, DependencyT>;

        explicit DependencyTracker(const uint64_t max_distance) :
            max_distance_(max_distance)
        {
        }

        inline void track(const stf::STFInst& inst) {
            static_cast<DerivedT*>(this)->track_impl(inst);
        }

        inline DistanceMap getProducerDistances(const stf::STFInst& inst) const {
            return static_cast<const DerivedT*>(this)->getProducerDistances_impl(inst);
        }

        inline bool hasProducer(const stf::STFInst& inst) const {
            return static_cast<const DerivedT*>(this)->hasProducer_impl(inst);
        }
};

class RegisterDependencyTracker : public DependencyTracker<RegisterDependencyTracker, stf::Registers::STF_REG> {
    public:
        explicit RegisterDependencyTracker(const uint64_t max_distance) :
            DependencyTracker(max_distance)
        {
        }

        inline void track_impl(const stf::STFInst& inst) {
            for(const auto& op: inst.getSourceOperands()) {
                addConsumer_(op.getReg(), inst);
            }
            for(const auto& op: inst.getDestOperands()) {
                addProducer_(op.getReg(), inst);
            }
        }

        inline DistanceMap getProducerDistances_impl(const stf::STFInst& inst) const {
            DistanceMap distances;
            const uint64_t idx = inst.index();
            for(const auto& op: inst.getSourceOperands()) {
                try {
                    const auto reg = op.getReg();
                    const auto distance = idx - producers_.at(reg).getIndex();
                    if(distance <= max_distance_) {
                        distances.emplace(distance, reg);
                    }
                }
                catch(const std::out_of_range&) {
                }
            }
            return distances;
        }

        inline bool hasProducer_impl(const stf::STFInst& inst) const {
            const uint64_t idx = inst.index();
            for(const auto& op: inst.getSourceOperands()) {
                try {
                    const auto distance = idx - producers_.at(op.getReg()).getIndex();
                    if(distance <= max_distance_) {
                        return true;
                    }
                }
                catch(const std::out_of_range&) {
                }
            }
            return false;
        }
};

class StLdDependencyTracker : public DependencyTracker<StLdDependencyTracker, uint64_t> {
    private:
        const uint64_t address_mask_ = 0;

        inline uint64_t maskedAddress_(const uint64_t address) const {
            return address & address_mask_;
        }

    public:
        inline void track_impl(const stf::STFInst& inst) {
            for(const auto& m: inst.getMemoryReads()) {
                addConsumer_(maskedAddress_(m.getAddress()), inst);
            }

            for(const auto& m: inst.getMemoryWrites()) {
                addProducer_(maskedAddress_(m.getAddress()), inst);
            }
        }

        inline DistanceMap getProducerDistances_impl(const stf::STFInst& inst) const {
            DistanceMap distances;
            const uint64_t idx = inst.index();
            for(const auto& m: inst.getMemoryReads()) {
                try {
                    const auto masked_address = maskedAddress_(m.getAddress());
                    const auto distance = idx - producers_.at(masked_address).getIndex();
                    if(distance <= max_distance_) {
                        distances.emplace(distance, masked_address);
                    }
                }
                catch(const std::out_of_range&) {
                }
            }

            return distances;
        }

        inline bool hasProducer_impl(const stf::STFInst& inst) const {
            const uint64_t idx = inst.index();
            for(const auto& m: inst.getMemoryReads()) {
                try {
                    const auto masked_address = maskedAddress_(m.getAddress());
                    const auto distance = idx - producers_.at(masked_address).getIndex();
                    if(distance <= max_distance_) {
                        return true;
                    }
                }
                catch(const std::out_of_range&) {
                }
            }
            return false;
        }

        explicit StLdDependencyTracker(const uint64_t max_distance,
                                       const uint64_t address_mask = std::numeric_limits<uint64_t>::max()) :
            DependencyTracker(max_distance),
            address_mask_(address_mask)
        {
        }
};

class LdLdDependencyTracker : public DependencyTracker<LdLdDependencyTracker, stf::Registers::STF_REG> {
    public:
        explicit LdLdDependencyTracker(const uint64_t max_distance) :
            DependencyTracker(max_distance)
        {
        }

        inline void track_impl(const stf::STFInst& inst) {
            if(STF_EXPECT_FALSE(inst.isLoad())) {
                boost::container::small_vector<stf::Registers::STF_REG, 1> producers;

                for(const auto& op: inst.getSourceOperands()) {
                    addConsumer_(op.getReg(), inst);
                }

                for(const auto& op: inst.getDestOperands()) {
                    addProducer_(op.getReg(), inst);
                }
            }
            else {
                for(const auto& op: inst.getDestOperands()) {
                    removeProducer_(op.getReg());
                }
            }
        }

        inline DistanceMap getProducerDistances_impl(const stf::STFInst& inst) const {
            DistanceMap distances;
            if(inst.isLoad()) {
                const uint64_t idx = inst.index();
                for(const auto& op: inst.getSourceOperands()) {
                    try {
                        const auto reg = op.getReg();
                        if(STF_EXPECT_FALSE(ignoreValue_(reg))) {
                            continue;
                        }
                        const auto distance = idx - producers_.at(reg).getIndex();
                        if(distance <= max_distance_) {
                            distances.emplace(distance, reg);
                        }
                    }
                    catch(const std::out_of_range&) {
                    }
                }
            }
            return distances;
        }

        inline bool hasProducer_impl(const stf::STFInst& inst) const {
            if(inst.isLoad()) {
                const uint64_t idx = inst.index();
                for(const auto& op: inst.getSourceOperands()) {
                    try {
                        const auto reg = op.getReg();
                        if(STF_EXPECT_FALSE(ignoreValue_(reg))) {
                            continue;
                        }
                        const auto distance = idx - producers_.at(reg).getIndex();
                        if(distance <= max_distance_) {
                            return true;
                        }
                    }
                    catch(const std::out_of_range&) {
                    }
                }
            }
            return false;
        }
};
