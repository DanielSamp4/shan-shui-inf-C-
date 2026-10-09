cmake_minimum_required(VERSION 3.20)
project(shan_shui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(shan_shui
  src/main.cpp
  src/prng.cpp
  src/noise.cpp
  src/stroke.cpp
  src/canvas.cpp
  src/blob.cpp
  src/scenery.cpp
  src/planner.cpp
  src/generator.cpp
  src/scroll.cpp
)
target_compile_features(shan_shui PRIVATE cxx_std_17)
find_package(Threads REQUIRED)
target_link_libraries(shan_shui PRIVATE Threads::Threads)
if(WIN32)
  target_link_libraries(shan_shui PRIVATE user32 gdi32)
endif()

if(MSVC)
  target_compile_options(shan_shui PRIVATE /W4 /permissive-)
else()
  target_compile_options(shan_shui PRIVATE -Wall -Wextra -Wpedantic)
endif()
