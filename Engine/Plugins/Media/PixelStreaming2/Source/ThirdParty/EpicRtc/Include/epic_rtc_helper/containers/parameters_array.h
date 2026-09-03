// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include <string>
#include <map>
#include <vector>

// epic_rtc
#include "epic_rtc/containers/epic_rtc_array.h"
#include "epic_rtc/containers/epic_rtc_string_view.h"
#include "epic_rtc_helper/memory/ref_count_ptr.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

namespace EpicRtc
{
    // This is implementation helper for EpicRtcParameterPairArrayInterface.
    // User-code can use this header-based only implementation instead of writing their own bits.
    class ParametersArrayImpl : public EpicRtcParameterPairArrayInterface
    {
    public:
        inline static EpicRtc::RefCountPtr<ParametersArrayImpl> Create(const std::map<std::string, std::string>& parameters)
        {
            return EpicRtc::MakeRefCountPtr<ParametersArrayImpl>(parameters);
        }

        // EpicRtcParameterPairArrayInterface implementation
        const EpicRtcParameterPair* Get() const override
        {
            return _data_view.data();
        }
        EpicRtcParameterPair* Get() override
        {
            return _data_view.data();
        }
        uint64_t Size() const override {
            return _data_view.size();
        }

    private:
        explicit ParametersArrayImpl(const std::map<std::string, std::string>& inData)
            : _data(inData)
        {
            _data_view.reserve(_data.size());
            for (auto& [key, value] : _data)
            {
                EpicRtcStringView keyView{ key.c_str(), key.size() };
                EpicRtcStringView valueView{ value.c_str(), value.size() };
                EpicRtcParameterPair pair;
                pair._key = keyView;
                pair._value = valueView;
                _data_view.push_back(pair);
            }
        }

        ParametersArrayImpl() = delete;
        virtual ~ParametersArrayImpl() = default;

        std::map<std::string, std::string> _data;
        std::vector<EpicRtcParameterPair> _data_view;

        EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
    };

}  // namespace EpicRtc
