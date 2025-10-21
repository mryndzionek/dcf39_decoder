cmake_minimum_required(VERSION 3.16.3)

project(dcf39_decoder)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(LIBS m dl pthread)
include_directories(include)

add_compile_options(-Wno-deprecated-declarations
                    -Wall -Werror -fPIC)

add_executable(dcf39_decoder src/main.c src/dcf39_decoder.c)
target_link_libraries(dcf39_decoder ${LIBS})
