# live_link_vcam

A rebuild of the Live Link VCAM app for Flutter, enabling cross-platform deployment.

## Tentacle

This app depends on the `FlutterTentacle` package (included in the Unreal Engine repo under `Engine/Extras/FlutterTentacle`), which in turn depends on the Tentacle SDK. The Tentacle SDK is not distributed to engine licensees. See the `FlutterTentacle` README for more info.

### IOS Note

Before building `live_link_vcam` for iOS, you will first need to run the `IOSSetup.sh` script as described in the `FlutterTentacle` README.

## Pigeon

This project uses Pigeon to automatically generate platform channel bindings for native APIs.
See `pigeons/README` for more information.

## Signing Certificates

To setup signing certificates, follow the instructions outlined in this article

- [Deploying Flutter Apps to The PlayStore](https://medium.com/@bernes.dev/deploying-flutter-apps-to-the-playstore-1bd0cce0d15c)

Add your `android-key.tks` to `android\app` folder and `key.properities` to `android\`.  The gradle files are already setup
to read these files when app bundling