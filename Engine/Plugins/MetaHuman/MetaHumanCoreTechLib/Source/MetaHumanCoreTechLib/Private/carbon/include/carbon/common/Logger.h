// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Defs.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

enum class LogLevel
{
	VERBOSE = 0,
	WARNING = 1,
	INFO = 2,
	DEBUG = 3,
	ERR = 4,
	CRITICAL = 5,
	FATAL = 6
};

using LogFunction = void(*)(LogLevel /*logLevel*/, const char* /*format*/, ...);

/**
* @Brief Logger class is simple logging functionality wrapper. Its main purpose is integration 
* of logging functionality from external system (for example UE)
* 
*/
class EPIC_CARBON_API Logger final
{
public:
    /**
    * Default Constructor.
    *
    */
    Logger() noexcept;

    /**
    * Constructor.
    *
    * @param[in] logFunction_ logging function provided from the external system. Initially set to DefaultLogger.
    *
    */
    Logger(LogFunction logFunction_) noexcept;

    /**
    * Destructor.
    */
    ~Logger() noexcept;
    
    /**
    * Send message to log system.
    *
    * @param[in] logLevel Log level of message.
    * @param[in] format format string.
    * @param[in] params multiple parameters to be embeded in the format string.
    *
    * @note 
    *     C style formatting syntax needs to be applied 
    * 
    */
    template<typename ...ARGS>
    void Log(LogLevel logLevel, const char* format, ARGS&& ...params)   
    { 
        if (logLevel != LogLevel::VERBOSE || m_logVerbose)
        {
            m_logFunction(logLevel, format, params...);
        }
    }

    //! Set whether to log verbose output
    void SetVerbose(bool enable) { m_logVerbose = enable; }

private:
    static void DefaultLogger(LogLevel logLevel, const char* format, ...);
    
    LogFunction m_logFunction{ DefaultLogger };
    bool m_logVerbose{true};
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
