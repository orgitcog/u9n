## Prerequisites:
* `yasm` built from source: https://github.com/yasm/yasm
    * the binary for the source build in your path
    * CANNOT USE THE BREW VERSION

The new xcode (15+) linker (aka ld_prime) now is checking for LC_BUILD_VERSION in all binaries.

If it is not present it throws the following warning:
`ld: warning: no platform load command found in '/Users/zack.neyland/Perforce/zn_fn_mac/Engine/Source/ThirdParty/libjpeg-turbo/3.0.0/lib/Mac/Release/libturbojpeg.a[x86_64][67](jsimdcpu.asm.o)', assuming: macOS`

Unfortunately brew's version of `yasm` is 1.3.0 which does not include this pull request: https://github.com/yasm/yasm/pull/180. That pull request adds a build directive to insert the LC_BUILD_VERSION.

## Building
1. Download the tarball of the libjpeg-turbo source.
2. Extract it to the version folder, e.g. `Engine/Source/ThirdParty/libjpeg-turbo/3.0.0`
3. Add the [builddirective ...] with the python script `python add_build_version.py`
4. Run the `./BuildForMac.command`