#!/bin/bash

# Copyright Epic Games, Inc. All Rights Reserved.

self_dir=$(dirname $0)
working_dir=~/.ushell/.working

# provision Python using provision.sh
if ! $self_dir/provision.sh $working_dir; then
    echo Failed to provision Python
    exit 1
fi

"$working_dir/python/current/bin/python" -Esu "$self_dir/../core/system/boot.py" "$@"
if [ ! $? ]; then
    echo boot.py failed
    exit 1
fi
