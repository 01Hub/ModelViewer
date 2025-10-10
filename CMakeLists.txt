cmake_minimum_required(VERSION 3.18)

project(ModelViewer)

find_package(assimp REQUIRED)
find_package(Freetype REQUIRED)
find_package(glm REQUIRED)
find_package(OpenCASCADE REQUIRED)
find_package(meshoptimizer REQUIRED)

if(NOT OpenCASCADE_FOUND)
    message(FATAL_ERROR "Cannot build the executable without OpenCASCADE. Please set the variable OpenCASCADE_DIR.")
else()
    message("Found OpenCASCADE " ${OpenCASCADE_VERSION})
endif()

find_package(Qt6 COMPONENTS Core Gui OpenGLWidgets Widgets LinguistTools)

if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    set(QT_TRANSLATE_NAMESPACE Qt6)
else()
    message("Qt6 not found")
endif()

message("Using Qt${QT_VERSION_MAJOR}")

include(GNUInstallDirs)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# Don't use AUTOUIC - we'll handle UI files manually

# Handle UI files manually
file(GLOB ui_sources ui/*.ui)
qt6_wrap_ui(ui_wrapped ${ui_sources})

add_executable(ModelViewer)

set(APP_VERSION_MAJOR 1)
set(APP_VERSION_MINOR 2)
set(APP_VERSION_PATCH 3)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/config.h"
  @ONLY
)

file(GLOB sources src/*.cpp src/*.cxx src/*.c include/*.h include/*.hxx)
file(GLOB ui_sources ui/*.ui)

# Create translations directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/translations)

set(TS_FILES
    translations/modelviewer_en.ts
    translations/modelviewer_fr.ts
    translations/modelviewer_de.ts
    translations/modelviewer_es.ts
    translations/modelviewer_it.ts
)

# Create empty .ts files if they don't exist
foreach(_ts ${TS_FILES})
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_ts})
        file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/${_ts}
"<?xml version=\"1.0\" encoding=\"utf-8\"?>
<!DOCTYPE TS>
<TS version=\"2.1\">
</TS>
")
    endif()
endforeach()

# Generate/update translation files using lupdate
# Make sure UI files are processed first so ui_*.h files exist
# Custom target to update translations for Qt6
add_custom_target(update_translations
    # First ensure UI files are processed
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ModelViewer --parallel 1 || true
    # Then run lupdate on all source files including generated UI headers
    COMMAND ${Qt6_LUPDATE_EXECUTABLE}
        ${sources} ${ui_sources}
        -ts ${TS_FILES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Updating .ts files"
    DEPENDS ${ui_wrapped}
)
    
# Compile .ts files to .qm files
qt6_add_lrelease(ModelViewer
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)


# Debug: Print QM files list
message(STATUS "QM_FILES will be: ${QM_FILES}")

# Create a resource file for translations
set(TRANSLATIONS_QRC ${CMAKE_CURRENT_BINARY_DIR}/translations.qrc)
file(WRITE ${TRANSLATIONS_QRC} "<!DOCTYPE RCC>\n<RCC version=\"1.0\">\n<qresource prefix=\"/translations\">\n")

foreach(qm_file ${QM_FILES})
    get_filename_component(qm_name ${qm_file} NAME)
    file(APPEND ${TRANSLATIONS_QRC} "    <file>${qm_file}</file>\n")
endforeach()

file(APPEND ${TRANSLATIONS_QRC} "</qresource>\n</RCC>")

# Add the translations resource to the executable
qt_add_resources(translation_resources ${TRANSLATIONS_QRC})

target_sources(ModelViewer
    PRIVATE
        ${sources}
        ${ui_wrapped}  # Use the manually wrapped UI files
        ModelViewer.qrc
        ${translation_resources}
)

target_include_directories(ModelViewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
        ${OpenCASCADE_INCLUDE_DIR}
)

target_link_libraries(ModelViewer
    PRIVATE
        assimp::assimp
        Freetype::Freetype
        glm::glm
        meshoptimizer::meshoptimizer
        Qt::Core
        Qt::Gui
        $<$<EQUAL:${QT_VERSION_MAJOR},6>:Qt::OpenGLWidgets>
        $<$<EQUAL:${QT_VERSION_MAJOR},5>:Qt::OpenGL>
        Qt::Widgets
        ${OpenCASCADE_FoundationClasses_LIBRARIES}
        ${OpenCASCADE_ModelingData_LIBRARIES}
        ${OpenCASCADE_ModelingAlgorithms_LIBRARIES}
        ${OpenCASCADE_DataExchange_LIBRARIES}
        ${OpenCASCADE_ApplicationFramework_LIBRARIES}
)

if(WIN32)
    target_sources(ModelViewer
        PRIVATE
            ModelViewer.rc
    )
endif()

install(TARGETS ModelViewer)

# Install translation files only if they exist
if(QM_FILES)
    install(FILES ${QM_FILES}
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/translations"
    )
    message(STATUS "Translation files will be installed to: ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/translations")
else()
    message(STATUS "No QM files to install")
endif()

install(DIRECTORY fonts shaders textures themes test-models data
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}"
)

if(WIN32)
   install(TARGETS ModelViewer
        RUNTIME_DEPENDENCIES
            PRE_EXCLUDE_REGEXES
                [[hvsifiletrust.*]]
                [[pdmutilities.*]]
                [[api-ms-win-.*]]
                [[ext-ms-.*]]
                [[qt6.*\\.dll]]
            POST_EXCLUDE_REGEXES
                [[.*system32.*.dll]]
    )

    if(QT_VERSION_MAJOR EQUAL 6)
        qt_generate_deploy_app_script(
            TARGET ModelViewer
            OUTPUT_SCRIPT deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
            NO_COMPILER_RUNTIME
        )

        install(SCRIPT ${deploy_script})
    endif()
endif()