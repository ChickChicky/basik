#!/usr/bin/env bash

TGT="$OSTYPE"

C_WARNINGS="-Wall"
C_INPUT="./src/*.cpp"
C_INCLUDE="-I ./ -I ./src/"
C_EXTRA=""

C_OUTPUT_WIN="./out/basik.exe"
C_OUTPUT_LINUX="./out/basik"

C_LIB_WIN=""
C_LIB_LINUX=""

B_SUCCESS="\e[32mSUCCESS\e[39m: Built Basik"
B_ERROR_COMPILE="\e[31mERROR\e[39m: Could not build Basik"

ignorearg=""
for arg in "$@"; do
    if [[ $ignorearg == "target" ]] ; then
        TGT=$arg
    else
        if [[ $arg == "-O3" ]]; then
            C_EXTRA="$C_EXTRA -O3"
        elif [[ $arg == "-O2" ]]; then
            C_EXTRA="$C_EXTRA -O2"
        elif [[ $arg == "-O1" ]]; then
            C_EXTRA="$C_EXTRA -O1"
        elif [[ $arg == "-O0" ]]; then
            C_EXTRA="$C_EXTRA -O0"
        elif [[ $arg == "-Oz" ]]; then
            C_EXTRA="$C_EXTRA -O0"
        elif [[ $arg == "-target" ]] ; then
            ignorearg="target"
        elif [[ $arg == "-g" ]] ; then
            C_EXTRA="$C_EXTRA -g"
        fi
    fi
done

function runcmd {
    echo -e "\e[90m> $1\e[39m\n"
    return $($1)
}

if [[ "$TGT" == "msys" ]] ; then

    mkdir -p $(dirname $C_OUTPUT_WIN)

    win_cmd0="g++ $C_INPUT $C_WARNINGS -o $C_OUTPUT_WIN $C_INCLUDE $C_EXTRA $C_LIB_WIN"

    if ! runcmd "$win_cmd0" ; then
        echo -e "\n$B_ERROR_COMPILE"
        exit 1
    else
        echo -e "\n$B_SUCCESS \e[90m($C_OUTPUT_WIN)\e[39m"
        exit 0
    fi

elif [[ "$TGT" == "linux-gnu" ]] ; then

    mkdir -p $(dirname $C_OUTPUT_LINUX)

    linux_cmd0="g++ $C_INPUT $C_WARNINGS -o $C_OUTPUT_LINUX $C_INCLUDE $C_EXTRA -Wl,--copy-dt-needed-entries $C_LIB_LINUX"

    if ! runcmd "$linux_cmd0" ; then
        echo -e "\n$B_ERROR_COMPILE"
        exit 1
    else
        echo -e "\n$B_SUCCESS \e[90m($C_OUTPUT_LINUX)\e[39m"
        exit 0
    fi

else
    echo -e "\e[31mERROR\e[39m: Invalid build target \`$TGT\`"
    exit 1
fi
