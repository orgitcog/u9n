// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/common/Log.h>
#include <carbon/common/Pimpl.h>

#include <atomic>
#include <memory>
#include <ostream>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
    Abstract class to represent progress reporting implementation.

    Simple utility that can be implemented to act as a CLI progress
    reporter, or maybe part of a more complex GUI application.
*/
class AbstractProgressReporter {
    public:
        virtual ~AbstractProgressReporter() = default;

        /**
            Start tracking progress for a particular process.

            @param message Message that should describe the process.
        */
        virtual void StartProgress(const char* message) = 0;

        /**
            Set the progress to the current state.

            @param progress New progress value between [0, 1].
        */
        virtual void SetProgress(float progress) = 0;

        /**
            Mark the ending of a progress.
        */
        virtual void EndProgress() = 0;
};


/**
    A simple command-line interface progress reporter implementation.

    An example of a fully printed out CMD progress line:
    Reading images   |==================================================| 100%, done in 44.3064 seconds.

    It includes a single line progress update (equal signs '=' in the example above though the character can be changed),
    estimated time printout while progress is running, and elapsed time took to finish the process once it is done.
*/
class CLIProgressReporter : public AbstractProgressReporter {
    public:
        /**
            Default Constructor.

            @param barLength How many character will there be to represent the 100% process.
            @param barStart The offset character count, acting as a TAB character to denote
                the offset between the message and progress bar.
            @param barPin Character that is used to represent a single pin inside the progress bar.
        */
        CLIProgressReporter(unsigned int barLength = 50, unsigned int barStart = 40, char barPin = '=');

        virtual ~CLIProgressReporter();

        /// Silence the reporter output (ignore it's update to the stream)
        void Silence();

        /// Continue pushing printouts to the stream.
        void Unsilence();

        /**
            Set custom output stream to which the utility prints progress.

            By default, constructor sets this field to std::cout.

            @warning Stream instance must live as long as it is the lifetime of
                this instance. CLIProgressReporter does not take ownership of
                the stream instance.
        */
        void SetStream(std::ostream* stream);

        virtual void StartProgress(const char* message) override final;
        virtual void SetProgress(float progress) final override;
        virtual void EndProgress() final override;

    private:
        struct Impl;
        Pimpl<Impl> m_pimpl;

        void PrintProgress();
        void PrintProgressEnd();
};


/**
 * @brief Utility counting progress reporter that can be used to wrap a an AbstractProgressReporter to count
 * progress in increments.
 */
class IncrementingProgressReporter {
public:
    IncrementingProgressReporter(AbstractProgressReporter* progressReporter)
        : m_progressReporter(progressReporter)
        , m_numSteps(1)
        , m_count(0)
    {}

    /**
        Start tracking progress for a particular process.

        @param message Message that should describe the process.
        @param numSteps Defines how many steps / iterations this progress will contain.

        @note Based on numSteps update method calculates how much to advance the
            progress counters.
    */
    void StartProgress(const char* message, int numSteps)
    {
        m_numSteps = std::max<int>(1, numSteps);
        m_count = 0;
        if (m_progressReporter) m_progressReporter->StartProgress(message);
    };

    void Update()
    {
        if (m_progressReporter) {
            m_count.fetch_add(1);
            m_progressReporter->SetProgress((float)m_count/(float)m_numSteps);
        }
    }

    /**
        Mark the ending of a progress.
    */
    void EndProgress()
    {
        if (m_progressReporter) {
            m_progressReporter->EndProgress();
        }
    }

private:
    AbstractProgressReporter* m_progressReporter;
    std::atomic<int> m_numSteps;
    std::atomic<int> m_count;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
