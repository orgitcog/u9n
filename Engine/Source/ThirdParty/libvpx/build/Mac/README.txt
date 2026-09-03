Prerequisites:
* yasm built from source: https://github.com/yasm/yasm
    * the binary for the source build in your path
    * CANNOT USE THE BREW VERSION

The new xcode (15+) linker (aka ld_prime) now is checking for LC_BUILD_VERSION in all binaries.
If it is not present it throws the following warning:
warning: no platform load command found in '.../Engine/Source/ThirdParty/libvpx/libvpx-1.14.1/lib/Mac/libvpx_fPIC.a[x86_64][226](emms_mmx.asm.o)', assuming: macOS

Unfortunately brew's version of `yasm` is 1.3.0 which does not include this pull request: https://github.com/yasm/yasm/pull/180. That pull request adds a build directive to insert the LC_BUILD_VERSION.

Additionally, since the libvpx source doesn't include this directive,
the build script now need to run a python script which adds the [builversion...] directive to all of the asm files
it can find in the temp build dir for libvpx.

Other than that the build script should work normally.

However, using non-XCode utilities can produce wrong binaries, and you will get linker errors.
Please check what your computer will use for build - ar, nm, strip, etc. 

You can do it by command "which ar", "which strip", etc. 

In a normal state, it shouldn't do call strip function so that you will see "[CP] libvpx.a < libvpx_g.a".
If you see "[STRIP] libvpx.a < libvpx_g.a", then you have installed GNU strip utilities what will create a wrong library.
Check if it is a state.
You can turn it off by using "make -j USE_GNU_STRIP=false" command for make library, but I strongly suggest 
checking and setting the environment to use XCode command tools only (cc, gcc, ar, lipo).