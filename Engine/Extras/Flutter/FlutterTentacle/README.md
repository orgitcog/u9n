# flutter_tentacle

This package adds Flutter support for Bluetooth scanning and timecode synchronization with Tentacle devices. It is designed for use specifically in Epic's Flutter apps, so it depends on the `EpicCommon` package and integrates with its features and related tooling.

## Setup

### Tentacle SDK

This project depends on the Tentacle SDK located in `Engine/Restricted/NotForLicensees/Source/ThirdParty/TentacleSDK`. Note that this SDK is not
distributed to Unreal Engine licensees. If you have access to it, place it in that location to build this package.

### Android Configuration

In the main `AndroidManifest.xml' for your project, add the following elements to the `<manifest>` element:

	<!-- Features and permissions for Tentacle scanning -->
    <uses-feature
        android:name="android.hardware.bluetooth_le"
        android:required="false" />

	<!-- API <= 30 -->
	<uses-permission
		android:name="android.permission.BLUETOOTH"
		android:maxSdkVersion="30" />
	<uses-permission
		android:name="android.permission.BLUETOOTH_ADMIN"
		android:maxSdkVersion="30" />
	<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />
	<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />

	<!-- API >= 31 -->
	<uses-permission
		android:name="android.permission.BLUETOOTH_SCAN"
		android:usesPermissionFlags="neverForLocation"
		tools:targetApi="s" />
	<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
	<!-- End Tentacle permissions -->

If your build fails when trying to parse this file, make sure this attribute is present on your `<manifest>` element:

	xmlns:tools="http://schemas.android.com/tools"

### iOS Configuration

First, run the `IOSSetup.sh` script in this directory. This will create a symbolic link to the Tentacle framework in the IOS directory, which is required to build this library.

Open the `Podfile` file in your Flutter app's `ios` directory and locate the code block starting with: `post_install do |installer|`. Above that line, add the following function.

	# Configure the Pods project with build information necessary for flutter_tentacle
	# Configure the Pods project with build information necessary for flutter_tentacle
	def configure_flutter_tentacle(installer)
	  tentacle_target = installer.pods_project.targets.find { |target| target.name == 'flutter_tentacle' }
	  
	  if tentacle_target
		# Allow Tentacle library to use Bluetooth permissions for device communication
		tentacle_target.build_configurations.each do |config|
		  config.build_settings['GCC_PREPROCESSOR_DEFINITIONS'] ||= [
			'$(inherited)',
			'PERMISSION_BLUETOOTH=1',
		  ]
		end
		
		dev_pods_group = installer.pods_project.groups.find { |group| group.display_name == 'Development Pods' }
		framework_ref = dev_pods_group['flutter_tentacle']['Frameworks']['Tentacle.xcframework']
	
		# Embed the Tentacle framework
		embed_frameworks_build_phase = installer.pods_project.new(Xcodeproj::Project::Object::PBXCopyFilesBuildPhase)
		embed_frameworks_build_phase.name = 'Embed Frameworks'
		embed_frameworks_build_phase.symbol_dst_subfolder_spec = :frameworks
		tentacle_target.build_phases << embed_frameworks_build_phase
	
		# Add the framework as a Sign & Embed build file
		build_file = embed_frameworks_build_phase.add_file_reference(framework_ref)
		build_file.settings = { 'ATTRIBUTES' => ['CodeSignOnCopy', 'RemoveHeadersOnCopy'] }
		frameworks_build_phase = tentacle_target.build_phases.find { |build_phase| build_phase.to_s == 'FrameworksBuildPhase' }
		frameworks_build_phase.add_file_reference(framework_ref)
	  end
	end

Then, inside the `post_install` block, add the following line:

	configure_flutter_tentacle(installer)

See `example/IOS/Podfile` for an example.

### Flutter Configuration

1. Add this package to your package's `pubspec.yaml`.
2. In your main build function...
	1. Add a top-level `Provider` for `TentacleDeviceManager`, e.g.:
    ```
    Provider(
      create: (_) => TentacleDeviceManager(),
      dispose: (_, provider) => provider.dispose(),
    )
    ```
	2. Add `TentacleLocalizations.localizationsDelegates` to your list of localization delegates.
3. When your app exits (e.g. in the `didRequestAppExit` function), call `TentaclePlugin.shutdown()`.
4. Optional: to add Tentacle timecode support to the `TimecodeManager`, call `manager.registerSource(TentacleTimecodeSource())` before you call `manager.initialize()`.

## Package development

### ffigen

We use some Tentacle SDK functions through Dart's Foreign Function Interface (FFI). Dart bindings for these functions are
automatically generated using `ffigen` and checked into source control.

If you need to regenerate them, first set up ffigen (see https://pub.dev/packages/ffigen), then run the following command
in the root VCAM project directory:

    dart run ffigen --config ffigen_tentacle.yaml

### Pigeon

This project uses Pigeon to automatically generate platform channel bindings for native APIs.
See `pigeons/README` for more information.