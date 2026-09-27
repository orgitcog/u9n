// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/TVariant.h"

template<class ResultType, class ErrorType>
class TResult
{
public:

    TResult(const ResultType& InReturnValue)
        : Result(TInPlaceType<ResultType>(), InReturnValue)
    {
    }

    TResult(ResultType&& InReturnValue)
        : Result(TInPlaceType<ResultType>(), MoveTemp(InReturnValue))
    {
    }

    TResult(const ErrorType& InError)
        : Result(TInPlaceType<ErrorType>(), InError)
    {
    }

    TResult(ErrorType&& InError)
        : Result(TInPlaceType<ErrorType>(), MoveTemp(InError))
    {
    }

    TResult(const TResult& InOther) = default;
    TResult(TResult&& InOther) = default;

    TResult& operator=(const TResult& InOther)
    {
        if (&InOther != this)
        {
            Result = InOther.Result;
        }

        return *this;
    }

    TResult& operator=(TResult&& InOther)
    {
        if (&InOther != this)
        {
            Result = MoveTemp(InOther.Result);
        }

        return *this;
    }

    ~TResult() = default;

    bool IsValid() const
    {
        return Result.template IsType<ResultType>();
    }

    bool IsError() const
    {
        return Result.template IsType<ErrorType>();
    }

    const ResultType& GetResult() const
    {
        check(IsValid());
        return Result.template Get<ResultType>();
    }

    ResultType& GetResult()
    {
        check(IsValid());
        return Result.template Get<ResultType>();
    }

    ResultType ClaimResult()
    {
        check(IsValid());
        return MoveTemp(Result.template Get<ResultType>());
    }

    const ErrorType& GetError() const
    {
        check(IsError());
        return Result.template Get<ErrorType>();
    }

	ErrorType& GetError()
    {
        check(IsError());
        return Result.template Get<ErrorType>();
    }

	ErrorType ClaimError()
    {
        check(IsError());
        return MoveTemp(Result.template Get<ErrorType>());
    }

    explicit operator bool()
    {
        return IsValid();
    }

private:

    TVariant<ResultType, ErrorType> Result;
};

struct FVoidResultTag
{
};

constexpr FVoidResultTag ResultOk = FVoidResultTag {};

template<class ErrorType>
class TResult<void, ErrorType>
{
public:

    TResult(FVoidResultTag)
        : bIsResultValid(true)
    {
    }

    TResult(const ErrorType& InError)
        : Error(InError)
        , bIsResultValid(false)
    {
    }

    TResult(ErrorType&& InError)
        : Error(MoveTemp(InError))
        , bIsResultValid(false)
    {
    }

    TResult(const TResult& InOther) = default;
    TResult(TResult&& InOther) = default;

    TResult& operator=(const TResult& InOther)
    {
        if (&InOther != this)
        {
            Error = InOther.Error;
            bIsResultValid = InOther.bIsResultValid;
        }

        return *this;
    }

    TResult& operator=(TResult&& InOther)
    {
        if (&InOther != this)
        {
            Error = MoveTemp(InOther.Error);
            bIsResultValid = InOther.bIsResultValid;
        }

        return *this;
    }

    ~TResult() = default;

    bool IsValid() const
    {
        return bIsResultValid;
    }

    bool IsError() const
    {
        return !bIsResultValid;
    }

    const ErrorType& GetError() const
    {
        check(IsError());
        return Error;
    }

	ErrorType& GetError()
    {
        check(IsError());
        return Error;
    }

	ErrorType ClaimError()
    {
        check(IsError());
        return MoveTemp(Error);
    }

    explicit operator bool()
    {
        return IsValid();
    }

private:

	ErrorType Error;
    bool bIsResultValid;
};