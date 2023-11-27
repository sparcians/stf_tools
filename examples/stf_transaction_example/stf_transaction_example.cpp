#include <iostream>

#include "stf_transaction_reader.hpp"
#include "stf_transaction_writer.hpp"
#include "protocols/tilelink.hpp"

void tilelink_example() {
    stf::STFTransactionWriter writer("test.zstf");
    writer.addTraceInfo(stf::TraceInfoRecord(stf::STF_GEN::STF_TRANSACTION_EXAMPLE, 1, 0, 0, "STF transaction example"));
    writer.setTraceFeature(stf::TRACE_FEATURES::STF_CONTAIN_TRANSACTIONS);
    writer.setProtocolId(stf::protocols::ProtocolId::TILELINK);
    writer.addClock(1, "system_clock");
    writer.finalizeHeader();

    stf::RecordIdManager id_manager;
    writer << stf::protocols::TileLink::makeTransaction<stf::protocols::tilelink::ChannelA>(id_manager, 1, 2, 3, 4, stf::type_utils::none, 5, stf::type_utils::none);
    writer << stf::protocols::TileLink::makeTransactionWithDelta<stf::protocols::tilelink::ChannelB>(id_manager, 10, 6, 7, 8, 9, std::vector<uint8_t>({0x1, 0x2, 0x3, 0x4}), 10, std::vector<uint8_t>({1, 0, 1, 0}));
    writer << stf::protocols::TileLink::makeTransactionWithDelta<stf::protocols::tilelink::ChannelC>(id_manager, 20, 11, 12, 13, 14, std::vector<uint8_t>({0x4, 0x3, 0x2, 0x1}), 15);
    writer << stf::protocols::TileLink::makeTransactionWithDelta<stf::protocols::tilelink::ChannelD>(id_manager, 30, 16, 17, 18, 19, std::vector<uint8_t>({0xab, 0xcd}), 20);
    writer << stf::protocols::TileLink::makeTransactionWithDelta<stf::protocols::tilelink::ChannelE>(id_manager, 40, 21);

    {
        auto transaction_with_metadata = stf::protocols::TileLink::makeTransaction<stf::protocols::tilelink::ChannelA>(id_manager, 1, 2, 3, 4, stf::type_utils::none, 5, stf::type_utils::none);

        transaction_with_metadata.getMetadata().append<uint8_t>(0xcc);
        transaction_with_metadata.getMetadata().append<uint8_t>(0xcc);
        transaction_with_metadata.getMetadata().append<uint8_t>(0xdd);
        transaction_with_metadata.getMetadata().append<uint8_t>(0xdd);
        transaction_with_metadata.getMetadata().append<uint8_t>(0xee);
        transaction_with_metadata.getMetadata().append<uint8_t>(0xee);
        transaction_with_metadata.getMetadata().append<uint8_t>(0xff);
        transaction_with_metadata.getMetadata().append<uint8_t>(0xff);

        writer << transaction_with_metadata;
    }

    // Same as the last transaction, but change the metadata type to uint64_t and append a single uint8_t
    {
        auto transaction_with_metadata = stf::protocols::TileLink::makeTransaction<stf::protocols::tilelink::ChannelA>(id_manager, 1, 2, 3, 4, stf::type_utils::none, 5, stf::type_utils::none);

        transaction_with_metadata.getMetadata().append<uint64_t>(0xffffeeeeddddcccc);
        transaction_with_metadata.getMetadata().append<uint8_t>(0xaa);

        writer << transaction_with_metadata;
    }

    writer.close();

    stf::STFTransactionReader reader("test.zstf", stf::protocols::ProtocolId::TILELINK);

    for(const auto& rec: reader) {
        std::cout << rec;
    }
}

int main(int argc, char** argv) {
    tilelink_example();
    return 0;
}
