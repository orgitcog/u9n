// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/math/Math.h>
#include <nls/geometry/Mesh.h>

#include <stdio.h>
#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::io)

template <typename T>
inline bool ToBinaryFile(FILE* pFile, const T& value, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0)
{
    return (fwrite(&value, sizeof(T), 1, pFile) == 1);
}

template <typename T>
inline bool ToBinaryFile(FILE* pFile, const T& str, typename std::enable_if<std::is_same<T, std::string>::value, int>::type = 0)
{
    bool success = true;
    success &= ToBinaryFile<uint32_t>(pFile, (uint32_t)str.size());
    success &= (fwrite(str.data(), 1, str.size(), pFile) == str.size());
    return success;
}

//! Serializes a column-major matrix to binary format
template <class T, int R, int C>
bool ToBinaryFile(FILE* pFile, const Eigen::Matrix<T, R, C>& mat)
{
    bool success = true;
    int r = static_cast<int>(mat.rows());
    int c = static_cast<int>(mat.cols());
    success &= (fwrite(&r, sizeof(r), 1, pFile) == 1);
    success &= (fwrite(&c, sizeof(c), 1, pFile) == 1);
    if (mat.size() > 0)
    {
        success &= (fwrite(mat.data(), sizeof(T) * r * c, 1, pFile) == 1);
    }
    return success;
}

template <typename S, typename T>
inline bool ToBinaryFile(FILE* pFile, const std::map<S,T>& values);

template <typename T>
bool ToBinaryFile(FILE* pFile, const Mesh<T>& mesh);

template <typename T>
bool FromBinaryFile(FILE* pFile, Mesh<T>& mesh);

template <class T>
inline bool ToBinaryFile(FILE* pFile, const std::shared_ptr<const Mesh<T>>& mesh);

template <class T>
inline bool FromBinaryFile(FILE* pFile, std::shared_ptr<Mesh<T>>& mesh);

template <typename T>
inline bool ToBinaryFile(FILE* pFile, const std::vector<T>& values, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0)
{
    if (!ToBinaryFile<uint64_t>(pFile, (uint64_t)values.size())) return false;
    size_t itemsWritten= fwrite(values.data(), sizeof(T), values.size(), pFile);
    return (itemsWritten == values.size());
}

template <typename T>
bool ToBinaryFile(FILE* pFile, const std::vector<T>& values, typename std::enable_if<!std::is_arithmetic<T>::value, int>::type = 0)
{
    if (!ToBinaryFile<uint64_t>(pFile, (uint64_t)values.size())) return false;
    for (size_t i = 0;  i < values.size(); ++i)
    {
        if (!ToBinaryFile(pFile, values[i])) return false;
    }
    return true;
}

template <typename S, typename T>
inline bool ToBinaryFile(FILE* pFile, const std::map<S,T>& values)
{
    if (!ToBinaryFile<uint32_t>(pFile, (uint32_t)values.size())) return false;
    for (const auto&[key, value] : values)
    {
        if (!ToBinaryFile(pFile, key)) return false;
        if (!ToBinaryFile(pFile, value)) return false;
    }
    return true;
}


template <typename S, typename T>
inline bool ToBinaryFile(FILE* pFile, const std::pair<S, T>& pair)
{
    if (!ToBinaryFile(pFile, pair.first))
        return false;
    if (!ToBinaryFile(pFile, pair.second))
        return false;

    return true;
}


template <typename T>
inline bool FromBinaryFile(FILE* pFile, T& value, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0)
{
    return (fread(&value, sizeof(T), 1, pFile) == 1);
}

template <typename T>
bool FromBinaryFile(FILE* pFile, T& str, typename std::enable_if<std::is_same<T, std::string>::value, int>::type = 0)
{
    uint32_t size = 0;
    if (!FromBinaryFile<uint32_t>(pFile, size)) return false;
    str.resize(size);
    return (fread(str.data(), 1, size, pFile) == (size_t)size);
}

//! Deserializes a column-major matrix from binary format
template <class T, int R, int C>
bool FromBinaryFile(FILE* pFile, Eigen::Matrix<T, R, C>& mat)
{
    bool success = true;
    int r = 0;
    int c = 0;
    success &= (fread(&r, sizeof(r), 1, pFile) == 1);
    success &= (fread(&c, sizeof(c), 1, pFile) == 1);
    if constexpr (R >= 0)
    {
        if (r != R) { return false; }
    }
    if constexpr (C >= 0)
    {
        if (c != C) { return false; }
    }
    mat.resize(r, c);
    if (mat.size() > 0)
    {
        success &= (fread(mat.data(), sizeof(T) * r * c, 1, pFile) == 1);
    }
    return success;
}

template <typename S, typename T>
inline bool FromBinaryFile(FILE* pFile, std::map<S,T>& values);

template <typename T>
inline bool FromBinaryFile(FILE* pFile, std::vector<T>& values, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0)
{
    uint64_t size{};
    if (!FromBinaryFile<uint64_t>(pFile, size)) return false;
    values.resize(size);
    return (fread(values.data(), sizeof(T), values.size(), pFile) == values.size());
}

template <typename T>
bool FromBinaryFile(FILE* pFile, std::vector<T>& values, typename std::enable_if<!std::is_arithmetic<T>::value, int>::type = 0)
{
    uint64_t size{};
    if (!FromBinaryFile<uint64_t>(pFile, size)) return false;
    values.resize(size);
    for (size_t i = 0;  i < values.size(); ++i)
    {
        if (!FromBinaryFile(pFile, values[i])) return false;
    }
    return true;
}

template <typename S, typename T>
inline bool FromBinaryFile(FILE* pFile, std::map<S,T>& values)
{
    values.clear();
    uint32_t size;
    if (!FromBinaryFile<uint32_t>(pFile, size)) return false;
    for (uint32_t i = 0; i < size; ++i)
    {
        S key;
        T value;
        if (!FromBinaryFile(pFile, key)) return false;
        if (!FromBinaryFile(pFile, value)) return false;
        values.emplace(std::make_pair(std::move(key), std::move(value)));
    }
    return true;
}

template <typename S, typename T>
inline bool FromBinaryFile(FILE* pFile, std::pair<S, T>& pair)
{
    S key;
    T value;
    if (!FromBinaryFile(pFile, key))
        return false;
    if (!FromBinaryFile(pFile, value))
        return false;

    pair = std::make_pair(std::move(key), std::move(value));
    return true;
}


//! utility to read and check for a value, and revert in case the value is not valid
template <typename T>
inline bool ReadAndCheckOrRevertFromBinaryFile(FILE* pFile, const T& expected)
{
    T value{};
    size_t itemsRead = fread(&value, sizeof(T), 1, pFile);
    if (itemsRead != 1)
    {
        return false;
    }
    if (value != expected)
    {
        fseek(pFile, -(int)sizeof(T), SEEK_CUR);
        return false;
    }
    return true;
}


template <typename T>
bool ToBinaryFile(FILE* pFile, const Mesh<T>& mesh)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, mesh.m_version);
    success &= io::ToBinaryFile(pFile, mesh.m_vertices);
    success &= io::ToBinaryFile(pFile, mesh.m_tris);
    success &= io::ToBinaryFile(pFile, mesh.m_quads);
    success &= io::ToBinaryFile(pFile, mesh.m_normals);
    success &= io::ToBinaryFile(pFile, mesh.m_texcoords);
    success &= io::ToBinaryFile(pFile, mesh.m_tex_tris);
    success &= io::ToBinaryFile(pFile, mesh.m_tex_quads);
    return success;
}

template <typename T>
bool FromBinaryFile(FILE* pFile, Mesh<T>& mesh)
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile(pFile, version);
    if (success && version == 1)
    {
        success &= io::FromBinaryFile(pFile, mesh.m_vertices);
        success &= io::FromBinaryFile(pFile, mesh.m_tris);
        success &= io::FromBinaryFile(pFile, mesh.m_quads);
        success &= io::FromBinaryFile(pFile, mesh.m_normals);
        success &= io::FromBinaryFile(pFile, mesh.m_texcoords);
        success &= io::FromBinaryFile(pFile, mesh.m_tex_tris);
        success &= io::FromBinaryFile(pFile, mesh.m_tex_quads);
    }
    else
    {
        success = false;
    }
    return success;
}


// template specializations for serializing and deserializing shared ptrs to meshes
template <class T>
inline bool ToBinaryFile(FILE* pFile, const std::shared_ptr<const Mesh<T>>& mesh)
{
    bool bInitialized = mesh.get() != nullptr;
    if (!io::ToBinaryFile(pFile, bInitialized))
        return false;

    if (bInitialized)
    {
        if (!io::ToBinaryFile(pFile, *mesh))
            return false;
    }

    return true;
}


template <class T>
inline bool FromBinaryFile(FILE* pFile, std::shared_ptr<Mesh<T>>& mesh)
{
    bool bInitialized;
    if (!io::FromBinaryFile(pFile, bInitialized))
    {
        return false;
    }

    if (!bInitialized)
    {
        mesh = nullptr;
    }
    else
    {
        mesh = std::make_shared<Mesh<T>>();
        if (!io::FromBinaryFile(pFile, *mesh.get()))
        {
            return false;
        }
    }

    return true;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::io)
