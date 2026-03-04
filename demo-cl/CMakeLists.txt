find_package(replxx)
find_package(Threads)

set(CMAKE_CXX_STANDARD 17)

add_executable(${CL_DEMO_NAME}
  main.cpp
  util.c
  util.h
  DoremoteWrapper.cpp
  DoremoteWrapper.hpp
)

target_include_directories(${CL_DEMO_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib ${CMAKE_BINARY_DIR}/include)

target_link_libraries(${CL_DEMO_NAME} PRIVATE
  ${SHARED_LIB_NAME}
  replxx::replxx
)

install(TARGETS ${CL_DEMO_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

