cmake_minimum_required(VERSION 3.20)
project(dominion_remastered C)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Phase 1b modules are 32-bit Windows DLLs that replace the game's own. "
                        "Configure with -DCMAKE_TOOLCHAIN_FILE=cmake/mingw32.cmake")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The game folder ships a 1998 msvcrt.dll that Windows loads before the system one,
# so our modules avoid the C runtime entirely and link only against Win32 DLLs.
set(DOM_MODULE_FLAGS -Wall -Wextra -O2 -ffreestanding -fno-builtin -fno-stack-protector
                     -fno-asynchronous-unwind-tables)
set(DOM_MODULE_LINK -nostdlib -static-libgcc -Wl,--enable-stdcall-fixup -Wl,-e,_DllMain@12)

add_subdirectory(src/engine)
add_subdirectory(src/bmpfont)
add_subdirectory(src/smacker)
add_subdirectory(src/waveplay)
