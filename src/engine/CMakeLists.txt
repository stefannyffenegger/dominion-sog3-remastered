# Import library for the original Rtlib32.dll, built from our own .def file.
set(RTLIB_IMPLIB ${CMAKE_CURRENT_BINARY_DIR}/librtlib32.a)
add_custom_command(
    OUTPUT ${RTLIB_IMPLIB}
    COMMAND ${CMAKE_DLLTOOL} -k -d ${CMAKE_CURRENT_SOURCE_DIR}/rtlib32.def -l ${RTLIB_IMPLIB}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rtlib32.def)
add_custom_target(rtlib32_implib DEPENDS ${RTLIB_IMPLIB})

add_library(rtlib32 INTERFACE)
add_dependencies(rtlib32 rtlib32_implib)
target_include_directories(rtlib32 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rtlib32 INTERFACE ${RTLIB_IMPLIB})
