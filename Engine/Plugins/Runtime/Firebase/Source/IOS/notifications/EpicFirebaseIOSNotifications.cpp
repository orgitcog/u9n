// Copyright Epic Games, Inc. All Rights Reserved.

#include "EpicFirebaseIOSNotifications.h"

#if PLATFORM_IOS && WITH_IOS_FIREBASE_INTEGRATION

THIRD_PARTY_INCLUDES_START
_Pragma("clang diagnostic push")
_Pragma("clang diagnostic ignored \"-Wobjc-property-no-attribute\"")
#include "ThirdParty/IOS/include/Firebase.h"
_Pragma("clang diagnostic pop")
THIRD_PARTY_INCLUDES_END
// Module header, needed for log category
#include "Firebase.h"
#include "IOS/IOSAppDelegate.h"

#include "Misc/Paths.h"
#include "Async/TaskGraphInterfaces.h"

bool FFirebaseIOSNotifications::bIsInitialized = false;
FString FFirebaseIOSNotifications::IOSFirebaseToken;
NSString* KEY_FIREBASE_TOKEN = @"firebasetoken";
NSString* KEY_FIREBASE_PROJECT_ID = @"firebaseprojectid";

@interface IOSAppDelegate (FirebaseHandling) <FIRMessagingDelegate>

-(void)SetupFirebase : (Boolean) enableAnalytics;
-(void)UpdateFirebaseToken : (UInt64) Timeout;

@end

@implementation IOSAppDelegate (FirebaseHandling)

-(void)SetupFirebase : (Boolean) enableAnalytics
{
	if (enableAnalytics)
	{
		[FIRAnalytics setAnalyticsCollectionEnabled:YES];
	}
    [FIRMessaging messaging].delegate = self;
    [UNUserNotificationCenter currentNotificationCenter].delegate = self;
    
    UNAuthorizationOptions authOptions = UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge;
    [[UNUserNotificationCenter currentNotificationCenter] requestAuthorizationWithOptions:authOptions
        completionHandler:^(BOOL granted, NSError * _Nullable error) 
     {
        if (granted)
        {
            UE_LOG(LogFirebase, Log, TEXT("Firebase authorization granted"));
        }
        else
        {
            UE_LOG(LogFirebase, Log, TEXT("Firebase authorization denied"));
        }
    }];

   [[UIApplication sharedApplication] registerForRemoteNotifications];
}

-(void)ConfigureFirebase
{
    [FIRApp configure];
}

-(void)ConfigureFirebaseWithCustomFile : (NSString*)fileName fileExtension:(NSString*)ext
{
	NSString* filePath = [[NSBundle mainBundle] pathForResource:fileName ofType:ext];
	if (filePath == nil)
	{
		UE_LOG(LogFirebase, Warning, TEXT("Failed to find custom Firebase file, using default configuration"));
		[self ConfigureFirebase];
	}
	else
	{
		FIROptions* options = [[FIROptions alloc] initWithContentsOfFile:filePath];
		if (options == nil)
		{
			UE_LOG(LogFirebase, Warning, TEXT("Failed to parse custom Firebase options, using default configuration"));
			[self ConfigureFirebase];
		}
		else
		{
			[FIRApp configureWithOptions:options];
		}
	}
}

-(void)messaging:(FIRMessaging *)messaging didReceiveRegistrationToken:(NSString *)fcmToken 
{
	if (fcmToken == nil)
	{
		// happens when deleting the token
		return;
	}
	
    NSDictionary *dataDict = [NSDictionary dictionaryWithObject:fcmToken forKey:@"token"];
    [[NSNotificationCenter defaultCenter] postNotificationName:
     @"FCMToken" object:nil userInfo:dataDict];
    
    FString Token = FString(fcmToken);
    FFirebaseIOSNotifications::SetFirebaseToken(Token);
#if !UE_BUILD_SHIPPING
    UE_LOG(LogFirebase, Log, TEXT("Firebase Token Refreshed : %s"), *Token);
#endif
	NSString* projectID = [[[FIRApp defaultApp] options] GCMSenderID];
    [[NSUserDefaults standardUserDefaults] setObject:fcmToken forKey:KEY_FIREBASE_TOKEN];
	[[NSUserDefaults standardUserDefaults] setObject:projectID forKey:KEY_FIREBASE_PROJECT_ID];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (void)UpdateFirebaseToken : (UInt64) Timeout
{
    NSUserDefaults* UserDefaults = [NSUserDefaults standardUserDefaults];
	NSString* currentProjectID = [[[FIRApp defaultApp] options] GCMSenderID];
	if ([UserDefaults objectForKey:KEY_FIREBASE_PROJECT_ID] == nil ||
		![[UserDefaults stringForKey:KEY_FIREBASE_PROJECT_ID] isEqualToString:currentProjectID])
	{
		if ([UserDefaults objectForKey:KEY_FIREBASE_TOKEN] != nil)
		{
			UE_LOG(LogFirebase, Log, TEXT("Firebase project changed, removing token"));
			[UserDefaults removeObjectForKey:KEY_FIREBASE_TOKEN];
		}
	}
	
    if ([UserDefaults objectForKey:KEY_FIREBASE_TOKEN] != nil)
    {
        FString Token = FString([UserDefaults stringForKey:KEY_FIREBASE_TOKEN]);
        FFirebaseIOSNotifications::SetFirebaseToken(Token);
#if !UE_BUILD_SHIPPING
        UE_LOG(LogFirebase, Log, TEXT("Retrieved Firebase Token from cache : %s"), *Token);
#endif
    }
    
	// Query token from Firebase even if there is one in cache already
	
    dispatch_semaphore_t updateTokenSemaphore = dispatch_semaphore_create(0);
    // wrapped in dispatch_async to avoid locking up if we're on the main thread
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void)
    {
        [[FIRMessaging messaging] tokenWithCompletion:^(NSString *firebaseToken, NSError *error)
         {
            if (error == nil && firebaseToken != nil)
            {
                FString Token = FString(firebaseToken);
                FFirebaseIOSNotifications::SetFirebaseToken(Token);
#if !UE_BUILD_SHIPPING
                UE_LOG(LogFirebase, Log, TEXT("Firebase Token Queried : %s"), *Token);
#endif
				NSString* projectID = [[[FIRApp defaultApp] options] GCMSenderID];
                [[NSUserDefaults standardUserDefaults] setObject:firebaseToken forKey:KEY_FIREBASE_TOKEN];
                [[NSUserDefaults standardUserDefaults] setObject:projectID forKey:KEY_FIREBASE_PROJECT_ID];
                [[NSUserDefaults standardUserDefaults] synchronize];
                
                dispatch_semaphore_signal(updateTokenSemaphore);
            }
        }];
    });
    dispatch_semaphore_wait(updateTokenSemaphore, dispatch_time(DISPATCH_TIME_NOW, Timeout));
    dispatch_release(updateTokenSemaphore);
}
@end

void FFirebaseIOSNotifications::ConfigureFirebase()
{
    if (!IsConfigured())
    {
        [[IOSAppDelegate GetDelegate] ConfigureFirebase];
    }
}

void FFirebaseIOSNotifications::ConfigureFirebaseWithCustomFile(const FString& FileName)
{
	if (IsConfigured())
	{
		UE_LOG(LogFirebase, Warning, TEXT("Failed to configure Firebase with custom file. Firebase is already configured!"));
		return;
	}
	
	FString Name = FPaths::GetBaseFilename(FileName);
	FString Extension = FPaths::GetExtension(FileName);
	CFStringRef NameStr = FPlatformString::TCHARToCFString(*Name);
	CFStringRef ExtensionStr = FPlatformString::TCHARToCFString(*Extension);
	[[IOSAppDelegate GetDelegate] ConfigureFirebaseWithCustomFile:(NSString*)NameStr fileExtension:(NSString*)ExtensionStr];
}

void FFirebaseIOSNotifications::Initialize(uint64 TokenQueryTimeoutNanoseconds, bool bEnableAnalytics)
{
    if (!IsConfigured())
    {
        ConfigureFirebase();
    }
    
    if (!bIsInitialized)
    {
        [[IOSAppDelegate GetDelegate] SetupFirebase:bEnableAnalytics];
        [[IOSAppDelegate GetDelegate] UpdateFirebaseToken:TokenQueryTimeoutNanoseconds];
        bIsInitialized = true;
    }
}

void FFirebaseIOSNotifications::SetFirebaseToken(FString Token)
{
    @synchronized ([IOSAppDelegate GetDelegate])
    {
		if (IFirebaseModuleInterface::Get().OnTokenUpdate.IsBound())
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady([OldToken = IOSFirebaseToken, NewToken = Token]()
				{
					IFirebaseModuleInterface::Get().OnTokenUpdate.Broadcast(OldToken, NewToken);
				}, TStatId(), NULL, ENamedThreads::GameThread);
		}
        IOSFirebaseToken = Token;
    }
}

FString FFirebaseIOSNotifications::GetFirebaseToken()
{
    FString Token;
    @synchronized ([IOSAppDelegate GetDelegate])
    {
        Token = IOSFirebaseToken;
    }
    
    if (Token.IsEmpty())
    {
        UE_LOG(LogFirebase, Log, TEXT("Firebase Token is empty"));
    }
    
    return Token;
}

void FFirebaseIOSNotifications::EnableFirebaseAutoInit()
{
	[FIRMessaging messaging].autoInitEnabled = YES;
}

bool FFirebaseIOSNotifications::IsConfigured()
{
	return [[FIRApp allApps] count] != 0;
}

void FFirebaseIOSNotifications::DeleteFirebaseToken()
{
	if (!IsConfigured())
	{
		UE_LOG(LogFirebase, Warning, TEXT("Trying to delete Firebase token, but Firebase is not configured."))
		return;
	}
	
	[[NSUserDefaults standardUserDefaults] removeObjectForKey:KEY_FIREBASE_TOKEN];
	[[NSUserDefaults standardUserDefaults] synchronize];
	
	[[FIRMessaging messaging] deleteTokenWithCompletion:^(NSError *error)
	 {
#if !UE_BUILD_SHIPPING
		NSLog(@"Error when deleting Firebase token: %@", error);
#endif
	}];
}

#endif // PLATFORM_IOS
