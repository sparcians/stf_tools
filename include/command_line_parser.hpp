#pragma once

#include <algorithm>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <getopt.h>

#include "format_utils.hpp"
#include "tools_util.hpp"
#include "stf_enum_utils.hpp"

namespace trace_tools {
    class CommandLineParser {
        private:
            class MultipleValueException : public std::exception {
            };

            static inline constexpr int NONFATAL_EXIT_CODE_ = 0;
            static inline constexpr int ERROR_EXIT_CODE_ = 1;

            static inline constexpr int DEFAULT_LONG_ARG_RETURN_VALUE_ = 0;
            static inline constexpr int DEFAULT_ENUM_RADIX_ = 0;
            static inline constexpr int DEFAULT_INTEGER_RADIX_ = 10;
            static inline constexpr int DEFAULT_NO_RADIX_ = -1;

            template<typename T, int RadixArg>
            static constexpr int getDefaultRadix_() {
                static_assert(!std::is_same_v<T, bool> || RadixArg == DEFAULT_NO_RADIX_,
                              "bool types do not take a Radix argument");

                static_assert(!std::is_same_v<T, std::string> || RadixArg == DEFAULT_NO_RADIX_,
                              "std::string types do not take a Radix argument");

                if constexpr(RadixArg != DEFAULT_NO_RADIX_) {
                    return RadixArg;
                }
                else if constexpr(std::is_same_v<T, bool>) {
                    return DEFAULT_NO_RADIX_;
                }
                else if constexpr(std::is_enum_v<T>) {
                    return DEFAULT_ENUM_RADIX_;
                }
                else if constexpr(std::is_integral_v<T>) {
                    return DEFAULT_INTEGER_RADIX_;
                }
                else {
                    return DEFAULT_NO_RADIX_;
                }
            }

            static constexpr const char* getFlagHyphens_(const bool is_long_flag) {
                if(is_long_flag) {
                    return "--";
                }
                else {
                    return "-";
                }
            }

            class BaseArgument {
                private:
                    static uint64_t getNextId_() {
                        static thread_local uint64_t next_id_ = 0;
                        return ++next_id_;
                    }

                protected:
                    const std::string help_message_;
                    const bool hidden_ = false;
                    const bool is_long_argument_ = false;
                    const uint64_t id_ = getNextId_();
                    bool required_ = false;
                    std::string required_error_message_;

                public:
                    BaseArgument() = default;

                    explicit BaseArgument(const std::string& help_message, const bool is_long_argument) :
                        help_message_(help_message),
                        hidden_(help_message_.empty()),
                        is_long_argument_(is_long_argument)
                    {
                    }

                    virtual ~BaseArgument() = default;

                    const std::string& getHelpMessage() const {
                        return help_message_;
                    }

                    bool isHidden() const {
                        return hidden_;
                    }

                    bool isLongArgument() const {
                        return is_long_argument_;
                    }

                    const char* getFlagHyphens() const {
                        return getFlagHyphens_(is_long_argument_);
                    }

                    virtual bool hasCountValue() const {
                        return false;
                    }

                    virtual bool hasNamedValue() const {
                        return false;
                    }

                    virtual bool hasMultipleValues() const {
                        return false;
                    }

                    virtual bool isSet() const = 0;

                    virtual void addValue(const char* arg) = 0;

                    virtual void checkRequired(const CommandLineParser& parser) const {
                        if(STF_EXPECT_FALSE(!isSet())) {
                            parser.raiseErrorWithHelp(required_error_message_);
                        }
                    }

                    void setRequired(const std::string& error_message) {
                        required_ = true;
                        required_error_message_ = error_message;
                    }
            };

            class Argument : virtual public BaseArgument {
                protected:
                    bool set_ = false;

                public:
                    Argument() = default;

                    explicit Argument(const std::string& help_message, const bool is_long_argument) :
                        BaseArgument(help_message, is_long_argument)
                    {
                    }

                    void addValue(const char* arg) override {
                        (void)arg;
                        if(STF_EXPECT_FALSE(set_)) {
                            throw MultipleValueException();
                        }
                        set_ = true;
                    }

                    bool isSet() const final {
                        return set_;
                    }
            };

            class NamedValueArgument : virtual public BaseArgument {
                protected:
                    const std::string name_;

                public:
                    explicit NamedValueArgument(const std::string& name) :
                        name_(name)
                    {
                    }

                    const std::string& getArgumentName() const {
                        return name_;
                    }

                    bool hasNamedValue() const final {
                        return true;
                    }
            };

            class ArgumentWithValue : public Argument, public NamedValueArgument {
                protected:
                    std::string value_;

                public:
                    ArgumentWithValue(const std::string& name, const std::string& help_message, const bool is_long_argument) :
                        BaseArgument(help_message, is_long_argument),
                        NamedValueArgument(name)
                    {
                    }

                    void addValue(const char* value) final {
                        Argument::addValue(value);
                        value_ = value;
                    }

                    template<typename T, int Radix = DEFAULT_NO_RADIX_>
                    const T& getValueAs() const {
                        constexpr int chosen_radix = getDefaultRadix_<T, Radix>();

                        if constexpr(std::is_same_v<T, std::string>) {
                            return value_;
                        }
                        else {
                            static thread_local std::unordered_map<uint64_t, T> parsed_values_;
                            auto it = parsed_values_.find(id_);

                            if(STF_EXPECT_FALSE(it == parsed_values_.end() || it->first != id_)) {
                                T parsed_value{};

                                if constexpr(std::is_integral_v<T> || std::is_enum_v<T>) {
                                    parsed_value = static_cast<T>(parseInt<T, chosen_radix>(value_));
                                }
                                else {
                                    std::istringstream temp(value_);
                                    temp >> parsed_value;
                                }

                                it = parsed_values_.emplace_hint(it, id_, parsed_value);
                            }

                            return it->second;
                        }
                    }

                    const std::string& getArgumentName() const {
                        return name_;
                    }
            };

            class MultiArgument : virtual public BaseArgument {
                private:
                    size_t count_ = 0;

                public:
                    explicit MultiArgument(const std::string& help_message, const bool is_long_argument) :
                        BaseArgument(help_message, is_long_argument)
                    {
                    }

                    void addValue(const char*) final {
                        ++count_;
                    }

                    bool isSet() const final {
                        return count_;
                    }

                    bool hasCountValue() const final {
                        return true;
                    }

                    size_t count() const {
                        return count_;
                    }
            };

            class MultiArgumentWithValues : public NamedValueArgument {
                protected:
                    std::deque<std::string> values_; // Using a deque ensures that pointers to the values will be stable

                public:
                    MultiArgumentWithValues(const std::string& name, const std::string& help_message, const bool is_long_argument) :
                        BaseArgument(help_message, is_long_argument),
                        NamedValueArgument(name)
                    {
                    }

                    void addValue(const char* value) final {
                        values_.emplace_back(value);
                    }

                    bool isSet() const final {
                        return !values_.empty();
                    }

                    const auto& getValues() const {
                        return values_;
                    }

                    bool hasMultipleValues() const final {
                        return true;
                    }
            };

            template<typename T> struct is_argument_with_value : std::is_base_of<NamedValueArgument, T> {};

            using ArgMap = std::unordered_map<std::string, std::unique_ptr<BaseArgument>>;

            // This class gives an ordered view of the arguments as the parser encountered them
            // Used by tools like stf_merge that have order-sensitive arguments
            class OrderedArgumentView {
                public:
                    class View {
                        private:
                            const std::string* const flag_;
                            const std::string* const value_;

                        public:
                            View(const std::string* flag, const std::string* value) :
                                flag_(flag),
                                value_(value)
                            {
                                stf_assert(flag != nullptr, "Can't construct a View with a null flag");
                            }

                            inline bool hasValue() const {
                                return value_ != nullptr;
                            }

                            inline const std::string& getFlag() const {
                                return *flag_;
                            }

                            inline const std::string& getValue() const {
                                stf_assert(hasValue(), "Attempted to get value of a value-less argument.");
                                return *value_;
                            }
                    };

                private:
                    using VecType = std::vector<View>;
                    using value_type = VecType::value_type;
                    VecType args_;

                public:
                    using iterator = VecType::iterator;

                    void add(const std::string& id, const BaseArgument& arg) {
                        const std::string* str_ptr = nullptr;

                        if(arg.hasMultipleValues()) {
                            str_ptr = &dynamic_cast<const MultiArgumentWithValues&>(arg).getValues().back();
                        }
                        else if(arg.hasNamedValue()) {
                            str_ptr = &dynamic_cast<const ArgumentWithValue&>(arg).getValueAs<std::string>();
                        }

                        args_.emplace_back(&id, str_ptr);
                    }

                    auto begin() const {
                        return args_.begin();
                    }

                    auto end() const {
                        return args_.end();
                    }
            };

            // Class that allows us to specify argument flags with a single character or a string
            // Single character flags are short flags (-a, -b) and string flags are long flags (--argument-a, --argument-b)
            class FlagString {
                private:
                    const std::string str_;
                    const bool is_long_argument_ = false;

                public:
                    FlagString(const char chr) :
                        str_(1, chr),
                        is_long_argument_(false)
                    {
                    }

                    FlagString(const std::string& str) :
                        str_(str),
                        is_long_argument_(true)
                    {
                    }

                    FlagString(const char* str) :
                        FlagString(std::string(str))
                    {
                    }

                    operator const std::string& () const {
                        return str_;
                    }

                    const std::string& get() const {
                        return str_;
                    }

                    size_t size() const {
                        return str_.size();
                    }

                    bool isLongArgument() const {
                        return is_long_argument_;
                    }

                    inline friend std::ostream& operator<<(std::ostream& os, const FlagString& flag) {
                        os << getFlagHyphens_(flag.is_long_argument_) << flag.str_;
                        return os;
                    }
            };

            class ArgumentDependency {
                private:
                    std::map<std::string, std::string> exclusive_; // Arguments that are mutually exclusive with this argument
                    std::map<std::string, std::string> dependencies_; // Arguments that this argument depends on

                public:
                    void addExclusive(const FlagString& other_arg, const std::string& error_msg) {
                        exclusive_.emplace(other_arg, error_msg);
                    }

                    void addDependency(const FlagString& other_arg, const std::string& error_msg) {
                        dependencies_.emplace(other_arg, error_msg);
                    }

                    void checkDependencies(const CommandLineParser& parser) const {
                        for(const auto& p: exclusive_) {
                            if(STF_EXPECT_FALSE(parser.hasArgument(p.first))) {
                                parser.raiseErrorWithHelp(p.second);
                            }
                        }

                        for(const auto& p: dependencies_) {
                            if(STF_EXPECT_FALSE(!parser.hasArgument(p.first))) {
                                parser.raiseErrorWithHelp(p.second);
                            }
                        }
                    }
            };

            const std::string program_name_;
            const bool allow_hidden_arguments_;

            using ArgInsertionVec = std::vector<std::pair<const std::string*, const BaseArgument*>>;

            ArgMap arguments_;
            ArgInsertionVec arguments_insertion_ordered_;

            std::vector<std::unique_ptr<NamedValueArgument>> positional_arguments_;
            OrderedArgumentView ordered_arguments_;
            std::unordered_set<std::string> active_arguments_;
            std::vector<const BaseArgument*> required_arguments_;
            std::stringstream help_addendum_; // String that will be appended to the end of the help string
            std::ostringstream arg_str_;
            std::vector<option> long_args_;
            std::unordered_map<std::string, std::string> short_to_long_map_; // Maps short flags to their corresponding long flags
            size_t max_argument_width_ = 1;
            bool parsed_ = false;

            using ArgumentDependencyMap = std::map<std::string, ArgumentDependency>;
            ArgumentDependencyMap argument_dependencies_;

            class InvalidArgumentException : public std::exception {
                private:
                    const std::string msg_;

                public:
                    InvalidArgumentException() = default;

                    explicit InvalidArgumentException(const std::string& msg) :
                        msg_(msg)
                    {
                    }

                    const char* what() const noexcept override {
                        return msg_.c_str();
                    }
            };

            inline const auto& getPositionalArgument_(const size_t idx) const {
                assertParsed_();

                const auto& a = positional_arguments_.at(idx);
                if(STF_EXPECT_FALSE(!a->isSet())) {
                    std::ostringstream ss;
                    ss << "Required argument " << a->getArgumentName() << " was not set!" << std::endl;
                    throw InvalidArgumentException(ss.str());
                }

                return a;
            }

            template<typename FlagType, bool is_long_and_short = false>
            void updateMaxArgumentWidth_(const FlagString& flag, const std::string& first_arg) {
                // Every argument needs at least 3 characters
                size_t flag_width = 3;

                if(flag.isLongArgument()) {
                    // Long arguments need to account for the argument name
                    flag_width += flag.size();
                    if constexpr(is_long_and_short) {
                        // Long args with alternative short flags need to account for a comma, space, hyphen, and short arg
                        flag_width += 4;
                    }
                }

                if constexpr(is_argument_with_value<FlagType>::value) {
                    // Arguments with values need a space, a '<', and a '>'
                    const size_t arg_width = first_arg.size() + 3;
                    flag_width += arg_width;

                    if constexpr(is_long_and_short) {
                        // Long args with alternative short flags will print the value name twice
                        flag_width += arg_width;
                    }
                }

                max_argument_width_ = std::max(max_argument_width_, flag_width);
            }

            void assertParsed_() const {
                stf_assert(parsed_, "Can't get argument values before we've parsed the arguments!");
            }

            void assertNotParsed_() const {
                stf_assert(!parsed_, "Can't alter the parser after we've already parsed the arguments!");
            }

            template<bool has_value>
            void addLongArg_(const char* name, const char short_flag) {
                long_args_.emplace_back(option{name, has_value ? required_argument : no_argument, nullptr, short_flag});
            }

            template<typename FlagType, typename ... FlagArgs>
            void addFlag_(const FlagString& flag, const std::string& first_arg, FlagArgs&&... args) {
                addFlag_<FlagType>(flag, DEFAULT_LONG_ARG_RETURN_VALUE_, first_arg, std::forward<FlagArgs>(args)...);
            }

            template<typename FlagType, typename ... FlagArgs>
            void addFlag_(const FlagString& flag, const char short_flag, const std::string& first_arg, FlagArgs&&... args) {
                constexpr bool has_value = is_argument_with_value<FlagType>::value;

                assertNotParsed_();

                const bool is_long_argument = flag.isLongArgument();

                // A long argument was specified with an alternative short flag
                if(short_flag != DEFAULT_LONG_ARG_RETURN_VALUE_) {
                    stf_assert(is_long_argument, "Can't have a short argument with an alternate short flag");

                    FlagString short_flag_str(short_flag);
                    // If a long argument has been specified with an alternate short flag, add it as a short option
                    addFlag_<FlagType>(short_flag_str, first_arg, std::forward<FlagArgs>(args)...);

                    // Add it to the short_to_long_map_ so we know what the long version of the argument is when we
                    // print the help message
                    const auto short_to_long_result = short_to_long_map_.emplace(short_flag_str, flag);

                    // Make sure we don't have any long arg collisions
                    if(STF_EXPECT_FALSE(!short_to_long_result.second)) {
                        std::ostringstream ss;
                        ss << "Attempted to add flag " << short_flag_str << " multiple times." << std::endl;
                        throw InvalidArgumentException(ss.str());
                    }

                    if(STF_EXPECT_FALSE(arguments_.count(flag) != 0)) {
                        std::ostringstream ss;
                        ss << "Attempted to add flag " << flag << " multiple times." << std::endl;
                        throw InvalidArgumentException(ss.str());
                    }

                    // Add it to long_args_ as well with the correct short_flag
                    addLongArg_<has_value>(short_to_long_result.first->second.c_str(), short_flag);

                    // Redo the argument width update to account for the extra flag
                    updateMaxArgumentWidth_<FlagType, true>(flag, first_arg);
                }
                else {
                    // This is either a short argument or a long argument with no alternate flags
                    const auto result = arguments_.emplace(flag, std::make_unique<FlagType>(first_arg, args..., is_long_argument));

                    if(STF_EXPECT_FALSE(!result.second)) {
                        std::ostringstream ss;
                        ss << "Attempted to add flag " << flag << " multiple times." << std::endl;
                        throw InvalidArgumentException(ss.str());
                    }

                    const auto& it = result.first;

                    if(STF_EXPECT_FALSE(it->second->isHidden() && !allow_hidden_arguments_)) {
                        std::ostringstream ss;
                        ss << "Attempted to add hidden flag " << flag << " without enabling hidden arguments." << std::endl;
                        throw InvalidArgumentException(ss.str());
                    }

                    if(is_long_argument) {
                        addLongArg_<has_value>(it->first.c_str(), short_flag);
                    }
                    else {
                        arg_str_ << flag.get();

                        if constexpr(has_value) {
                            arg_str_ << ':';
                        }
                    }

                    const BaseArgument* const arg_ptr = it->second.get();
                    arguments_insertion_ordered_.emplace_back(&it->first, arg_ptr);

                    // The first argument is the arg name for flags with values
                    updateMaxArgumentWidth_<FlagType>(flag, first_arg);
                }
            }

        public:
            class EarlyExitException : public std::exception {
                private:
                    const int code_ = 0;
                    const std::string msg_;

                public:
                    explicit EarlyExitException(const int code = 0, const char* message = nullptr) :
                        code_(code),
                        msg_(message ? message : "")
                    {
                    }

                    EarlyExitException(const int code, const std::string& message) :
                        code_(code),
                        msg_(message)
                    {
                    }

                    int getCode() const { return code_; }

                    const char* what() const noexcept override {
                        return msg_.c_str();
                    }
            };

            explicit CommandLineParser(const std::string& program_name, const bool allow_hidden_arguments = false) :
                program_name_(program_name),
                allow_hidden_arguments_(allow_hidden_arguments)
            {
                arg_str_ << ':';
                addFlag("help", 'h', "show this message");
                addFlag("version", 'V', "show the STF version the tool is built with");
            }

            // Adds a flag that does not take a value
            void addFlag(const FlagString& flag,
                         const std::string& help_message) {
                addFlag_<Argument>(flag, help_message);
            }

            // Adds a long flag with an alternative short flag that does not take a value
            void addFlag(const std::string& flag,
                         const char short_flag,
                         const std::string& help_message) {
                addFlag_<Argument>(flag, short_flag, help_message);
            }

            // Adds a flag that takes a value
            void addFlag(const FlagString& flag,
                         const std::string& arg_name,
                         const std::string& help_message) {
                addFlag_<ArgumentWithValue>(flag, arg_name, help_message);
            }

            // Adds a long flag with an alternative short flag that takes a value
            void addFlag(const std::string& flag,
                         const char short_flag,
                         const std::string& arg_name,
                         const std::string& help_message) {
                addFlag_<ArgumentWithValue>(flag, short_flag, arg_name, help_message);
            }

            // Adds a flag that does not take a value and can be specified multiple times
            void addMultiFlag(const FlagString& flag,
                              const std::string& help_message) {
                addFlag_<MultiArgument>(flag, help_message);
            }

            // Adds a long flag with an alternative short flag that does not take a value and can be specified multiple times
            void addMultiFlag(const std::string& flag,
                              const char short_flag,
                              const std::string& help_message) {
                addFlag_<MultiArgument>(flag, short_flag, help_message);
            }

            // Adds a flag that takes a value and can be specified multiple times
            void addMultiFlag(const FlagString& flag,
                              const std::string& arg_name,
                              const std::string& help_message) {
                addFlag_<MultiArgumentWithValues>(flag, arg_name, help_message);
            }

            // Adds a long flag with an alternative short flag that takes a value and can be specified multiple times
            void addMultiFlag(const std::string& flag,
                              const char short_flag,
                              const std::string& arg_name,
                              const std::string& help_message) {
                addFlag_<MultiArgumentWithValues>(flag, short_flag, arg_name, help_message);
            }

            void addPositionalArgument(const std::string& argument_name,
                                       const std::string& help_message,
                                       const bool multiple_value = false) {
                assertNotParsed_();

                if(STF_EXPECT_FALSE(!positional_arguments_.empty() &&
                                    positional_arguments_.back()->hasMultipleValues())) {
                    if(multiple_value) {
                        throw InvalidArgumentException("There can be at most one multiple-value positional argument.");
                    }

                    throw InvalidArgumentException("A multiple-value positional argument must be the last argument.");
                }

                std::unique_ptr<NamedValueArgument> new_arg;
                if(multiple_value) {
                    new_arg = std::make_unique<MultiArgumentWithValues>(argument_name, help_message, false);
                }
                else {
                    new_arg = std::make_unique<ArgumentWithValue>(argument_name, help_message, false);
                }

                if(STF_EXPECT_FALSE(new_arg->isHidden())) {
                    throw InvalidArgumentException(argument_name + " is hidden. Hidden positional arguments are never allowed.");
                }

                positional_arguments_.emplace_back(std::move(new_arg));
                max_argument_width_ = std::max(max_argument_width_, argument_name.size() + 1);
            }

            void getHelpMessage(std::ostream& os) const {
                static constexpr int TAB_WIDTH = 4;
                const int arg_field_width = static_cast<int>(max_argument_width_) + TAB_WIDTH;

                os << "usage: " << program_name_ << " [options]";

                for(const auto& a: positional_arguments_) {
                    os << " <" << a->getArgumentName() << '>';
                }

                os << std::endl;

                std::ostringstream arg_str;

                for(const auto& a: arguments_insertion_ordered_) {
                    if(a.second->isHidden()) {
                        continue;
                    }
                    stf::format_utils::formatSpaces(os, TAB_WIDTH);

                    arg_str << a.second->getFlagHyphens() << *a.first;

                    std::string named_arg_str;

                    if(a.second->hasNamedValue()) {
                        const auto named_arg = dynamic_cast<const NamedValueArgument*>(a.second);
                        named_arg_str = " <" + named_arg->getArgumentName() + '>';
                        arg_str << named_arg_str;
                    }

                    if(auto it = short_to_long_map_.find(*a.first); it != short_to_long_map_.end()) {
                        arg_str << ", " << getFlagHyphens_(true) << it->second;

                        if(!named_arg_str.empty()) {
                            arg_str << named_arg_str;
                        }
                    }

                    stf::format_utils::formatLeft(os, arg_str.str(), arg_field_width);
                    os << a.second->getHelpMessage() << std::endl;
                    arg_str.str("");
                }

                for(const auto& a: positional_arguments_) {
                    stf::format_utils::formatSpaces(os, TAB_WIDTH);
                    arg_str << '<' << a->getArgumentName() << '>';
                    stf::format_utils::formatLeft(os, arg_str.str(), arg_field_width);
                    os << a->getHelpMessage() << std::endl;
                    arg_str.str("");
                }

                os << help_addendum_.rdbuf() << std::endl;
            }

            std::string getHelpMessage() const {
                std::ostringstream ss;
                getHelpMessage(ss);
                return ss.str();
            }

            void parseArguments(const int argc, char** argv) {
                assertNotParsed_();

                long_args_.emplace_back(option{0,0,0,0});

                try {
                    int c;
                    opterr = 0;
                    int option_index = 0;
                    char short_arg[2]{'\0', '\0'};

                    while((c = getopt_long(argc, argv, arg_str_.str().c_str(), long_args_.data(), &option_index)) != -1) {
                        // Special handling for -h flag
                        if(STF_EXPECT_FALSE(c == 'h')) {
                            throw EarlyExitException(NONFATAL_EXIT_CODE_, getHelpMessage());
                        }

                        // Special handling for -V flag
                        if(STF_EXPECT_FALSE(c == 'V')) {
                            throw EarlyExitException(NONFATAL_EXIT_CODE_, getVersion());
                        }

                        const char* arg_str = nullptr;

                        if(c == DEFAULT_LONG_ARG_RETURN_VALUE_) {
                            arg_str = long_args_.at(static_cast<unsigned int>(option_index)).name;
                        }
                        else {
                            short_arg[0] = static_cast<char>(c);
                            arg_str = short_arg;
                        }

                        if(const auto it = arguments_.find(arg_str); STF_EXPECT_TRUE(it != arguments_.end())) {
                            it->second->addValue(optarg);
                            ordered_arguments_.add(it->first, *it->second);
                            active_arguments_.emplace(arg_str);
                            continue;
                        }

                        std::ostringstream ss;
                        const char* const error_arg = argv[optind-1];

                        if(c == ':') {
                            ss << "Option " << error_arg << " is missing an argument" << std::endl;
                        }
                        else if (c == '?') {
                            ss << "Unknown option specified: " << error_arg << std::endl;
                        }
                        else {
                            stf_throw("Argument parser is broken");
                        }

                        throw InvalidArgumentException(ss.str());
                    }

                    const int expected_arguments = optind + static_cast<int>(positional_arguments_.size());

                    if(STF_EXPECT_FALSE(argc > expected_arguments)) {
                        if(positional_arguments_.empty() || !positional_arguments_.back()->hasMultipleValues()) {
                            std::ostringstream ss;
                            ss << "Unknown arguments specified:";
                            for(int i = expected_arguments; i < argc; ++i) {
                                ss << ' ' << argv[i];
                            }
                            ss << std::endl;
                            throw InvalidArgumentException(ss.str());
                        }
                    }
                    if(STF_EXPECT_FALSE(argc < expected_arguments)) {
                        std::ostringstream ss;
                        const auto num_missing_arguments = static_cast<size_t>(expected_arguments - argc);
                        ss << "Required arguments were not specified:";
                        for(size_t i = positional_arguments_.size() - num_missing_arguments; i < positional_arguments_.size(); ++i) {
                            ss << ' ' << positional_arguments_[i]->getArgumentName();
                        }
                        ss << std::endl;
                        throw InvalidArgumentException(ss.str());
                    }
                }
                catch(const InvalidArgumentException& e) {
                    raiseErrorWithHelp(e.what());
                }

                for(size_t i = 0; i < positional_arguments_.size(); ++i) {
                    positional_arguments_[i]->addValue(argv[static_cast<size_t>(optind) + i]);
                }

                // Grab any trailing arguments if the last positional argument accepts multiple values
                if(!positional_arguments_.empty() && positional_arguments_.back()->hasMultipleValues()) {
                    const int start_idx = optind + static_cast<int>(positional_arguments_.size());
                    for(int i = start_idx; i < argc; ++i) {
                        positional_arguments_.back()->addValue(argv[i]);
                    }
                }

                parsed_ = true;

                // Check for any missing required arguments
                for(const auto arg: required_arguments_) {
                    arg->checkRequired(*this);
                }

                // Check for any mutually exclusive arguments or missing argument dependencies
                for(const auto& p: argument_dependencies_) {
                    if(hasArgument(p.first)) {
                        p.second.checkDependencies(*this);
                    }
                }
            }

            bool hasArgument(const FlagString& arg) const {
                assertParsed_();
                return active_arguments_.count(arg) != 0;
            }

            template<typename T, int Radix = DEFAULT_NO_RADIX_>
            bool getArgumentValue(const FlagString& arg, T& value) const {
                assertParsed_();

                const auto& a = arguments_.at(arg);
                if(STF_EXPECT_FALSE(a->hasMultipleValues())) {
                    throw InvalidArgumentException("Attempted to call getArgumentValue on a multi-value argument.");
                }

                if constexpr(std::is_same_v<T, bool>) {
                    value = a->isSet();

                    if(value && a->hasNamedValue()) {
                        const auto value_arg = dynamic_cast<const ArgumentWithValue*>(a.get());
                        value = value_arg->getValueAs<bool>();
                    }

                    return value;
                }
                else {
                    if(a->isSet()) {
                        if(STF_EXPECT_TRUE(a->hasNamedValue())) {
                            const auto value_arg = dynamic_cast<const ArgumentWithValue*>(a.get());

                            if constexpr(std::is_same_v<T, std::string_view>) {
                                value = value_arg->getValueAs<std::string>();
                            }
                            else {
                                value = value_arg->getValueAs<T, Radix>();
                            }

                            return true;
                        }

                        throw InvalidArgumentException("Attempted to get a non-boolean value from a boolean-only argument.");
                    }

                    return false;
                }
            }

            template<typename T, int Radix = DEFAULT_NO_RADIX_>
            void getPositionalArgument(const size_t idx, T& value) const {
                const auto& a = getPositionalArgument_(idx);
                if(STF_EXPECT_FALSE(a->hasMultipleValues())) {
                    throw InvalidArgumentException("Attempted to call getPositionalArgument on a multi-value argument.");
                }
                const auto pos_arg = static_cast<const ArgumentWithValue*>(a.get());

                value = pos_arg->getValueAs<T, Radix>();
            }

            const auto& getMultipleValueArgument(const FlagString& arg) const {
                const auto& a = arguments_.at(arg);
                if(STF_EXPECT_FALSE(!a->hasMultipleValues())) {
                    throw InvalidArgumentException("Attempted to call getMultipleValueArgument on a single-value argument.");
                }

                return dynamic_cast<const MultiArgumentWithValues*>(a.get())->getValues();
            }

            const auto& getMultipleValuePositionalArgument(const size_t idx) const {
                const auto& a = getPositionalArgument_(idx);
                if(STF_EXPECT_FALSE(!a->hasMultipleValues())) {
                    throw InvalidArgumentException("Attempted to call getMultipleValuePositionalArgument on a single-value argument.");
                }

                return dynamic_cast<const MultiArgumentWithValues*>(a.get())->getValues();
            }

            void appendHelpText(const std::string& append_str) {
                assertNotParsed_();
                help_addendum_ << std::endl << append_str;
            }

            void raiseErrorWithHelp(const std::string& msg) const {
                std::ostringstream ss;
                ss << msg << std::endl;
                getHelpMessage(ss);
                throw EarlyExitException(ERROR_EXIT_CODE_, ss.str());
            }

            /**
             * Throws an exception if the condition is false.
             */
            template<typename ... MsgArgs>
            inline void assertCondition(const bool cond, MsgArgs&&... msg_args) const {
                if(STF_EXPECT_FALSE(!cond)) {
                    std::ostringstream ss;
                    (ss << ... << std::forward<MsgArgs>(msg_args));
                    raiseErrorWithHelp(ss.str());
                }
            }

            void setMutuallyExclusive(const FlagString& arg1,
                                      const FlagString& arg2,
                                      const std::string& msg = {}) {
                assertNotParsed_();

                if(msg.empty()) {
                    std::ostringstream ss;
                    ss << arg1 << " and " << arg2 << " flags are mutually exclusive";
                    setMutuallyExclusive(arg1, arg2, ss.str());
                }
                else {
                    argument_dependencies_[arg1].addExclusive(arg2, msg);
                    argument_dependencies_[arg2].addExclusive(arg1, msg);
                }
            }

            void setDependentArgument(const FlagString& dependent_arg,
                                      const FlagString& dependency_arg,
                                      const std::string& msg = {}) {
                assertNotParsed_();

                if(msg.empty()) {
                    std::ostringstream ss;
                    ss << dependent_arg << " requires the " << dependency_arg << " flag";
                    setDependentArgument(dependent_arg, dependency_arg, ss.str());
                }
                else {
                    argument_dependencies_[dependent_arg].addDependency(dependency_arg, msg);
                }
            }

            void setRequired(const FlagString& flag,
                             const std::string& error_message = {}) {
                assertNotParsed_();

                const auto& arg = arguments_.at(flag);

                if(error_message.empty()) {
                    std::ostringstream ss;
                    ss << "Required argument " << flag << " was not specified.";
                    arg->setRequired(ss.str());
                }
                else {
                    arg->setRequired(error_message);
                }
            }

            auto begin() const {
                assertParsed_();
                return ordered_arguments_.begin();
            }

            auto end() const {
                assertParsed_();
                return ordered_arguments_.end();
            }
    };
} // end namespace trace_tools
