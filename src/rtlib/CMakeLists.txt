# Our Rtlib32.dll. Stage 1: forwards every export to the original engine (renamed
# Rtlib32_orig.dll) and patches in the functions we have rewritten (ported.h).
set(ORIG_IMPLIB ${CMAKE_CURRENT_BINARY_DIR}/librtlib32_orig.a)
add_custom_command(
    OUTPUT ${ORIG_IMPLIB}
    COMMAND ${CMAKE_DLLTOOL} -k -d ${CMAKE_CURRENT_SOURCE_DIR}/rtlib32_orig.def -l ${ORIG_IMPLIB}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rtlib32_orig.def)
add_custom_target(rtlib32_orig_implib DEPENDS ${ORIG_IMPLIB})

add_library(rtlib SHARED hooks.c res.c utils.c compress.c codec.c audio.c rtlib32.def ${PROJECT_SOURCE_DIR}/src/common/minicrt.c)
add_dependencies(rtlib rtlib32_orig_implib)
set_target_properties(rtlib PROPERTIES PREFIX "" OUTPUT_NAME "Rtlib32" SUFFIX ".dll")
target_compile_options(rtlib PRIVATE ${DOM_MODULE_FLAGS})
# away from the original's preferred address (0x10000000)
target_link_options(rtlib PRIVATE ${DOM_MODULE_LINK} -Wl,--image-base,0x60000000)
target_link_libraries(rtlib PRIVATE ${ORIG_IMPLIB} kernel32 user32 winmm gcc)
