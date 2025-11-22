cmake_minimum_required(VERSION 3.16)
project(images2pdf_qt LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (MSVC)
    # Use static runtime on MSVC builds to avoid redistributing the CRT.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
)
option(IMAGES2PDF_QT_ENABLE_DEPLOY "Bundle QML dependencies via qt6_deploy during install" ON)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
qt_standard_project_setup()

file(GLOB_RECURSE CPP_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE HEADERS     CONFIGURE_DEPENDS "include/*.h")
file(GLOB_RECURSE QML_SOURCES CONFIGURE_DEPENDS 
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} 
    "src/*.qml"
)

qt_add_executable(images2pdf-qt
    MACOSX_BUNDLE
    WIN32
    ${CPP_SOURCES}
    ${HEADERS}
)
qt_add_qml_module(images2pdf-qt
    URI "images2pdf_qt"
    VERSION 1.0
    QML_FILES ${QML_SOURCES} 
)
target_include_directories(images2pdf-qt PRIVATE include)
target_link_libraries(images2pdf-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(images2pdf-qt PRIVATE -static-libstdc++ -static-libgcc)
endif()
qt_import_qml_plugins(images2pdf-qt)
qt_finalize_executable(images2pdf-qt)

install(TARGETS images2pdf-qt DESTINATION bin)
if(IMAGES2PDF_QT_ENABLE_DEPLOY)
    qt_generate_deploy_qml_app_script(
        TARGET images2pdf-qt
        OUTPUT_SCRIPT qt_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    qt_generate_deploy_app_script(
        TARGET images2pdf-qt
        OUTPUT_SCRIPT qt_deploy_runtime_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${qt_deploy_script})
    install(SCRIPT ${qt_deploy_runtime_script})
else()
    message(STATUS "Skipping qt6_deploy (IMAGES2PDF_QT_ENABLE_DEPLOY=OFF)")
endif()
