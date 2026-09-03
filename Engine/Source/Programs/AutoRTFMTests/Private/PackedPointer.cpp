// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFM.h"
#include "Catch2Includes.h"

#include <inttypes.h>

#pragma pack(push,1)
struct Foo
{
    uint8_t a;
    uint64_t b;
};
#pragma pack(pop)

TEST_CASE("PackedPointer")
{
    Foo foo;
    foo.a = 1;
    foo.b = 2;
    AutoRTFM::Commit([&] ()
    {
        foo.a++;
        foo.b++;
    });
    REQUIRE(foo.a == 2);
    REQUIRE(foo.b == 3);
}

