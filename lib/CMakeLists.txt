find_package(Boost REQUIRED)

# object library providing the core functionality
add_library(${CORE_LIB_NAME} OBJECT
  DoricoRemote.cpp
  DoricoRemote.hpp
  Request.hpp
  Response.hpp
  utility.hpp
  utility.cpp
)

# DLL/shared library exporting the C wrapper functions
add_library(${SHARED_LIB_NAME} SHARED
  doremote.h
  doremote.cpp
)

# static library
add_library(${STATIC_LIB_NAME} STATIC
  doremote.h
  doremote.cpp
)

add_custom_target(
  COMMAND ${CMAKE_COMMAND}
    -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/${SHARED_LIB_NAME} ${CMAKE_BINARY_DIR}/${SHARED_LIB_NAME}
)

include(GenerateExportHeader)

# Generate header file containing macros for the DLL import/export modifiers.
# It's included in doremote.h.
generate_export_header(${PROJECT_NAME}
  EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}-export.h
)

target_link_libraries(${CORE_LIB_NAME} PRIVATE Boost::headers)
target_link_libraries(${SHARED_LIB_NAME} PRIVATE ${CORE_LIB_NAME})
target_link_libraries(${STATIC_LIB_NAME} PRIVATE ${CORE_LIB_NAME})

set_target_properties(${CORE_LIB_NAME}   PROPERTIES CXX_STANDARD 20)
set_target_properties(${SHARED_LIB_NAME} PROPERTIES CXX_STANDARD 20)
set_target_properties(${STATIC_LIB_NAME} PROPERTIES CXX_STANDARD 20)

target_compile_definitions(${CORE_LIB_NAME} PRIVATE
  ${COMMON_TARGET_DEFS}
)

target_compile_definitions(${SHARED_LIB_NAME} PRIVATE
  ${COMMON_TARGET_DEFS}
  ${PROJECT_NAME}_EXPORTS # add DLL export modifiers to the function declarations
)

target_compile_definitions(${STATIC_LIB_NAME} PRIVATE
  ${COMMON_TARGET_DEFS}
  DOREMOTE_STATIC_DEFINE  # don't add any DLL import/export modifiers
)

include_directories(${CORE_LIB_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/third-party
)

include_directories(${SHARED_LIB_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include)
include_directories(${STATIC_LIB_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include)

if(MSVC)
  target_compile_options(${CORE_LIB_NAME} PRIVATE /W3 /sdl /bigobj /utf-8)
  target_compile_options(${SHARED_LIB_NAME} PRIVATE /W3 /sdl /utf-8)
  target_compile_options(${STATIC_LIB_NAME} PRIVATE /W3 /sdl /utf-8)

  set_property(TARGET ${CORE_LIB_NAME} PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )

  set_property(TARGET ${SHARED_LIB_NAME} PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
endif()

install(TARGETS ${SHARED_LIB_NAME} ${STATIC_LIB_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/ ${CMAKE_BINARY_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)