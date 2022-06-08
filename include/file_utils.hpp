#ifndef __FILE_UTILS_HPP__
#define __FILE_UTILS_HPP__

#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <unistd.h>
#include "filesystem.hpp"
#include "format_utils.hpp"
#include "stf_exception.hpp"

/**
 * Opens either stdout or a file depending on the value of output_filename
 * \param output_filename File to open. "-" will open stdout.
 * \param output_file std::ofstream object used to open file
 */
class OutputFileStream {
    private:
        std::ofstream of_;
        std::ostream os_;
        stf::format_utils::FlagSaver<std::ostream> flags_;
        bool is_stdout_ = false;

        template<typename T>
        using Manipulator = T& (*)(T&);

    public:
        explicit OutputFileStream(const std::string_view filename) :
            os_(nullptr),
            flags_(os_)
        {
            open(filename);
        }

        ~OutputFileStream() {
            close();
        }

        void open(const std::string_view output_filename) {
            std::streambuf* buf;
            if(output_filename == "-") {
                buf = std::cout.rdbuf();
                is_stdout_ = true;
            }
            else {
                of_.open(output_filename.data());
                stf_assert(!of_.fail(), "Failed to open " << output_filename << " for writing: " << strerror(errno));
                buf = of_.rdbuf();
                is_stdout_ = false;
            }

            os_.rdbuf(buf);
        }

        void close() {
            os_.rdbuf(nullptr);
            if(!is_stdout_) {
                of_.close();
            }
        }

        template<typename T>
        typename std::enable_if<!std::is_scalar<T>::value, OutputFileStream&>::type
        operator<<(const T& rhs) {
            os_ << rhs;
            return *this;
        }

        template<typename T>
        typename std::enable_if<std::is_scalar<T>::value, OutputFileStream&>::type
        operator<<(const T rhs) {
            os_ << rhs;
            return *this;
        }

        OutputFileStream& operator<<(const char* rhs) {
            os_ << rhs;
            return *this;
        }

        OutputFileStream& operator<<(Manipulator<std::ostream> op) {
            os_ << op;
            return *this;
        }

        OutputFileStream& operator<<(Manipulator<std::basic_ios<char>> op) {
            os_ << op;
            return *this;
        }

        OutputFileStream& operator<<(Manipulator<std::ios_base> op) {
            os_ << op;
            return *this;
        }

        const std::ostream& getStream() const {
            return os_;
        }

        std::ostream& getStream() {
            return os_;
        }

        inline void saveFlags() {
            flags_.saveFlags();
        }

        inline void restoreFlags() {
            flags_.restoreFlags();
        }

        bool isStdout() const {
            return is_stdout_;
        }
};

class OutputFileManager {
    private:
        static constexpr std::string_view TEMP_FILENAME_BASE_ = "/tmp/stfXXXXXX";
        const bool allow_overwrite_ = false;
        int tmp_fd_ = -1;
        std::string outfile_;
        std::string final_outfile_;
        bool success_ = false;

    public:
        class FileExistsException : public std::exception {
            private:
                std::string msg_;

            public:
                explicit FileExistsException(std::string msg) :
                    msg_(std::move(msg))
                {
                }

                const char* what() const noexcept override {
                    return msg_.c_str();
                }
        };

    private:
        void assertNonexistentOutput_(const std::string_view file) {
            if(STF_EXPECT_FALSE(!allow_overwrite_ && fs::exists(file))) {
                std::ostringstream msg;
                msg << "Output file " << file << " already exists.";
                throw FileExistsException(msg.str());
            }
        }

        void createTemporaryIfNeeded_(const std::string_view infile, const std::string_view outfile) {

            if(fs::equivalent(infile, outfile)) {
                const auto filename_ext = fs::path(outfile).extension().string();
                std::string temp_filename(TEMP_FILENAME_BASE_);
                temp_filename += filename_ext;

                tmp_fd_ = mkstemps(temp_filename.data(), static_cast<int>(filename_ext.size())); // Keep this handle open for now to ensure that nobody else gets this filename
                stf_assert(tmp_fd_ >= 0, "Failed to create temporary file: " << strerror(errno));
                final_outfile_ = outfile;
                outfile_ = temp_filename;
            }
            else {
                outfile_ = outfile;
            }
        }

        void copyTempToDestination_() const {
            try {
                fs::rename(outfile_, final_outfile_);
            }
            catch(const fs::filesystem_error& e) {
                if(e.code().value() != EXDEV) {
                    throw;
                }
                fs::copy_file(outfile_, final_outfile_, fs::copy_options::overwrite_existing);
                fs::remove(outfile_);
            }
        }

    public:
        explicit OutputFileManager(const bool allow_overwrite) :
            allow_overwrite_(allow_overwrite)
        {
        }

        ~OutputFileManager() {
            close();
        }

        void open(const std::string_view infile, const std::string_view outfile) {
            assertNonexistentOutput_(outfile);
            createTemporaryIfNeeded_(infile, outfile);
        }

        std::string_view getOutputName() const {
            return outfile_;
        }

        void close() {
            if(tmp_fd_ >= 0) {
                ::close(tmp_fd_);
                tmp_fd_ = -1;
                if(success_) {
                    copyTempToDestination_();
                }
            }
        }

        void setSuccess() {
            success_ = true;
        }
};

#endif
