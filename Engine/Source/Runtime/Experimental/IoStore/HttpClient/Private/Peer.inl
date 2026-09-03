// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !defined(IAS_HTTP_HAS_OPENSSL)
#	define IAS_HTTP_HAS_OPENSSL 0
#endif

#if IAS_HTTP_HAS_OPENSSL
#	if defined(IAS_HTTP_EXPLICIT_VERIFY_TIME)
#		if !IAS_HTTP_EXPLICIT_VERIFY_TIME 
#			error Either define this to >=1 or not at all
#		endif
#		include <HAL/PlatformTime.h>
#		include <ctime>
#	endif
#	include <openssl/engine.h>
#	include <openssl/err.h>
#	include <openssl/ssl.h>
#endif

namespace UE::IoStore::HTTP
{

////////////////////////////////////////////////////////////////////////////////
static FCertRoots GDefaultCertRoots;

struct ECertRootsRefType
{
	static const FCertRootsRef	None	= 0;
	static const FCertRootsRef	Default = ~0ull;
};



////////////////////////////////////////////////////////////////////////////////
class FPeer
{
public:
				FPeer() = default;
				FPeer(FSocket InSocket);
	FWaitable	GetWaitable() const					{ return Socket.GetWaitable(); }
	FOutcome	Send(const char* Data, int32 Size)	{ return Socket.Send(Data, Size); }
	FOutcome	Recv(char* Out, int32 MaxSize)		{ return Socket.Recv(Out, MaxSize); }
	bool		IsValid() const						{ return Socket.IsValid(); }

private:
	FSocket		Socket;
};

////////////////////////////////////////////////////////////////////////////////
FPeer::FPeer(FSocket InSocket)
: Socket(MoveTemp(InSocket))
{
}



#if IAS_HTTP_HAS_OPENSSL

////////////////////////////////////////////////////////////////////////////////
static void Ssl_ContextDestroy(UPTRINT Handle)
{
	auto* Context = (SSL_CTX*)Handle;
	SSL_CTX_free(Context);
}

////////////////////////////////////////////////////////////////////////////////
static UPTRINT Ssl_ContextCreate(FMemoryView PemData)
{
	if (static bool InitOnce = false; !InitOnce)
	{
		// While OpenSSL will lazily initialise itself, the defaults used will fail
		// initialisation on some platforms. So we have a go here. We do not register
		// anything for clean-up as we do not know if anyone else has done so.
		uint64 InitOpts = OPENSSL_INIT_NO_ATEXIT;
		OPENSSL_init_ssl(InitOpts, nullptr);
		InitOnce = true;
	}

	auto* Method = TLS_client_method();
	SSL_CTX* Context = SSL_CTX_new(Method);
	checkf(Context != nullptr, TEXT("ERR_get_error() == %d"), ERR_get_error());

	SSL_CTX_set_options(Context, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3);

	const void* Data = PemData.GetData();
	uint32 Size = uint32(PemData.GetSize());
	BIO* Bio = BIO_new_mem_buf(Data, Size);

	uint32 NumAdded = 0;
	while (true)
	{
		X509* FiveOhNine = PEM_read_bio_X509(Bio, nullptr, 0, nullptr);
		if (FiveOhNine == nullptr)
		{
			break;
		}

		X509_STORE* Store = SSL_CTX_get_cert_store(Context);
		int32 Result = X509_STORE_add_cert(Store, FiveOhNine);
		NumAdded += (Result == 1);

		X509_free(FiveOhNine);
	}

	BIO_free(Bio);

	if (NumAdded == 0)
	{
		SSL_CTX_free(Context);
		return 0;
	}

#if defined(IAS_HTTP_EXPLICIT_VERIFY_TIME)
	if (X509_VERIFY_PARAM* VerifyParam = SSL_CTX_get0_param(Context); VerifyParam != nullptr)
	{
		int32 AliasTown;
		std::tm Utc = {};
		FPlatformTime::UtcTime(
			Utc.tm_year, Utc.tm_mon,
			AliasTown,
			Utc.tm_mday, Utc.tm_hour, Utc.tm_min,
			AliasTown, AliasTown
		);

		Utc.tm_year -= 1900;
		Utc.tm_mon -= 1;

		time_t Now = std::mktime(&Utc);

		X509_VERIFY_PARAM_set_time(VerifyParam, Now);
	}
#endif

	return UPTRINT(Context);
}

////////////////////////////////////////////////////////////////////////////////
static int32 Ssl_ContextCertNum(UPTRINT Handle)
{
	auto* Context = (SSL_CTX*)Handle;
	X509_STORE* Store = SSL_CTX_get_cert_store(Context);
	STACK_OF(X509_OBJECT)* Objects = X509_STORE_get0_objects(Store);
	return sk_X509_OBJECT_num(Objects);
}

////////////////////////////////////////////////////////////////////////////////
static int32 Ssl_BioWrite(BIO* Bio, const char* Data, size_t Size, size_t* BytesWritten)
{
	*BytesWritten = 0;
	BIO_clear_retry_flags(Bio);

	auto* Peer = (FPeer*)BIO_get_data(Bio);
	FOutcome Outcome = Peer->Send(Data, Size);
	if (Outcome.IsWaiting())
	{
		BIO_set_retry_write(Bio);
		return 0;
	}

	if (Outcome.IsError())
	{
		return -1;
	}

	*BytesWritten = Outcome.GetResult();
	return 1;
}

////////////////////////////////////////////////////////////////////////////////
static int32 Ssl_BioRead(BIO* Bio, char* Data, size_t Size, size_t* BytesRead)
{
	*BytesRead = 0;
	BIO_clear_retry_flags(Bio);

	auto* Peer = (FPeer*)BIO_get_data(Bio);
	FOutcome Outcome = Peer->Recv(Data, Size);
	if (Outcome.IsWaiting())
	{
		BIO_set_retry_read(Bio);
		return 0;
	}

	if (Outcome.IsError())
	{
		return -1;
	}

	*BytesRead = Outcome.GetResult();
	return 1;
}

////////////////////////////////////////////////////////////////////////////////
static long Ssl_BioControl(BIO*, int Cmd, long, void*)
{
	return (Cmd == BIO_CTRL_FLUSH) ? 1 : 0;
}

////////////////////////////////////////////////////////////////////////////////
static SSL* Ssl_Create(FCertRootsRef Certs, const char* HostName=nullptr)
{
	static_assert(OPENSSL_VERSION_NUMBER >= 0x10100000L, "version supporting autoinit required");

	if (Certs == ECertRootsRefType::Default)
	{
		Certs = FCertRoots::Explicit(GDefaultCertRoots);
		check(Certs != 0);
	}
	auto* Context = (SSL_CTX*)Certs;

	static BIO_METHOD* BioMethod = nullptr;
	if (BioMethod == nullptr)
	{
		int32 BioId = BIO_get_new_index() | BIO_TYPE_SOURCE_SINK;
		BioMethod = BIO_meth_new(BioId, "IasBIO");
		BIO_meth_set_write_ex(BioMethod, Ssl_BioWrite);
		BIO_meth_set_read_ex(BioMethod,	Ssl_BioRead);
		BIO_meth_set_ctrl(BioMethod, Ssl_BioControl);
	}

	BIO* Bio = BIO_new(BioMethod);

	// SSL_MODE_ENABLE_PARTIAL_WRITE ??!!!

	SSL* Ssl = SSL_new(Context);
	SSL_set_connect_state(Ssl);
	SSL_set0_rbio(Ssl, Bio);
	SSL_set0_wbio(Ssl, Bio);
	BIO_up_ref(Bio);

	if (HostName != nullptr)
	{
		SSL_set_tlsext_host_name(Ssl, HostName);
	}

	return Ssl;
}

////////////////////////////////////////////////////////////////////////////////
static void Ssl_Destroy(SSL* Ssl)
{
	SSL_free(Ssl);
}

////////////////////////////////////////////////////////////////////////////////
static void Ssl_AssociatePeer(SSL* Ssl, FPeer* Peer)
{
	BIO* Bio = SSL_get_rbio(Ssl);
	check(Bio == SSL_get_wbio(Ssl));
	BIO_set_data(Bio, Peer);
}

////////////////////////////////////////////////////////////////////////////////
static void Ssl_SetupAlpn(SSL* Ssl, int32 /*MaxHttpVersion*/)
{
	static const uint8 AlpnProtos[] =
		"\x08" "http/1.1"
	//	"\x02" "h2"
		;
	SSL_set_alpn_protos(Ssl, AlpnProtos, sizeof(AlpnProtos) - 1);
}

////////////////////////////////////////////////////////////////////////////////
static int32 Ssl_GetProtocolVersion(SSL* Ssl)
{
	int32 Proto = -1;

	const char* AlpnProto = nullptr;
	uint32 AlpnProtoLen;
	SSL_get0_alpn_selected(Ssl, &(const uint8*&)AlpnProto, &AlpnProtoLen);
	if (AlpnProto == nullptr)
	{
		return Proto;
	}

	FAnsiStringView Needle(AlpnProto, AlpnProtoLen);
	FAnsiStringView Candidates[] = {
		"http/1.1",
	//	"h2",
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Candidates); ++i)
	{
		const FAnsiStringView& Candidate = Candidates[i];
		if (AlpnProtoLen != Candidate.Len())
		{
			continue;
		}

		if (Candidate != Needle)
		{
			continue;
		}

		Proto = i + 1;
		break;
	}

	return Proto;
}

////////////////////////////////////////////////////////////////////////////////
static FOutcome Ssl_GetOutcome(SSL* Ssl, int32 SslResult, const char* Message="tls error")
{
	int32 Error = SSL_get_error(Ssl, SslResult);
	if (Error != SSL_ERROR_WANT_READ && Error != SSL_ERROR_WANT_WRITE)
	{
		return FOutcome::Error(Message, Error);
	}
	return FOutcome::Waiting();
}

////////////////////////////////////////////////////////////////////////////////
static FOutcome Ssl_Handshake(SSL* Ssl)
{
	int32 Result = SSL_do_handshake(Ssl);
	if (Result == 0) return FOutcome::Error("unsuccessful tls handshake");
	if (Result != 1) return Ssl_GetOutcome(Ssl, Result, "tls handshake error");

	if (Result = SSL_get_verify_result(Ssl); Result != X509_V_OK)
	{
		return FOutcome::Error("x509 verification error", Result);
	}

	return FOutcome::Ok();
}

////////////////////////////////////////////////////////////////////////////////
static FOutcome Ssl_Write(SSL* Ssl, const char* Data, int32 Size)
{
	int32 Result = SSL_write(Ssl, Data, Size);
	return (Result > 0) ? FOutcome::Ok(Result) : Ssl_GetOutcome(Ssl, Result);
}

////////////////////////////////////////////////////////////////////////////////
static FOutcome Ssl_Read(SSL* Ssl, char* Out, int32 MaxSize)
{
	int32 Result = SSL_read(Ssl, Out, MaxSize);
	return (Result > 0) ? FOutcome::Ok(Result) : Ssl_GetOutcome(Ssl, Result);
}

#else

struct SSL;
static void		Ssl_ContextDestroy(...)		{}
static UPTRINT	Ssl_ContextCreate(...)		{ return 0; }
static int32	Ssl_ContextCertNum(...)		{ return 0; }
static int32	Ssl_BioWrite(...)			{ return -1; }
static int32	Ssl_BioRead(...)			{ return -1; }
static long		Ssl_BioControl(...)			{ return 0; }
static SSL*		Ssl_Create(...)				{ return nullptr; }
static void		Ssl_Destroy(...)			{}
static void		Ssl_AssociatePeer(...)		{}
static void		Ssl_SetupAlpn(...)			{}
static int32	Ssl_GetProtocolVersion(...)	{ return -1; }
static FOutcome	Ssl_GetOutcome(...)			{ return FOutcome::Error("!impl"); }
static FOutcome	Ssl_Handshake(...)			{ return FOutcome::Error("!impl"); }
static FOutcome	Ssl_Write(...)				{ return FOutcome::Error("!impl"); }
static FOutcome	Ssl_Read(...)				{ return FOutcome::Error("!impl"); }

#endif // IAS_HTTP_HAS_OPENSSL



////////////////////////////////////////////////////////////////////////////////
FCertRoots::~FCertRoots()
{
	if (Handle != 0)
	{
		Ssl_ContextDestroy(Handle);
	}
}

////////////////////////////////////////////////////////////////////////////////
FCertRoots::FCertRoots(FMemoryView PemData)
{
	Handle = Ssl_ContextCreate(PemData);
}

////////////////////////////////////////////////////////////////////////////////
int32 FCertRoots::Num() const
{
	if (Handle == 0)
	{
		return -1;
	}

	return Ssl_ContextCertNum(Handle);
}

////////////////////////////////////////////////////////////////////////////////
void FCertRoots::SetDefault(FCertRoots&& CertRoots)
{
	check(GDefaultCertRoots.IsValid() != CertRoots.IsValid());
	GDefaultCertRoots = MoveTemp(CertRoots);
}

////////////////////////////////////////////////////////////////////////////////
FCertRootsRef FCertRoots::NoTls()
{
	return ECertRootsRefType::None;
}

////////////////////////////////////////////////////////////////////////////////
FCertRootsRef FCertRoots::Default()
{
	return ECertRootsRefType::Default;
}

////////////////////////////////////////////////////////////////////////////////
FCertRootsRef FCertRoots::Explicit(const FCertRoots& CertRoots)
{
	check(CertRoots.IsValid());
	return CertRoots.Handle;
}



////////////////////////////////////////////////////////////////////////////////
class FTlsPeer
	: public FPeer
{
public:
				FTlsPeer()								= default;
				~FTlsPeer();
				FTlsPeer(FTlsPeer&& Rhs)				{ Move(MoveTemp(Rhs)); }
				FTlsPeer& operator = (FTlsPeer&& Rhs)	{ return Move(MoveTemp(Rhs)); }
				FTlsPeer(FSocket InSocket, FCertRootsRef Certs=ECertRootsRefType::None, const char* HostName=nullptr);
	FOutcome	Handshake();
	FOutcome	Send(const char* Data, int32 Size);
	FOutcome	Recv(char* Out, int32 MaxSize);

protected:
	FTlsPeer&	Move(FTlsPeer&& Rhs);
	SSL*		Ssl = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
FTlsPeer::FTlsPeer(FSocket InSocket, FCertRootsRef Certs, const char* HostName)
: FPeer(MoveTemp(InSocket))
{
	if (Certs == ECertRootsRefType::None)
	{
		return;
	}

	Ssl = Ssl_Create(Certs, HostName);
	Ssl_AssociatePeer(Ssl, this);
}

////////////////////////////////////////////////////////////////////////////////
FTlsPeer::~FTlsPeer()
{
	if (Ssl != nullptr)
	{
		Ssl_Destroy(Ssl);
	}
}

////////////////////////////////////////////////////////////////////////////////
FTlsPeer& FTlsPeer::Move(FTlsPeer&& Rhs)
{
	FPeer::operator = (MoveTemp(Rhs));

	Swap(Ssl, Rhs.Ssl);
	if (Ssl != nullptr)		Ssl_AssociatePeer(Ssl, this);
	if (Rhs.Ssl != nullptr) Ssl_AssociatePeer(Rhs.Ssl, &Rhs);

	return *this;
}

////////////////////////////////////////////////////////////////////////////////
FOutcome FTlsPeer::Handshake()
{
	if (Ssl == nullptr)
	{
		return FOutcome::Ok();
	}

	return Ssl_Handshake(Ssl);
}

////////////////////////////////////////////////////////////////////////////////
FOutcome FTlsPeer::Send(const char* Data, int32 Size)
{
	if (Ssl == nullptr)
	{
		return FPeer::Send(Data, Size);
	}

	return Ssl_Write(Ssl, Data, Size);
}

////////////////////////////////////////////////////////////////////////////////
FOutcome FTlsPeer::Recv(char* Out, int32 MaxSize)
{
	if (Ssl == nullptr)
	{
		return FPeer::Recv(Out, MaxSize);
	}

	return Ssl_Read(Ssl, Out, MaxSize);
}



////////////////////////////////////////////////////////////////////////////////
class FHttpPeer
	: public FTlsPeer
{
public:
				FHttpPeer() = default;
				FHttpPeer(FSocket InSocket, FCertRootsRef Certs=ECertRootsRefType::None, const char* HostName=nullptr);
	FOutcome	Handshake();

private:
	void		AssignProto();
	int32		Proto = 0;
};

////////////////////////////////////////////////////////////////////////////////
FHttpPeer::FHttpPeer(FSocket InSocket, FCertRootsRef Certs, const char* HostName)
: FTlsPeer(MoveTemp(InSocket), Certs, HostName)
{
	if (Ssl != nullptr)
	{
		Ssl_SetupAlpn(Ssl, 1);
	}
}

////////////////////////////////////////////////////////////////////////////////
FOutcome FHttpPeer::Handshake()
{
	FOutcome Outcome = FTlsPeer::Handshake();
	if (Outcome.IsOk())
	{
		AssignProto();
	}

	return Outcome;
}

////////////////////////////////////////////////////////////////////////////////
void FHttpPeer::AssignProto()
{
	Proto = 1;

	if (Ssl == nullptr)
	{
		return;
	}

	if (int32 Negotiated = Ssl_GetProtocolVersion(Ssl); Negotiated > 0)
	{
		Proto = Negotiated;
	}
}

} // namespace UE::IoStore::HTTP
