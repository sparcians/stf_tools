#include <unordered_set>

#include "stf_record_types.hpp"
#include "stf_reader.hpp"
#include "stf_writer.hpp"

#include "command_line_parser.hpp"
#include "file_utils.hpp"
#include "stf_descriptor_map.hpp"

using DescriptorSet = std::unordered_set<stf::descriptors::internal::Descriptor>;

void parseCommandLine(int argc,
                      char** argv,
                      DescriptorSet& stripped_records,
                      bool& overwrite,
                      int& compression_level,
                      std::string& input_trace,
                      std::string& output_filename) {
    trace_tools::CommandLineParser parser("stf_strip");
    parser.addMultiFlag('r', "type", "Record type to remove. Can be specified multiple times.");
    parser.addFlag('f', "Allow overwriting the output file");
    parser.addFlag('c', "#", "Compression level (ZSTD: 1-22, default 3)");
    parser.addPositionalArgument("trace", "Trace in STF format");
    parser.addPositionalArgument("output", "Output filename");
    parser.appendHelpText("Allowed record types:");

    // Used to filter out record types that cannot be stripped from a trace (either
    // because they aren't allowed in a trace to begin with, or because stripping them
    // would create an invalid trace)
    const auto is_allowed = [](const stf::descriptors::internal::Descriptor desc) {
        switch(desc) {
            case stf::descriptors::internal::Descriptor::STF_RESERVED:
            case stf::descriptors::internal::Descriptor::__RESERVED_END:
            case stf::descriptors::internal::Descriptor::STF_VERSION:
            case stf::descriptors::internal::Descriptor::STF_ISA:
            case stf::descriptors::internal::Descriptor::STF_INST_IEM:
            case stf::descriptors::internal::Descriptor::STF_END_HEADER:
                return false;
            default:
                return true;
        }
    };

    for(const auto& p: STF_DESCRIPTOR_NAME_MAP) {
        if(!is_allowed(p.second)) {
            continue;
        }

        parser.appendHelpText("    " + p.first);
    }

    parser.appendHelpText("\nExample:");
    parser.appendHelpText("    stf_strip -r STF_INST_REG -c 22 input.zstf output.zstf");

    parser.parseArguments(argc, argv);

    for(const auto& arg: parser.getMultipleValueArgument('r')) {
        const auto it = STF_DESCRIPTOR_NAME_MAP.find(arg);

        parser.assertCondition(it != STF_DESCRIPTOR_NAME_MAP.end() && is_allowed(it->second),
                               "Invalid record type specified: ", arg);

        stripped_records.insert(it->second);
    }

    parser.assertCondition(!stripped_records.empty(), "No records were specified for removal. Exiting.");

    overwrite = parser.hasArgument('f');
    parser.getArgumentValue('c', compression_level);
    parser.getPositionalArgument(0, input_trace);
    parser.getPositionalArgument(1, output_filename);
}

int main(int argc, char** argv) {
    DescriptorSet stripped_records;
    bool overwrite = false;
    std::string input_trace;
    std::string output_filename;
    int compression_level = -1;

    try {
        parseCommandLine(argc, argv, stripped_records, overwrite, compression_level, input_trace, output_filename);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    OutputFileManager outfile_man(overwrite);

    try {
        outfile_man.open(input_trace, output_filename);
    }
    catch(const OutputFileManager::FileExistsException& e) {
        std::cerr << e.what() << std::endl << "Specify -f if you want to overwrite." << std::endl;
        return 1;
    }

    stf::STFReader reader(input_trace);
    stf::STFWriter writer(outfile_man.getOutputName(), compression_level);

    const auto strip_record = [&stripped_records](const stf::descriptors::internal::Descriptor desc) {
        return stripped_records.count(desc) != 0;
    };

    // This section essentially duplicates the behavior of STFReader::copyHeader, but skips header records
    // that are being stripped.
    if(!strip_record(stf::descriptors::internal::Descriptor::STF_COMMENT)) {
        writer.addHeaderComments(reader.getHeaderComments());
    }

    // The ISA, IEM, and FORCE_PC records in the header are required by the spec
    writer.setISA(reader.getISA());
    writer.setHeaderIEM(reader.getInitialIEM());

    // We can't strip the FORCE_PC record from the header, so let the user know that this instance
    // will remain in the stripped trace
    if(strip_record(stf::descriptors::internal::Descriptor::STF_FORCE_PC)) {
        std::cerr << "INFO: Not stripping STF_FORCE_PC record found in trace header" << std::endl;
    }

    writer.setHeaderPC(reader.getInitialPC());

    if(!strip_record(stf::descriptors::internal::Descriptor::STF_TRACE_INFO)) {
        writer.addTraceInfoRecords(reader.getTraceInfo());
    }

    // Add a trace info record for this tool. If we stripped STF_TRACE_INFO, this will be the
    // only instance of that record type
    writer.addTraceInfo(
        stf::STF_GEN::STF_GEN_STF_STRIP,
        stf::STF_CUR_VERSION_MAJOR,
        stf::STF_CUR_VERSION_MINOR,
        0,
        // Generate a trace generator comment containing which record types were removed
        [&stripped_records] {
            std::ostringstream os;
            os << "removed: ";
            bool first = true;
            for(const auto desc: stripped_records) {
                if(first) {
                    first = false;
                }
                else {
                    os << ", ";
                }

                os << desc;
            }
            return os.str();
        }()
    );

    if(!strip_record(stf::descriptors::internal::Descriptor::STF_TRACE_INFO_FEATURE)) {
        writer.setTraceFeature(reader.getTraceFeatures()->getFeatures());
    }

    if(reader.hasISAExtendedInfoRecord() && !strip_record(stf::descriptors::internal::Descriptor::STF_ISA_EXTENDED)) {
        writer.setISAExtendedInfo(reader.getISAExtendedInfo());
    }

    const bool stripped_vlen = strip_record(stf::descriptors::internal::Descriptor::STF_VLEN_CONFIG);

    if(const auto vlen = reader.getVLen(); vlen != 0 && !stripped_vlen) {
        writer.setVLen(vlen);
    }

    // End of header setup
    writer.finalizeHeader();

    bool printed_vector_state_warning = false;

    try {
        stf::STFRecord::UniqueHandle r;
        while(reader) {
            reader >> r;
            if(STF_EXPECT_FALSE(strip_record(r->getId()))) {
                continue;
            }
            // If we're stripping VLEN_CONFIG, we also need to strip any vector-related register state records or else
            // the trace writer will assert
            else if(STF_EXPECT_FALSE(stripped_vlen && r->getId() == stf::descriptors::internal::Descriptor::STF_INST_REG)) {
                if(const auto& reg_rec = r->as<stf::InstRegRecord>(); reg_rec.isVector()) {
                    // If the vector record is not a state record, that means we have actual vector instructions in the trace
                    // and *cannot* strip the VLEN_CONFIG record without generating an invalid trace
                    stf_assert(reg_rec.getOperandType() == stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE,
                               "Trace contains vector instructions. Cannot strip VLEN_CONFIG from this trace.");

                    // Print an info message (once) to let the user know what happened
                    if(STF_EXPECT_FALSE(!printed_vector_state_warning)) {
                        std::cerr << "INFO: Stripping vector register state records because VLEN_CONFIG is stripped" << std::endl;
                        printed_vector_state_warning = true;
                    }
                    continue;
                }
            }

            writer << *r;
        }
    }
    catch(const stf::EOFException&) {
    }

    reader.close();
    writer.close();

    outfile_man.setSuccess();

    return 0;
}
