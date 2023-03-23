#
# Build libraries
#
add_library(server OBJECT
            server.cpp
            server.h)

target_include_directories(server PUBLIC .)
target_compile_features(server PUBLIC cxx_std_11) # don't bump

#
# Executables
#

add_executable(server-demo demo.cpp)
target_link_libraries(server-demo PRIVATE llama ggml utils server)

# if windows, link ws2_32
if (WIN32)
    target_link_libraries(server-demo PRIVATE ws2_32)
endif()