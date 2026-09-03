cl astc_thunk.cpp /std:c++17 /Zi -DASTC_SUPPORTS_RDO -I5.0.1/Source 5.0.1/lib/Win64/Release/astcenc-sse4.1-static.lib /Fe:Thunks/astcenc_thunk_win64_5.0.1.dll /MD /link /dll
cl astc_thunk.cpp /std:c++17 /Zi -I4.2.0/Source 4.2.0/lib/Win64/Release/astcenc-sse4.1-static.lib /Fe:Thunks/astcenc_thunk_win64_4.2.0.dll /MD /link /dll
