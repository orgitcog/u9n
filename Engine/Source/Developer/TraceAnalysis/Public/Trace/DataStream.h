// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreFwd.h"
#include "HAL/Platform.h"
#include "HAL/Runnable.h"
#include "Templates/UniquePtr.h"

class IFileHandle;

namespace UE {
namespace Trace {

class IInDataStream
{
public:
	virtual			~IInDataStream() = default;

	/**
	 * Read bytes from the stream.
	 * @param Data Pointer to buffer to read into
	 * @param Size Maximum size that can be read in bytes
	 * @return Number of bytes read from the stream. Zero indicates end of stream and negative values indicate errors.
	 */
	virtual int32	Read(void* Data, uint32 Size) = 0;

	/**
	 * Close the stream. Reading from a closed stream
	 * is considered an error.
	 */
	virtual void	Close() {}
	
	/**
	 * Query if the stream is ready to read. Some streams may need to
	 * establish the data stream before reading can begin. A stream may not
	 * block indefinitely.
	 * 
	 * @return if the stream is ready to be read from
	 */
	virtual bool	WaitUntilReady() { return true; }
};

/*
* An implementation of IInDataStream that reads from a file on disk.
*/
class TRACEANALYSIS_API FFileDataStream : public IInDataStream
{
public:
	FFileDataStream();
	virtual ~FFileDataStream() override;

	/*
	* Open the file.
	* 
	* @param Path	The path to the file.
	* 
	* @return True if the file was opened successfully.
	*/
	bool Open(const TCHAR* Path);

	virtual int32 Read(void* Data, uint32 Size) override;
	virtual void Close() override;

private:
	TUniquePtr<IFileHandle> Handle;
	uint64 Remaining;
};

/**
 * Creates a stream to directly consume a trace stream from the tracing application. Sets up
 * a listening socket and stream is not considered ready until a connection is made.
 */
class TRACEANALYSIS_API FDirectSocketStream : public IInDataStream, public FRunnable
{
public:
	FDirectSocketStream();
	virtual ~FDirectSocketStream() override;

	/**
	 * Initiates listening sockets. Must be called before attempting to read from
	 * the stream.
	 * @return Port number used for listening
	 */
	uint16 StartListening();

private:
	enum
	{
		DefaultPort = 1986,			// Default port to use.
		MaxPortAttempts = 16,		// How many port increments are tried if fail to bind default.
		MaxQueuedConnections = 4,	// Size of connection queue.
	};

	// IInStream interface
	virtual bool WaitUntilReady() override;
	virtual int32 Read(void* Data, uint32 Size) override;
	virtual void Close() override;

	// FRunnable interface
	virtual uint32 Run() override;
	virtual void Stop() override;
	
	void Accept();
	bool CreateSocket(uint16 Port);

private:
	TUniquePtr<struct FDirectSocketContext> AsioContext;
	TUniquePtr<class FTraceDataStream> InternalStream;
	TUniquePtr<class FRunnableThread> ListeningThread;
	FEvent* ConnectionEvent;
};

} // namespace Trace
} // namespace UE
