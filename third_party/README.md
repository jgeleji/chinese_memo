# third_party

This directory is not kept in git (it is ~80 MB of upstream sources).

The SDL2 build expects one subdirectory:

    third_party/SDL2-2.30.9/

which is the plain SDL2 source release, unzipped:

    https://www.libsdl.org/release/SDL2-2.30.9.tar.gz

    mkdir -p third_party
    cd third_party
    curl -LO https://www.libsdl.org/release/SDL2-2.30.9.tar.gz
    tar xzf SDL2-2.30.9.tar.gz

CMakeLists.txt does `add_subdirectory(third_party/SDL2-2.30.9)`, so the same
sources are used for the PC (SDL) build and for the Android (NDK, arm64-v8a)
build of libSDL2.so. No SDL sources are patched: everything the port needs
(cout capture, virtual text screen, touch buttons) lives in input_sdl2.cpp.

The Java side of SDL (org.libsdl.app) is copied into
android/app/src/main/java/org/libsdl/app/ from the same release, because the
Gradle android plugin needs those sources inside the module.
