cmake_minimum_required(VERSION 3.1) # target_compile_features() introduced in cmake-3.1

project(speedcore CXX C)

# if no build type => default is Release (or request if interactive)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type" FORCE)
   set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Debug")  # "cmake-gui" or "cmake -i"
endif()

#set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED) # provides ${CMAKE_THREAD_LIBS_INIT}

add_executable(          speedcore speedcore.cpp)
target_compile_features( speedcore PRIVATE cxx_auto_type cxx_lambdas cxx_strong_enums ) #http://www.cmake.org/cmake/help/v3.3/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html
target_compile_options(  speedcore PRIVATE -Wall -Wextra)
target_compile_options(  speedcore PRIVATE "$<$<CONFIG:RELEASE>:-march=native>")
target_link_libraries(   speedcore ${CMAKE_THREAD_LIBS_INIT})
