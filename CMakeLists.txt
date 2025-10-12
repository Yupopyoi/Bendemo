cmake_minimum_required(VERSION 3.16)

project(Bendemo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
  message(FATAL_ERROR "MSVC/Qt(msvc2022_64) 用プロジェクトです。\
  Visual Studio 17 2022 + -A x64 のキットで再構成してください。")
endif()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Multimedia SerialPort Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core Multimedia SerialPort Widgets)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)

# find dependencies
find_package(OpenCV CONFIG REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Bendemo
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        SerialInterface.h
        SerialInterface.cpp
        integratedvaluecontroller.h integratedvaluecontroller.cpp
        cameradisplayer.h cameradisplayer.cpp
        darknessdetector.h darknessdetector.cpp
        bbox_renderer.h bbox_renderer.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Bendemo APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(Bendemo SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Bendemo
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(Bendemo PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(Bendemo PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)
target_link_libraries(Bendemo PRIVATE Qt${QT_VERSION_MAJOR}::SerialPort)
target_link_libraries(Bendemo PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(Bendemo PRIVATE Qt${QT_VERSION_MAJOR}::OpenGLWidgets)

# If OpenCV exports targets (modern), link by imported targets.
# Otherwise fall back to legacy variables.
if (TARGET OpenCV::opencv_core)
    message(STATUS "OpenCV found (CONFIG). Version: ${OpenCV_VERSION}")
    target_link_libraries(Bendemo PRIVATE OpenCV::opencv_world)
else()
    message(STATUS "OpenCV found (MODULE). Version: ${OpenCV_VERSION}")
    include_directories(${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Bendemo PRIVATE ${OpenCV_LIBS})
endif()

qt_standard_project_setup()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Bendemo)
endif()
set_target_properties(Bendemo PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Bendemo
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Bendemo)
endif()
