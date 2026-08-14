// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

	#include "Logging.h"
	#include "Misc/AutomationTest.h"
	#include "PixelStreaming2PluginSettings.h"
	#include "TestUtils.h"
	#include "UtilsAsync.h"

namespace UE::PixelStreaming2
{
	void DoMediaDirectionTest(EMediaType Media, EMediaDirection Direction)
	{
		int32 StreamerPort = TestUtils::NextStreamerPort();
		int32 PlayerPort = TestUtils::NextPlayerPort();

		TSharedPtr<UE::PixelStreaming2Servers::IServer> SignallingServer = CreateSignallingServer(StreamerPort, PlayerPort);

		FString								 StreamerName(FString::Printf(TEXT("MockStreamer%d"), StreamerPort));
		TSharedPtr<IPixelStreaming2Streamer> Streamer = CreateStreamer(StreamerName, StreamerPort);
		TSharedPtr<FVideoProducer>			 VideoProducer = FVideoProducer::Create();
		Streamer->SetVideoProducer(VideoProducer);

		EMediaDirection PlayerMediaDirection = EMediaDirection::Bidirectional;
		if (Direction == EMediaDirection::SendOnly)
		{
			// Streamer is only sending so player should only receive
			PlayerMediaDirection = EMediaDirection::RecvOnly;
		}
		else if (Direction == EMediaDirection::RecvOnly)
		{
			// Streamer is only receiving so player should only send
			PlayerMediaDirection = EMediaDirection::SendOnly;
		}
		else if (Direction == EMediaDirection::Disabled)
		{
			// Streamer has disabled media so player disables it too
			PlayerMediaDirection = EMediaDirection::Disabled;
		}

		FMockPlayerConfig PlayerConfig = { .AudioDirection = EMediaDirection::Disabled, .VideoDirection = EMediaDirection::Disabled };
		if (Media == EMediaType::Audio)
		{
			PlayerConfig.AudioDirection = PlayerMediaDirection;
		}
		else if (Media == EMediaType::Video)
		{
			PlayerConfig.VideoDirection = PlayerMediaDirection;
		}

		TSharedPtr<FMockPlayer> Player = CreatePlayer(PlayerConfig);

		TSharedPtr<bool> bStreamingStarted = MakeShared<bool>(false);
		Streamer->OnStreamingStarted().AddLambda([bStreamingStarted](IPixelStreaming2Streamer*) {
			*(bStreamingStarted.Get()) = true;
		});

		TSharedPtr<bool> bLocalTrack = MakeShared<bool>(false);
		TSharedPtr<bool> bRemoteTrack = MakeShared<bool>(false);

		ADD_LATENT_AUTOMATION_COMMAND(FExecuteLambda([Streamer]() { Streamer->StartStreaming(); }))
		ADD_LATENT_AUTOMATION_COMMAND(FWaitAndCheckStreamerBool(TEXT("Check streaming started"), 5.0, Streamer, bStreamingStarted, true))
		ADD_LATENT_AUTOMATION_COMMAND(FExecuteLambda([Player, PlayerPort]() { Player->Connect(PlayerPort); }))
		ADD_LATENT_AUTOMATION_COMMAND(FSubscribePlayerAfterStreamerConnectedOrTimeout(5.0, Streamer, Player, StreamerName))
		// Wait 5 seconds to ensure connection is fully established
		ADD_LATENT_AUTOMATION_COMMAND(FWaitSeconds(5.0))

		if (Media == EMediaType::Audio)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FExecuteLambda([Player, bLocalTrack, bRemoteTrack]() {
				*(bLocalTrack.Get()) = Player->GetHasLocalAudioTrack();
				*(bRemoteTrack.Get()) = Player->GetHasRemoteAudioTrack();
			}))
		}
		else if (Media == EMediaType::Video)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FExecuteLambda([Player, bLocalTrack, bRemoteTrack]() {
				*(bLocalTrack.Get()) = Player->GetHasLocalVideoTrack();
				*(bRemoteTrack.Get()) = Player->GetHasRemoteVideoTrack();
			}))
		}

		if (Direction == EMediaDirection::SendOnly)
		{
			// Check that the player has only a remote track
			ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerTrackOrTimeout(5.0, Player, bRemoteTrack))
			ADD_LATENT_AUTOMATION_COMMAND(FWaitAndCheckBool(5.0, Player, bLocalTrack, false))
		}
		else if (Direction == EMediaDirection::RecvOnly)
		{
			// Check that the player has only a local track
			ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerTrackOrTimeout(5.0, Player, bLocalTrack))
			ADD_LATENT_AUTOMATION_COMMAND(FWaitAndCheckBool(5.0, Player, bRemoteTrack, false))
		}
		else if (Direction == EMediaDirection::Bidirectional)
		{
			// Check that the player has both a local and remote track
			ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerTrackOrTimeout(5.0, Player, bLocalTrack))
			ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerTrackOrTimeout(5.0, Player, bRemoteTrack))
		}
		else if (Direction == EMediaDirection::Disabled)
		{
			// Wait 5 seconds and then check that the player has no local and no remote track
			ADD_LATENT_AUTOMATION_COMMAND(FWaitAndCheckBool(5.0, Player, bLocalTrack, false))
			ADD_LATENT_AUTOMATION_COMMAND(FWaitAndCheckBool(5.0, Player, bRemoteTrack, false))
		}

		ADD_LATENT_AUTOMATION_COMMAND(FCleanupAll(SignallingServer, Streamer, Player))
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2AudioSendOnlyTest, "System.Plugins.PixelStreaming2.FPS2AudioSendOnlyTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FPS2AudioSendOnlyTest::RunTest(const FString& Parameters)
	{
		SetMediaDirection(EMediaType::Audio, EMediaDirection::SendOnly);
		DoMediaDirectionTest(EMediaType::Audio, EMediaDirection::SendOnly);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2AudioRecvOnlyTest, "System.Plugins.PixelStreaming2.FPS2AudioRecvOnlyTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FPS2AudioRecvOnlyTest::RunTest(const FString& Parameters)
	{
		SetMediaDirection(EMediaType::Audio, EMediaDirection::RecvOnly);
		DoMediaDirectionTest(EMediaType::Audio, EMediaDirection::RecvOnly);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2AudioDisabledTest, "System.Plugins.PixelStreaming2.FPS2AudioDisabledTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FPS2AudioDisabledTest::RunTest(const FString& Parameters)
	{
		SetMediaDirection(EMediaType::Audio, EMediaDirection::Disabled);
		DoMediaDirectionTest(EMediaType::Audio, EMediaDirection::Disabled);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2AudioBidirectionalTest, "System.Plugins.PixelStreaming2.FPS2AudioBidirectionalTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FPS2AudioBidirectionalTest::RunTest(const FString& Parameters)
	{
		SetMediaDirection(EMediaType::Audio, EMediaDirection::Bidirectional);
		DoMediaDirectionTest(EMediaType::Audio, EMediaDirection::Bidirectional);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2VideoSendOnlyTest, "System.Plugins.PixelStreaming2.FPS2VideoSendOnlyTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FPS2VideoSendOnlyTest::RunTest(const FString& Parameters)
	{
		SetMediaDirection(EMediaType::Video, EMediaDirection::SendOnly);
		DoMediaDirectionTest(EMediaType::Video, EMediaDirection::SendOnly);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2VideoRecvOnlyTest, "System.Plugins.PixelStreaming2.FPS2VideoRecvOnlyTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FPS2VideoRecvOnlyTest::RunTest(const FString& Parameters)
	{
		SetMediaDirection(EMediaType::Video, EMediaDirection::RecvOnly);
		DoMediaDirectionTest(EMediaType::Video, EMediaDirection::RecvOnly);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2VideoDisabledTest, "System.Plugins.PixelStreaming2.FPS2VideoDisabledTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FPS2VideoDisabledTest::RunTest(const FString& Parameters)
	{
		SetMediaDirection(EMediaType::Video, EMediaDirection::Disabled);
		DoMediaDirectionTest(EMediaType::Video, EMediaDirection::Disabled);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2VideoBidirectionalTest, "System.Plugins.PixelStreaming2.FPS2VideoBidirectionalTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FPS2VideoBidirectionalTest::RunTest(const FString& Parameters)
	{
		SetMediaDirection(EMediaType::Video, EMediaDirection::Bidirectional);
		DoMediaDirectionTest(EMediaType::Video, EMediaDirection::Bidirectional);

		return true;
	}
} // namespace UE::PixelStreaming2

#endif // WITH_DEV_AUTOMATION_TESTS