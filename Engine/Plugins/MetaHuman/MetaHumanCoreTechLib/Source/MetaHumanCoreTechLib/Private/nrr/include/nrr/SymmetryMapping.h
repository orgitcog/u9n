// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>
#include <nls/math/Math.h>
#include <nls/serialization/EigenSerialization.h>

#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Simple helper class returning symmetry information
 */
class SymmetryMapping
{
public:
    SymmetryMapping() = default;
    SymmetryMapping(const Eigen::VectorXi& symmetries) : m_symmetries(symmetries)
    {
        if (!CheckSymmetries(symmetries))
        {
            CARBON_CRITICAL("invalid symmetry information");
        }
    }

    bool Load(const std::string& filename)
    {
        const std::string symmetryData = ReadFile(filename);
        const JsonElement jSymmetry = ReadJson(symmetryData);
        return Load(jSymmetry);
    }

    bool Load(const JsonElement& jSymmetry)
    {
        Eigen::VectorXi symmetries;
        if (jSymmetry.Contains("symmetry"))
        {
            io::FromJson(jSymmetry["symmetry"], symmetries);
        }
        else
        {
            CARBON_CRITICAL("no symmetry data");
        }

        if (!CheckSymmetries(symmetries))
        {
            CARBON_CRITICAL("invalid symmetry information");
        }

        m_symmetries = symmetries;

        return true;
    }

    int NumSymmetries() const { return int(m_symmetries.size()); }

    int Map(int vID) const { return m_symmetries[vID]; }

    bool IsSelfSymmetric(int vID) const { return m_symmetries[vID] == vID; }

    const Eigen::VectorXi& Symmetries() const { return m_symmetries; }

    static bool CheckSymmetries(const Eigen::VectorXi& symmetries)
    {
        for (int i = 0; i < int(symmetries.size()); ++i)
        {
            int other = symmetries[i];
            if ((other < 0) || (other >= int(symmetries.size())) || (symmetries[other] != i))
            {
                return false;
            }
        }
        return true;
    }

private:
    Eigen::VectorXi m_symmetries;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
