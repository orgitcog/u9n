rem @echo off
mkdir build
..\..\..\..\..\..\..\..\Source\ThirdParty\Protobuf\3.18.0\bin\Win64\x64\Release\protoc.exe metahuman_service_api.proto --cpp_out=build
powershell -Command "(gc build\metahuman_service_api.pb.cc) -replace 'metahuman_service_api.pb.h', 'metahuman_service_api.pb.h.inc' | Out-File -encoding ASCII build\metahuman_service_api.pb.cc.inc
copy build\metahuman_service_api.pb.cc.inc metahuman_service_api.pb.cc.inc
move /Y build\metahuman_service_api.pb.h metahuman_service_api.pb.h.inc
rmdir /S /Q build
