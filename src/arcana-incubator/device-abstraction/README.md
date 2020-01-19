# device abstraction

## SDL2 setup

Requires SDL2, setup as follows:

### Linux

Install `libsdl2-devel` (debian), or similar.

### Windows

1. [Download](https://www.libsdl.org/download-2.0.php) development libraries and runtime binaries.

2. Place the DLL file next to the executable. Place the development library folder somewhere.

3. Add a file called `sdl2-config.cmake` to the development library root, with these contents:

    ```cmake
    set(SDL2_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

    # Support both 32 and 64 bit builds
    if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
        set(SDL2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/lib/x64/SDL2.lib;${CMAKE_CURRENT_LIST_DIR}/lib/x64/SDL2main.lib")
    else()
        set(SDL2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/lib/x86/SDL2.lib;${CMAKE_CURRENT_LIST_DIR}/lib/x86/SDL2main.lib")
    endif()

    string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)
    ```

4. Move all files in the `include` folder to a new subfolder, `include/SDL2`.

5. Add an environment variable to the build configuration called `SDL2_DIR`, pointing to the root, i.e. `C:\Libraries\SDL2-2.0.10`.
