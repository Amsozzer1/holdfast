# WebAssembly build of the tracker core + simulator for the browser playground.
#   emcmake cmake -S wasm -B build/wasm && cmake --build build/wasm
# (scripts/build_wasm.sh runs this inside the pinned Emscripten container.)
cmake_minimum_required(VERSION 3.20)
project(holdfast_wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
# Header-only Eigen, pinned by hash. SOURCE_SUBDIR points nowhere so Eigen's own CMake
# (tests, docs) is not configured.
FetchContent_Declare(eigen
  URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
  URL_HASH SHA256=8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72
  SOURCE_SUBDIR none)
FetchContent_MakeAvailable(eigen)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_executable(holdfast_wasm
  bindings.cpp
  ${ROOT}/src/bbox.cpp
  ${ROOT}/src/kalman.cpp
  ${ROOT}/src/hungarian.cpp
  ${ROOT}/src/tracker.cpp
  ${ROOT}/src/sim.cpp
  ${ROOT}/src/playground.cpp)
target_include_directories(holdfast_wasm PRIVATE ${ROOT}/include ${eigen_SOURCE_DIR})
target_compile_options(holdfast_wasm PRIVATE -O3 -fexceptions -DEIGEN_NO_DEBUG)
target_link_options(holdfast_wasm PRIVATE
  -O3 -fexceptions -lembind --no-entry
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createHoldfast
  -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 -sSINGLE_FILE=1
  -sDYNAMIC_EXECUTION=0)  # no eval/new Function: works under a strict CSP
set_target_properties(holdfast_wasm PROPERTIES OUTPUT_NAME holdfast SUFFIX ".js")
