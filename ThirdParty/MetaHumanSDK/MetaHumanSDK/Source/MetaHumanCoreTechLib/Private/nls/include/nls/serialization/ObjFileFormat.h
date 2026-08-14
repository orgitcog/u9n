// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//! from irlcpp code

#include <carbon/utils/TaskThreadPool.h>
#include <nls/math/Math.h>

#include <iostream>
#include <string>
#include <stdio.h>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class Mesh;


template <class T>
class ObjFileReader
{
public:
    static bool readObj(const std::string& fileName,
                        Mesh<T>& mesh,
                        const std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool>& taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/false));
    static bool readObjFromString(const std::string& data,
                                  Mesh<T>& mesh,
                                  const std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool>& taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/false));

    enum class ErrorType
    {
        NONE, LEXER, SYNTAX, FACES_WITHOUT_UVS, UNEXPECTED_TOK, UNEXPECTED_FACE_SIZE
    };

    static const char* ErrorTypeToString(ErrorType errorType);
};


template <class T>
class ObjFileWriter
{
public:
    static void writeObj(const Mesh<T>& mesh, const std::string& filename, bool withTexture = true, bool withNormals = false);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
