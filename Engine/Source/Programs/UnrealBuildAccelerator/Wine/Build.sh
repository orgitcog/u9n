#!/bin/sh
cd "$(dirname "$0")"
winegcc -shared -m64 Windows.c Linux.c Windows.def -o ../../../../Binaries/Win64/UnrealBuildAccelerator/x64/UbaWine.dll.so

