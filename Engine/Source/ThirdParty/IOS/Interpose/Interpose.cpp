// Copyright Epic Games, Inc. All Rights Reserved.

#include <fcntl.h>

typedef int (*p_creat) (const char* path, mode_t mode);
static int replacement_creat(const char* path, mode_t mode)
{
    return creat(path, mode);
}

__attribute__((used)) static struct{ p_creat replacement; p_creat replacee; } interpose_creat
__attribute__ ((section ("__DATA,__interpose"))) = { replacement_creat, creat };
