cmake_minimum_required(VERSION 3.25)
project(holdfast VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HOLDFAST_BUILD_APPS  "Build the OpenCV/ONNX Runtime app layer and binaries" ON)
option(HOLDFAST_BUILD_TESTS "Build unit tests" ON)
option(HOLDFAST_SANITIZE    "Build with ASan + UBSan" OFF)

if(HOLDFAST_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  add_link_options(-fsanitize=address,undefined)
endif()

find_package(Eigen3 REQUIRED NO_MODULE)

# ---- core: Eigen + std only. Everything the tracker needs to think. ----
add_library(holdfast_core STATIC
  src/bbox.cpp
  src/kalman.cpp
  src/hungarian.cpp
  src/tracker.cpp
  src/degrade.cpp
  src/gt_detector.cpp
  src/mot_io.cpp
  src/stage_timer.cpp
  src/sim.cpp
  src/playground.cpp)
target_include_directories(holdfast_core PUBLIC include)
target_link_libraries(holdfast_core PUBLIC Eigen3::Eigen)
target_compile_options(holdfast_core PRIVATE -Wall -Wextra -Wpedantic)

# ---- app layer: image I/O, camera-motion compensation, ONNX detector, render ----
if(HOLDFAST_BUILD_APPS)
  if(NOT OpenCV_DIR AND EXISTS ${CMAKE_SOURCE_DIR}/.deps/opencv/lib/cmake/opencv4)
    set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/.deps/opencv/lib/cmake/opencv4)
  endif()
  find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs video calib3d)

  set(HOLDFAST_ORT_DIR ${CMAKE_SOURCE_DIR}/.deps/onnxruntime CACHE PATH "ONNX Runtime root")
  find_path(ORT_INCLUDE_DIR onnxruntime_cxx_api.h PATHS ${HOLDFAST_ORT_DIR}/include NO_DEFAULT_PATH REQUIRED)
  find_library(ORT_LIBRARY onnxruntime PATHS ${HOLDFAST_ORT_DIR}/lib NO_DEFAULT_PATH REQUIRED)

  add_library(holdfast_app STATIC
    src/app/image_source.cpp
    src/app/camera_motion.cpp
    src/app/onnx_detector.cpp
    src/app/renderer.cpp
    src/app/pipeline.cpp)
  target_include_directories(holdfast_app PUBLIC include ${OpenCV_INCLUDE_DIRS} ${ORT_INCLUDE_DIR})
  target_link_libraries(holdfast_app PUBLIC holdfast_core ${OpenCV_LIBS} ${ORT_LIBRARY})
  target_compile_options(holdfast_app PRIVATE -Wall -Wextra)
  set_target_properties(holdfast_app PROPERTIES INTERFACE_LINK_OPTIONS "")

  foreach(app holdfast_run holdfast_bench)
    add_executable(${app} apps/${app}.cpp)
    target_link_libraries(${app} PRIVATE holdfast_app)
    set_target_properties(${app} PROPERTIES BUILD_RPATH "${HOLDFAST_ORT_DIR}/lib" INSTALL_RPATH "$ORIGIN/../lib;@loader_path/../lib")
  endforeach()
  install(TARGETS holdfast_run holdfast_bench RUNTIME DESTINATION bin)

  if(HOLDFAST_BUILD_TESTS)
    find_package(GTest REQUIRED)
    add_executable(holdfast_app_tests tests/test_camera_motion.cpp)
    target_link_libraries(holdfast_app_tests PRIVATE holdfast_app GTest::gtest_main)
    set_target_properties(holdfast_app_tests PROPERTIES BUILD_RPATH "${HOLDFAST_ORT_DIR}/lib")
  endif()
endif()

if(HOLDFAST_BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  add_executable(holdfast_tests
    tests/test_hungarian.cpp
    tests/test_kalman.cpp
    tests/test_tracker.cpp
    tests/test_degrade.cpp
    tests/test_gt_detector.cpp
    tests/test_playground.cpp)
  target_link_libraries(holdfast_tests PRIVATE holdfast_core GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(holdfast_tests)
  if(TARGET holdfast_app_tests)
    gtest_discover_tests(holdfast_app_tests)
  endif()
endif()
