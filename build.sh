#!/bin/bash

SOURCEFILES="Kr/KrCommon.cpp Kr/KrBasic.cpp Kr/Main.cpp Kr/Json.cpp Kr/Network.cpp Kr/StringBuilder.cpp"
OUTPUTFILE=Katachi_Debug
GCCFLAGS="-g"
CLANGFLAGS="-gcodeview -Od"

if [ ! -d "./bin" ]; then
    mkdir bin
fi

if [ "$1" == "optimize" ]; then
    GCCFLAGS="-O2"
    CLANGFLAGS="-O2 -gcodeview"
	OUTPUTFILE=Katachi
fi

if command -v gcc &> /dev/null
then
	echo ------------------------------
	echo Building With GCC
	echo ------------------------------
    g++ -std=c++17 -Wno-switch -Wno-enum-conversion $GCCFLAGS $SOURCEFILES -o ./bin/$OUTPUTFILE -lssl -lcrypto
    exit
else
    echo GCC Not Found
    echo ------------------------------
fi

if command -v clang &> /dev/null
then
	echo ------------------------------
	echo Building With Clang
	echo ------------------------------
    clang++ -std=c++17 -Wno-switch -Wno-enum-conversion $CLANGFLAGS $SOURCEFILES -o ./bin/$OUTPUTFILE -lssl -lcrypto
    popd
else
    echo Clang Not Found
    echo ------------------------------
fi
