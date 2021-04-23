#pragma once

#include <cmath>
#include <locale>
#include <ostream>
#include <string>

#include "format_utils.hpp"
#include "file_utils.hpp"

/**
 * \brief Gets the number of decimal digits needed to represent the value
 */
template<typename T>
inline T numDecimalDigits(const uint64_t value) {
    return static_cast<T>(floor(log10(static_cast<double>(value)))) + 1;
}

class CommaFormatter {
    private:
        /**
         * \class comma_numpunct
         * Locale formatter that inserts ',' characters to separate thousands in numbers
         */
        class comma_numpunct : public std::numpunct<char> {
          protected:
            char do_thousands_sep() const final {
                return ',';
            }

            std::string do_grouping() const final {
                return "\03";
            }
        };

        inline static const std::locale comma_locale_{std::locale(), new comma_numpunct()};

        std::ostream& os_;
        stf::format_utils::FlagSaver<std::ostream> flags_;

        template<typename T>
        using Manipulator = T& (*)(T&);

    public:
        explicit CommaFormatter(std::ostream& os) :
            os_(os),
            flags_(os)
        {
            os.imbue(comma_locale_);
        }

        explicit CommaFormatter(OutputFileStream& os) :
            CommaFormatter(os.getStream())
        {
        }

        template<typename T>
        typename std::enable_if<!std::is_scalar<T>::value, CommaFormatter&>::type
        operator<<(const T& rhs) {
            os_ << rhs;
            return *this;
        }

        template<typename T>
        typename std::enable_if<std::is_scalar<T>::value, CommaFormatter&>::type
        operator<<(const T rhs) {
            os_ << rhs;
            return *this;
        }

        CommaFormatter& operator<<(const char* rhs) {
            os_ << rhs;
            return *this;
        }

        CommaFormatter& operator<<(Manipulator<std::ostream> op) {
            os_ << op;
            return *this;
        }

        CommaFormatter& operator<<(Manipulator<std::basic_ios<char>> op) {
            os_ << op;
            return *this;
        }

        CommaFormatter& operator<<(Manipulator<std::ios_base> op) {
            os_ << op;
            return *this;
        }

        inline void saveFlags() {
            flags_.saveFlags();
        }

        inline void restoreFlags() {
            flags_.restoreFlags();
        }

        /**
         * \brief returns the width of a formatted number once commas are added
         */
        template<typename T>
        static inline constexpr T formattedWidth(const T width) {
            return width + width / 3;
        }
};
