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

#include <unistd.h>

#include "format_utils.hpp"
#include "tools_util.hpp"
#include "stf_enum_utils.hpp"

namespace trace_tools {
    class CommandLineParser {
        private:
            class MultipleValueException : public std::exception {
            };

            class BaseArgument {
                private:
                    static uint64_t getNextId_() {
                        static thread_local uint64_t next_id_ = 0;
                        return ++next_id_;
                    }

                protected:
                    const std::string help_message_;
                    const bool hidden_ = false;
                    const uint64_t id_ = getNextId_();

                public:
                    BaseArgument() = default;

                    explicit BaseArgument(std::string help_message) :
                        help_message_(std::move(help_message)),
                        hidden_(help_message_.empty())
                    {
                    }

                    virtual ~BaseArgument() = default;

                    const std::string& getHelpMessage() const {
                        return help_message_;
                    }

                    bool isHidden() const {
                        return hidden_;
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

                    virtual void checkRequired(const CommandLineParser&) const {
                    }
            };

            class Argument : virtual public BaseArgument {
                protected:
                    bool set_ = false;

                public:
                    Argument() = default;

                    explicit Argument(const std::string& help_message) :
                        BaseArgument(help_message)
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

            class RequiredArgumentOverlay : virtual public BaseArgument {
                protected:
                    std::string error_message_;

                public:
                    explicit RequiredArgumentOverlay(const std::string& error_message) :
                        error_message_(error_message)
                    {
                    }

                    void checkRequired(const CommandLineParser& parser) const final {
                        if(STF_EXPECT_FALSE(!isSet())) {
                            parser.raiseErrorWithHelp(error_message_);
                        }
                    }
            };

            class RequiredArgument : public Argument, public RequiredArgumentOverlay {
                public:
                    explicit RequiredArgument(const std::string& help_message, const std::string& error_message) :
                        Argument(help_message),
                        RequiredArgumentOverlay(error_message)
                    {
                    }
            };

            class NamedValueArgument : virtual public BaseArgument {
                protected:
                    const std::string name_;

                public:
                    explicit NamedValueArgument(std::string name) :
                        name_(std::move(name))
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
                    ArgumentWithValue(const std::string& name, const std::string& help_message) :
                        BaseArgument(help_message),
                        NamedValueArgument(name)
                    {
                    }

                    void addValue(const char* value) final {
                        Argument::addValue(value);
                        value_ = value;
                    }

                    template<typename T, int Radix = 0>
                    inline typename std::enable_if<std::is_enum<T>::value, T>::type
                    getValueAs() const {
                        static thread_local std::unordered_map<uint64_t, T> parsed_values_;
                        auto it = parsed_values_.find(id_);

                        if(STF_EXPECT_FALSE(it == parsed_values_.end() || it->first != id_)) {
                            it = parsed_values_.emplace_hint(it,
                                                             id_,
                                                             static_cast<T>(parseInt<stf::enums::int_t<T>, Radix>(value_)));
                        }

                        return it->second;
                    }

                    template<typename T, int Radix = 10>
                    inline typename std::enable_if<std::is_integral<T>::value && !std::is_enum<T>::value, T>::type
                    getValueAs() const {
                        static thread_local std::unordered_map<uint64_t, T> parsed_values_;
                        auto it = parsed_values_.find(id_);

                        if(STF_EXPECT_FALSE(it == parsed_values_.end() || it->first != id_)) {
                            it = parsed_values_.emplace_hint(it,
                                                             id_,
                                                             parseInt<T, Radix>(value_));
                        }

                        return it->second;
                    }

                    template<typename T>
                    inline typename std::enable_if<!std::is_integral<T>::value && !std::is_enum<T>::value && !std::is_same<T, std::string>::value, T>::type
                    getValueAs() const {
                        static thread_local std::unordered_map<uint64_t, T> parsed_values_;
                        auto it = parsed_values_.find(id_);

                        if(STF_EXPECT_FALSE(it == parsed_values_.end() || it->first != id_)) {
                            std::istringstream temp(value_);
                            T parsed_value;
                            temp >> parsed_value;
                            it = parsed_values_.emplace_hint(it,
                                                             id_,
                                                             parsed_value);
                        }

                        return it->second;
                    }

                    template<typename T>
                    inline typename std::enable_if<std::is_same<T, std::string>::value, const T&>::type
                    getValueAs() const {
                        return value_;
                    }

                    const std::string& getArgumentName() const {
                        return name_;
                    }
            };

            class RequiredArgumentWithValue : public ArgumentWithValue, public RequiredArgumentOverlay {
                public:
                    RequiredArgumentWithValue(const std::string& name,
                                              const std::string& help_message,
                                              const std::string& error_message) :
                        ArgumentWithValue(name, help_message),
                        RequiredArgumentOverlay(error_message)
                {
                }
            };

            class MultiArgument : virtual public BaseArgument {
                private:
                    size_t count_ = 0;

                public:
                    explicit MultiArgument(const std::string& help_message) :
                        BaseArgument(help_message)
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

            class RequiredMultiArgument : public MultiArgument, public RequiredArgumentOverlay {
                public:
                    RequiredMultiArgument(const std::string& help_message, const std::string& error_message) :
                        MultiArgument(help_message),
                        RequiredArgumentOverlay(error_message)
                    {
                    }
            };

            class MultiArgumentWithValues : public NamedValueArgument {
                protected:
                    std::deque<std::string> values_; // Using a deque ensures that pointers to the values will be stable

                public:
                    MultiArgumentWithValues(const std::string& name, const std::string& help_message) :
                        BaseArgument(help_message),
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

            class RequiredMultiArgumentWithValues : public MultiArgumentWithValues, public RequiredArgumentOverlay {
                public:
                    RequiredMultiArgumentWithValues(const std::string& name,
                                                    const std::string& help_message,
                                                    const std::string& error_message) :
                        MultiArgumentWithValues(name, help_message),
                        RequiredArgumentOverlay(error_message)
                    {
                    }
            };

            template<typename T> struct is_argument_with_value : std::is_base_of<NamedValueArgument, T> {};

            // This class gives an ordered view of the arguments as the parser encountered them
            // Used by tools like stf_merge that have order-sensitive arguments
            class OrderedArgumentView {
                public:
                    class View {
                        private:
                            std::pair<char, const std::string*> data_;

                        public:
                            template<typename ... Args>
                            View(Args&&... args) :
                                data_(std::forward<Args>(args)...)
                            {
                            }

                            inline bool hasValue() const {
                                return data_.second != nullptr;
                            }

                            inline char getFlag() const {
                                return data_.first;
                            }

                            inline const std::string& getValue() const {
                                stf_assert(hasValue(), "Attempted to get value of a value-less argument.");
                                return *data_.second;
                            }
                    };

                private:
                    using VecType = std::vector<View>;
                    using value_type = VecType::value_type;
                    VecType args_;

                public:
                    class iterator {
                        private:
                            VecType::const_iterator it_;

                        public:
                            iterator(const VecType& vec, const bool is_end = false) :
                                it_(is_end ? vec.end() : vec.begin())
                            {
                            }

                            inline iterator& operator++() {
                                ++it_;
                                return *this;
                            }

                            inline iterator operator++(int) {
                                const auto copy = *this;
                                operator++();
                                return copy;
                            }

                            inline bool operator==(const iterator& rhs) const {
                                return it_ == rhs.it_;
                            }

                            inline bool operator!=(const iterator& rhs) const {
                                return it_ != rhs.it_;
                            }

                            inline const value_type& operator*() const {
                                return *it_;
                            }

                            inline const value_type* operator->() const {
                                return it_.operator->();
                            }
                    };

                    void add(const char id, const BaseArgument& arg) {
                        const std::string* str_ptr = nullptr;

                        if(arg.hasMultipleValues()) {
                            str_ptr = &dynamic_cast<const MultiArgumentWithValues&>(arg).getValues().back();
                        }

                        args_.emplace_back(id, str_ptr);
                    }

                    auto begin() const {
                        return iterator(args_);
                    }

                    auto end() const {
                        return iterator(args_, true);
                    }
            };

            class ArgumentDependency {
                private:
                    std::map<char, std::string> exclusive_; // Arguments that are mutually exclusive with this argument
                    std::map<char, std::string> dependencies_; // Arguments that this argument depends on

                public:
                    void addExclusive(const char other_arg, const std::string& error_msg) {
                        exclusive_.emplace(other_arg, error_msg);
                    }

                    void addDependency(const char other_arg, const std::string& error_msg) {
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

            using ArgMap = std::unordered_map<char, std::unique_ptr<BaseArgument>>;
            using ArgInsertionVec = std::vector<ArgMap::iterator>;

            ArgMap arguments_;
            ArgInsertionVec arguments_insertion_ordered_;

            std::vector<std::unique_ptr<NamedValueArgument>> positional_arguments_;
            OrderedArgumentView ordered_arguments_;
            std::unordered_set<char> active_arguments_;
            std::set<char> required_arguments_;
            std::stringstream help_addendum_; // String that will be appended to the end of the help string
            std::ostringstream arg_str_;
            size_t max_argument_width_ = 1;

            using ArgumentDependencyMap = std::map<char, ArgumentDependency>;
            ArgumentDependencyMap argument_dependencies_;

            class InvalidArgumentException : public std::exception {
                private:
                    std::string msg_;

                public:
                    InvalidArgumentException() = default;

                    explicit InvalidArgumentException(std::string msg) :
                        msg_(std::move(msg))
                    {
                    }

                    const char* what() const noexcept override {
                        return msg_.c_str();
                    }
            };

            inline const auto& getPositionalArgument_(const size_t idx) const {
                const auto& a = positional_arguments_.at(idx);
                if(STF_EXPECT_FALSE(!a->isSet())) {
                    std::ostringstream ss;
                    ss << "Required argument " << a->getArgumentName() << " was not set!" << std::endl;
                    throw InvalidArgumentException(ss.str());
                }

                return a;
            }

            template<typename FlagType, typename ... FlagArgs>
            void addFlag_(const char flag,
                          const std::string& first_arg,
                          FlagArgs&&... args)
            {
                if(is_argument_with_value<FlagType>::value) {
                    arg_str_ << flag << ':';
                }
                else {
                    arg_str_ << flag;
                }
                const auto result = arguments_.emplace(flag, std::make_unique<FlagType>(first_arg, args...));
                if(STF_EXPECT_FALSE(!result.second)) {
                    std::ostringstream ss;
                    ss << "Attempted to add flag -" << flag << " multiple times." << std::endl;
                    throw InvalidArgumentException(ss.str());
                }

                if(STF_EXPECT_FALSE(result.first->second->isHidden() && !allow_hidden_arguments_)) {
                    std::ostringstream ss;
                    ss << "Attempted to add hidden flag -" << flag << " without enabling hidden arguments." << std::endl;
                    throw InvalidArgumentException(ss.str());
                }

                arguments_insertion_ordered_.emplace_back(result.first);

                // The first argument is the arg name for flags with values
                if(is_argument_with_value<FlagType>::value) {
                    max_argument_width_ = std::max(max_argument_width_, first_arg.size() + 3);
                }
            }

            template<typename FlagType, typename ... FlagArgs>
            void addRequiredFlag_(std::string_view error_message,
                                  const char flag,
                                  FlagArgs&&... args) {
                std::ostringstream ss;
                if(error_message.empty()) {
                    ss << "Required argument -" << flag << " was not specified.";
                    error_message = ss.str();
                }
                addFlag_<FlagType>(flag, args..., std::string(error_message));
                required_arguments_.emplace(flag);
            }

        public:
            class EarlyExitException : public std::exception {
                private:
                    int code_ = 0;
                    std::string msg_;

                public:
                    explicit EarlyExitException(int code = 0, const char* message = nullptr) :
                        code_(code),
                        msg_(message ? message : "")
                    {
                    }

                    EarlyExitException(int code, std::string message) :
                        code_(code),
                        msg_(std::move(message))
                    {
                    }

                    int getCode() const { return code_; }

                    const char* what() const noexcept override {
                        return msg_.c_str();
                    }
            };

            explicit CommandLineParser(std::string program_name, const bool allow_hidden_arguments = false) :
                program_name_(std::move(program_name)),
                allow_hidden_arguments_(allow_hidden_arguments)
            {
                addFlag('h', "show this message");
                addFlag('V', "show the STF version the tool is built with");
            }

            void addFlag(const char flag,
                         const std::string& arg_name,
                         const std::string& help_message,
                         const bool required = false,
                         std::string_view error_message = {}) {
                if(!required) {
                    addFlag_<ArgumentWithValue>(flag, arg_name, help_message);
                }
                else {
                    addRequiredFlag_<RequiredArgumentWithValue>(error_message, flag, arg_name, help_message);
                }
            }

            void addFlag(const char flag,
                         const std::string& arg_name,
                         const char* help_message,
                         const bool required = false,
                         std::string_view error_message = {}) {
                addFlag(flag, arg_name, std::string(help_message), required, error_message);
            }

            void addMultiFlag(const char flag,
                              const std::string& arg_name,
                              const std::string& help_message,
                              const bool required = false,
                              std::string_view error_message = {}) {
                if(!required) {
                    addFlag_<MultiArgumentWithValues>(flag, arg_name, help_message);
                }
                else {
                    addRequiredFlag_<RequiredMultiArgumentWithValues>(error_message, flag, arg_name, help_message);
                }
            }

            void addMultiFlag(const char flag,
                              const std::string& arg_name,
                              const char* help_message,
                              const bool required = false,
                              std::string_view error_message = {}) {
                addMultiFlag(flag, arg_name, std::string(help_message), required, error_message);
            }

            void addFlag(const char flag,
                         const std::string& help_message,
                         const bool required = false,
                         std::string_view error_message = {}) {
                if(!required) {
                    addFlag_<Argument>(flag, help_message);
                }
                else {
                    addRequiredFlag_<RequiredArgument>(error_message, flag, help_message);
                }
            }

            void addMultiFlag(const char flag,
                              const std::string& help_message,
                              const bool required = false,
                              std::string_view error_message = {}) {
                if(!required) {
                    addFlag_<MultiArgument>(flag, help_message);
                }
                else {
                    addRequiredFlag_<RequiredMultiArgument>(error_message, flag, help_message);
                }
            }

            void addPositionalArgument(const std::string& argument_name,
                                       const std::string& help_message,
                                       const bool multiple_value = false) {
                if(STF_EXPECT_FALSE(!positional_arguments_.empty() &&
                                    positional_arguments_.back()->hasMultipleValues())) {
                    if(multiple_value) {
                        throw InvalidArgumentException("There can be at most one multiple-value positional argument.");
                    }

                    throw InvalidArgumentException("A multiple-value positional argument must be the last argument.");
                }

                std::unique_ptr<NamedValueArgument> new_arg;
                if(multiple_value) {
                    new_arg = std::make_unique<MultiArgumentWithValues>(argument_name, help_message);
                }
                else {
                    new_arg = std::make_unique<ArgumentWithValue>(argument_name, help_message);
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

                os << "usage: " << program_name_;

                for(const auto& it: arguments_insertion_ordered_) {
                    const auto& a = *it;
                    if(a.second->isHidden()) {
                        continue;
                    }
                    os << " [-" << a.first;
                    if(a.second->hasNamedValue()) {
                        const auto named_arg = dynamic_cast<const NamedValueArgument*>(a.second.get());
                        os << ' ' << named_arg->getArgumentName();
                    }
                    os << ']';
                }

                for(const auto& a: positional_arguments_) {
                    os << " <" << a->getArgumentName() << '>';
                }

                os << std::endl;

                for(const auto& it: arguments_insertion_ordered_) {
                    const auto& a = *it;
                    if(a.second->isHidden()) {
                        continue;
                    }
                    stf::format_utils::formatSpaces(os, TAB_WIDTH);
                    os << '-';
                    std::ostringstream arg_str;
                    arg_str << a.first;
                    if(a.second->hasNamedValue()) {
                        const auto named_arg = dynamic_cast<const NamedValueArgument*>(a.second.get());
                        arg_str << " <" << named_arg->getArgumentName() << '>';
                    }
                    stf::format_utils::formatLeft(os, arg_str.str(), arg_field_width);
                    os << a.second->getHelpMessage() << std::endl;
                }

                for(const auto& a: positional_arguments_) {
                    stf::format_utils::formatSpaces(os, TAB_WIDTH);
                    os << '<';
                    stf::format_utils::formatLeft(os, a->getArgumentName() + '>', arg_field_width);
                    os << a->getHelpMessage() << std::endl;
                }

                os << help_addendum_.rdbuf() << std::endl;
            }

            std::string getHelpMessage() const {
                std::ostringstream ss;
                getHelpMessage(ss);
                return ss.str();
            }

            void parseArguments(int argc, char** argv) {
                try {
                    int c;
                    opterr = 0;
                    while((c = getopt(argc, argv, arg_str_.str().c_str())) != -1) {
                        const char c_char = static_cast<char>(c);

                        // Special handling for -h flag
                        if(STF_EXPECT_FALSE(c_char == 'h')) {
                            throw EarlyExitException(0, getHelpMessage());
                        }

                        // Special handling for -V flag
                        if(STF_EXPECT_FALSE(c_char == 'V')) {
                            throw EarlyExitException(0, getVersion());
                        }

                        if(const auto it = arguments_.find(c_char); STF_EXPECT_TRUE(it != arguments_.end())) {
                            it->second->addValue(optarg);
                            ordered_arguments_.add(c_char, *it->second);
                            active_arguments_.emplace(c_char);
                            continue;
                        }

                        stf_assert(c_char == '?', "Argument parser is broken");
                        std::ostringstream ss;
                        ss << "Unknown option specified: -" << static_cast<char>(optopt) << std::endl;
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

                // Check for any missing required arguments
                for(const auto arg: required_arguments_) {
                    arguments_.at(arg)->checkRequired(*this);
                }

                // Check for any mutually exclusive arguments or missing argument dependencies
                for(const auto& p: argument_dependencies_) {
                    if(hasArgument(p.first)) {
                        p.second.checkDependencies(*this);
                    }
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
            }

            bool hasArgument(const char arg) const {
                return active_arguments_.count(arg) != 0;
            }

            template<typename T>
            inline typename std::enable_if<!std::is_integral<T>::value && !std::is_enum<T>::value, bool>::type
            getArgumentValue(const char arg, T& value) const {
                const auto& a = arguments_.at(arg);
                if(STF_EXPECT_FALSE(a->hasMultipleValues())) {
                    throw InvalidArgumentException("Attempted to call getArgumentValue on a multi-value argument.");
                }

                if(a->isSet()) {
                    if(STF_EXPECT_TRUE(a->hasNamedValue())) {
                        const auto value_arg = dynamic_cast<const ArgumentWithValue*>(a.get());
                        value = value_arg->getValueAs<T>();
                        return true;
                    }

                    throw InvalidArgumentException("Attempted to get a non-boolean value from a boolean-only argument.");
                }

                return false;
            }

            template<typename T, int Radix = 0>
            inline typename std::enable_if<std::is_enum<T>::value, bool>::type
            getArgumentValue(const char arg, T& value) const {
                const auto& a = arguments_.at(arg);
                if(STF_EXPECT_FALSE(a->hasMultipleValues())) {
                    throw InvalidArgumentException("Attempted to call getArgumentValue on a multi-value argument.");
                }

                if(a->isSet()) {
                    if(STF_EXPECT_TRUE(a->hasNamedValue())) {
                        const auto value_arg = dynamic_cast<const ArgumentWithValue*>(a.get());
                        value = value_arg->getValueAs<T, Radix>();
                        return true;
                    }

                    throw InvalidArgumentException("Attempted to get a non-boolean value from a boolean-only argument.");
                }

                return false;
            }

            template<typename T, int Radix = 10>
            inline typename std::enable_if<std::is_integral<T>::value && !std::is_enum<T>::value, bool>::type
            getArgumentValue(const char arg, T& value) const {
                const auto& a = arguments_.at(arg);
                if(STF_EXPECT_FALSE(a->hasMultipleValues())) {
                    throw InvalidArgumentException("Attempted to call getArgumentValue on a multi-value argument.");
                }

                if(a->isSet()) {
                    if(STF_EXPECT_TRUE(a->hasNamedValue())) {
                        const auto value_arg = dynamic_cast<const ArgumentWithValue*>(a.get());
                        value = value_arg->getValueAs<T, Radix>();
                        return true;
                    }

                    throw InvalidArgumentException("Attempted to get a non-boolean value from a boolean-only argument.");
                }

                return false;
            }

            template<typename T>
            inline typename std::enable_if<!std::is_scalar<T>::value, const T&>::type
            getPositionalArgument(const size_t idx) const {
                const auto& a = getPositionalArgument_(idx);
                if(STF_EXPECT_FALSE(a->hasMultipleValues())) {
                    throw InvalidArgumentException("Attempted to call getPositionalArgument on a multi-value argument.");
                }
                const auto pos_arg = static_cast<const ArgumentWithValue*>(a.get());
                return pos_arg->getValueAs<T>();
            }

            template<typename T>
            inline typename std::enable_if<std::is_scalar<T>::value && !std::is_integral<T>::value, T>::type
            getPositionalArgument(const size_t idx) const {
                const auto& a = getPositionalArgument_(idx);
                if(STF_EXPECT_FALSE(a->hasMultipleValues())) {
                    throw InvalidArgumentException("Attempted to call getPositionalArgument on a multi-value argument.");
                }
                const auto pos_arg = static_cast<const ArgumentWithValue*>(a.get());
                return pos_arg->getValueAs<T>();
            }

            template<typename T, int Radix = 10>
            inline typename std::enable_if<std::is_integral<T>::value, T>::type
            getPositionalArgument(const size_t idx) const {
                const auto& a = getPositionalArgument_(idx);
                if(STF_EXPECT_FALSE(a->hasMultipleValues())) {
                    throw InvalidArgumentException("Attempted to call getPositionalArgument on a multi-value argument.");
                }
                const auto pos_arg = static_cast<const ArgumentWithValue*>(a.get());
                return pos_arg->getValueAs<T, Radix>();
            }

            template<typename T>
            inline void getPositionalArgument(const size_t idx, T& value) const {
                value = getPositionalArgument<T>(idx);
            }

            const auto& getMultipleValueArgument(const char arg) const {
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

            void appendHelpText(const std::string_view append_str) {
                help_addendum_ << std::endl << append_str;
            }

            void raiseErrorWithHelp(const std::string_view msg) const {
                std::ostringstream ss;
                ss << msg << std::endl;
                getHelpMessage(ss);
                throw EarlyExitException(1, ss.str());
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

            void setMutuallyExclusive(const char arg1, const char arg2) {
                std::ostringstream ss;
                ss << "-" << arg1 << " and -" << arg2 << " flags are mutually exclusive";
                setMutuallyExclusive(arg1, arg2, ss.str());
            }

            void setMutuallyExclusive(const char arg1, const char arg2, const std::string& msg) {
                argument_dependencies_[arg1].addExclusive(arg2, msg);
                argument_dependencies_[arg2].addExclusive(arg1, msg);
            }

            void setDependentArgument(const char dependent_arg, const char dependency_arg) {
                std::ostringstream ss;
                ss << "-" << dependent_arg << " requires the -" << dependency_arg << " flag";
                setDependentArgument(dependent_arg, dependency_arg, ss.str());
            }

            void setDependentArgument(const char dependent_arg, const char dependency_arg, const std::string& msg) {
                argument_dependencies_[dependent_arg].addDependency(dependency_arg, msg);
            }

            auto begin() const {
                return ordered_arguments_.begin();
            }

            auto end() const {
                return ordered_arguments_.end();
            }
    };

    template<>
    inline std::string_view CommandLineParser::ArgumentWithValue::getValueAs<std::string_view>() const {
        return value_;
    }

    template<>
    inline bool CommandLineParser::getArgumentValue(const char arg, bool& value) const {
        const auto& a = arguments_.at(arg);
        if(STF_EXPECT_FALSE(a->hasMultipleValues())) {
            throw InvalidArgumentException("Attempted to call getArgumentValue on a multi-value argument.");
        }

        value = a->isSet();
        if(value && a->hasNamedValue()) {
            const auto value_arg = dynamic_cast<const ArgumentWithValue*>(a.get());
            value = value_arg->getValueAs<bool>();
        }

        return value;
    }

} // end namespace trace_tools
