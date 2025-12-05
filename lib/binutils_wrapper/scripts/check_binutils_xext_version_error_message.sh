#!/bin/bash

function cmake_message()
{
    cmake -E env CLICOLOR_FORCE=1 cmake -E cmake_echo_color "$@"
}

function result_message()
{
    cmake_message --cyan --bold --no-newline "Checking if expected binutils X-extension version error message is present: " --newline "$@"
}

if [ $# -ne 1 ]; then
    echo "Usage: check_binutils_xext_version_error_message.sh <error message>"
    exit 1
fi

if [ -z "$1" ]; then
    echo "Expected error message must not be empty"
    exit 1
fi

strings libbfd.a | grep -q "$1"

if [ $? -eq 0 ]; then
    result_message --green --bold "Success"
else
    result_message --red --bold "Failed" \
        "Expected error message string \"$1\" not found in libbfd.a" \
        "Check riscv_parse_add_subset() in binutils/bfd/elfxx-riscv.c to see if it has changed"
    exit 1
fi
