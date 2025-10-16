cmake_minimum_required(VERSION 3.20)
project(Bendemo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
  message(FATAL_ERROR "This project is for MSVC/Qt (msvc2022_64). Please reconfigure it using the Visual Studio 17 2022 + -A x64 kit.")
endif()

if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    set(CMAKE_CUDA_ARCHITECTURES native)
  else()
    # Replace according to your GPU (e.g. RTX30=86, RTX40=89)
    set(CMAKE_CUDA_ARCHITECTURES 89)
  endif()
endif()

set(CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}")

if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# --- Torch_DIR  ---
if(NOT Torch_DIR)
  if(DEFINED ENV{Torch_DIR})
    set(Torch_DIR "$ENV{Torch_DIR}")
  else()
    set(_guess "$ENV{USERPROFILE}/libtorch/share/cmake/Torch")
    file(TO_CMAKE_PATH "${_guess}" Torch_DIR)
  endif()
endif()
message(STATUS "Torch_DIR = ${Torch_DIR}")

find_package(Qt6 REQUIRED COMPONENTS Core Multimedia SerialPort Widgets OpenGLWidgets)
find_package(OpenCV CONFIG REQUIRED)
find_package(Torch REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    SerialInterface.h SerialInterface.cpp
    integratedvaluecontroller.h integratedvaluecontroller.cpp
    cameradisplayer.h cameradisplayer.cpp
    darknessdetector.h darknessdetector.cpp
    bbox_renderer.h bbox_renderer.cpp
    autobending.h autobending.cpp
    yoloexecutor.h yoloexecutor.cpp
  )

qt_add_executable(Bendemo
  MANUAL_FINALIZATION
  ${PROJECT_SOURCES}
)

target_link_libraries(Bendemo PRIVATE
  Qt6::Core Qt6::Multimedia Qt6::SerialPort Qt6::Widgets Qt6::OpenGLWidgets
  ${TORCH_LIBRARIES}
  yaml-cpp::yaml-cpp
)

if (TARGET OpenCV::opencv_world)
  message(STATUS "OpenCV (CONFIG) using opencv_world, version: ${OpenCV_VERSION}")
  target_link_libraries(Bendemo PRIVATE OpenCV::opencv_world)
elseif (TARGET opencv_world)
  message(STATUS "OpenCV (CONFIG) using legacy opencv_world target, version: ${OpenCV_VERSION}")
  target_link_libraries(Bendemo PRIVATE opencv_world)
else()
  message(STATUS "OpenCV (CONFIG) using modules, version: ${OpenCV_VERSION}")
  target_link_libraries(Bendemo PRIVATE
    OpenCV::opencv_core
    OpenCV::opencv_imgproc
    OpenCV::opencv_imgcodecs
    OpenCV::opencv_videoio
    OpenCV::opencv_highgui
  )
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

if (MSVC)
  add_definitions(-DNOMINMAX)
endif()

qt_standard_project_setup()

set_target_properties(Bendemo PROPERTIES
  WIN32_EXECUTABLE TRUE
)

install(TARGETS Bendemo
  RUNTIME DESTINATION bin
  BUNDLE  DESTINATION .
  LIBRARY DESTINATION lib
)

qt_finalize_executable(Bendemo)
