// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Common.h>

#include <functional>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Most basic const-forwarding pointer wrapper for use with the Pimpl programming technique.
 * https://en.cppreference.com/w/cpp/language/pimpl.
 *
 * Use a custom delete Deleter = void(*)(T*) when the class does not have a destructor.
 * E.g. in the cpp class use Pimpl(std::unique_ptr<T, void(*)(T*)>(new T, [](T* ptr) {std::default_delete<T>()(ptr);}))
 *
 * @warning does not work with class that have a DLL interface.
 */
template <class T, class Deleter = std::default_delete<T>>
class Pimpl
{
    public:
        Pimpl(std::unique_ptr<T, Deleter>&& obj) : m_ptr(std::move(obj)) {}
        Pimpl(T* ptr) : m_ptr(ptr) {}
        ~Pimpl() = default;
        Pimpl(Pimpl&&) = default;
        Pimpl(const Pimpl&) = delete;
        Pimpl& operator=(Pimpl&&) = default;
        Pimpl& operator=(const Pimpl&) = delete;

        constexpr T* operator->() { return m_ptr.get(); }
        constexpr const T* operator->() const { return m_ptr.get(); }
        constexpr T& operator*() { return *m_ptr.get(); }
        constexpr const T& operator*() const { return *m_ptr.get(); }
        constexpr T* Get() { return m_ptr.get(); }
        constexpr const T* Get() const { return m_ptr.get(); }

    private:
        std::unique_ptr<T, Deleter> m_ptr;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
