#include <iostream>

#include "stf_transaction_reader.hpp"
#include "stf_transaction_writer.hpp"
#include "protocols/tilelink.hpp"

int main(int argc, char** argv) {
    stf::STFTransactionWriter writer("test.zstf");
    writer.addTraceInfo(stf::TraceInfoRecord(stf::STF_GEN::STF_TRANSACTION_EXAMPLE, 1, 0, 0, "STF transaction example"));
    writer.setTraceFeature(stf::TRACE_FEATURES::STF_CONTAIN_TRANSACTIONS);
    writer.setProtocolId(stf::protocols::ProtocolId::TILELINK);
    writer.addClock(1, "system_clock");
    writer.finalizeHeader();

    writer << stf::protocols::TileLink::makeTransaction<stf::protocols::tilelink::ChannelA>(1, 2, 3, 4, 5);
    writer << stf::protocols::TileLink::makeTransactionWithDelta<stf::protocols::tilelink::ChannelB>(10, 6, 7, 8, 9, 10, std::vector<uint8_t>({0x1, 0x2, 0x3, 0x4}), std::vector<uint8_t>({0xff, 0xff, 0xff, 0xff}));
    writer << stf::protocols::TileLink::makeTransactionWithDelta<stf::protocols::tilelink::ChannelC>(20, 11, 12, 13, 14, 15, std::vector<uint8_t>({0x4, 0x3, 0x2, 0x1}));
    writer << stf::protocols::TileLink::makeTransactionWithDelta<stf::protocols::tilelink::ChannelD>(30, 16, 17, 18, 19, 20, std::vector<uint8_t>({0xab, 0xcd}));
    writer << stf::protocols::TileLink::makeTransactionWithDelta<stf::protocols::tilelink::ChannelE>(40, 21);

    writer.close();

    stf::STFTransactionReader reader("test.zstf", stf::protocols::ProtocolId::TILELINK);

    for(const auto& rec: reader) {
        std::cout << rec;
    }

    return 0;
}