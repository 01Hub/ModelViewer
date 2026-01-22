cmake_minimum_required(VERSION 3.18)

project(ModelViewer)

find_package(assimp REQUIRED)
find_package(Freetype REQUIRED)
find_package(glm REQUIRED)
find_package(OpenCASCADE REQUIRED)
find_package(meshoptimizer REQUIRED)
find_package(basisu CONFIG REQUIRED)

message(STATUS "VCPKG_INSTALLED_DIR: ${VCPKG_INSTALLED_DIR}")

if(NOT OpenCASCADE_FOUND)
    message(FATAL_ERROR "Cannot build the executable without OpenCASCADE. Please set the variable OpenCASCADE_DIR.")
else()
    message("Found OpenCASCADE " ${OpenCASCADE_VERSION})
endif()

find_package(Qt6 COMPONENTS Core Gui OpenGLWidgets Widgets LinguistTools)

# Optional: WebEngine - just check for it, don't link yet
find_package(Qt6 COMPONENTS WebEngineWidgets QUIET)

if(Qt6WebEngineWidgets_FOUND)
    message(STATUS "Qt WebEngine found - tutorials will use WebEngine")
    set(HAVE_WEBENGINE TRUE)
else()
    message(WARNING "Qt WebEngine not found - tutorials will use QTextBrowser")
    set(HAVE_WEBENGINE FALSE)
endif()

if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    set(QT_TRANSLATE_NAMESPACE Qt6)
else()
    message("Qt6 not found")
endif()

message("Using Qt${QT_VERSION_MAJOR}")

# ============================================================================
# Define vcpkg plugins directory (different for debug/release)
# ============================================================================
if(DEFINED VCPKG_INSTALLED_DIR)
    if(DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_BASE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    else()
        set(VCPKG_BASE "${VCPKG_INSTALLED_DIR}/x64-windows")
    endif()
    
    # For debug builds, plugins are in debug/Qt6/plugins
    # For release builds, plugins are in Qt6/plugins
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        set(QT_VCPKG_PLUGINS_DIR "${VCPKG_BASE}/debug/Qt6/plugins")
        set(DEBUG_SUFFIX "d")
    else()
        set(QT_VCPKG_PLUGINS_DIR "${VCPKG_BASE}/Qt6/plugins")
        set(DEBUG_SUFFIX "")
    endif()
    
    message(STATUS "VCPKG_INSTALLED_DIR: ${VCPKG_INSTALLED_DIR}")
    message(STATUS "VCPKG_TARGET_TRIPLET: ${VCPKG_TARGET_TRIPLET}")
    message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
    message(STATUS "Qt plugins directory: ${QT_VCPKG_PLUGINS_DIR}")
    
    # Check if directory exists and list plugins
    if(EXISTS "${QT_VCPKG_PLUGINS_DIR}/imageformats")
        message(STATUS "Image format plugins found!")
        file(GLOB IMAGEFORMAT_PLUGINS "${QT_VCPKG_PLUGINS_DIR}/imageformats/*.dll")
        message(STATUS "Available plugins:")
        foreach(PLUGIN ${IMAGEFORMAT_PLUGINS})
            get_filename_component(PLUGIN_NAME ${PLUGIN} NAME)
            message(STATUS "  ${PLUGIN_NAME}")
        endforeach()
    else()
        message(WARNING "Image format plugins directory not found: ${QT_VCPKG_PLUGINS_DIR}/imageformats")
    endif()
else()
    message(WARNING "VCPKG_INSTALLED_DIR not defined")
endif()

# ============================================================================
# Function to deploy image format plugins
# ============================================================================
function(deploy_image_plugin TARGET_NAME PLUGIN_NAME)
    if(NOT DEFINED QT_VCPKG_PLUGINS_DIR)
        return()
    endif()
    
    # Plugin file with appropriate suffix
    set(PLUGIN_FILE "${PLUGIN_NAME}${DEBUG_SUFFIX}.dll")
    set(PLUGIN_SOURCE "${QT_VCPKG_PLUGINS_DIR}/imageformats/${PLUGIN_FILE}")
    
    if(EXISTS "${PLUGIN_SOURCE}")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory 
                "$<TARGET_FILE_DIR:${TARGET_NAME}>/plugins/imageformats"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PLUGIN_SOURCE}"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>/plugins/imageformats/${PLUGIN_FILE}"
            COMMENT "Copying image format plugin: ${PLUGIN_FILE}"
            VERBATIM
        )
        message(STATUS "Will deploy plugin: ${PLUGIN_FILE}")
    else()
        message(WARNING "Plugin not found: ${PLUGIN_SOURCE}")
    endif()
endfunction()

include(GNUInstallDirs)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Handle UI files manually
file(GLOB ui_sources ui/*.ui)
qt6_wrap_ui(ui_wrapped ${ui_sources})

# ============================================================================
# CREATE THE TARGET - This must come before any target_* commands
# ============================================================================
add_executable(ModelViewer)

set(APP_VERSION_MAJOR 1)
set(APP_VERSION_MINOR 2)
set(APP_VERSION_PATCH 3)


if(WIN32)
    # Windows: will compute at runtime relative to exe
    set(USE_RELATIVE_DATA_DIR 1)
    set(DATA_DIR_VALUE "${PROJECT_NAME}")
else()
    # Linux: absolute path from make install
    set(USE_RELATIVE_DATA_DIR 0)
    set(DATA_DIR_VALUE "${CMAKE_INSTALL_FULL_DATAROOTDIR}/${PROJECT_NAME}")
endif()

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
add_custom_target(update_translations
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ModelViewer --parallel 1 || true
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
        ${ui_wrapped}
        ModelViewer.qrc
        ${translation_resources}
)

target_include_directories(ModelViewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
        ${OpenCASCADE_INCLUDE_DIR}
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)

# ============================================================================
# Link libraries - NOW the target exists
# ============================================================================
target_link_libraries(ModelViewer
    PRIVATE
        assimp::assimp
        Freetype::Freetype
        glm::glm
        meshoptimizer::meshoptimizer
        Qt6::Core
        Qt6::Gui
        Qt6::OpenGLWidgets        
        Qt6::Widgets        
        basisu::basisu_encoder
        ${OpenCASCADE_FoundationClasses_LIBRARIES}
        ${OpenCASCADE_ModelingData_LIBRARIES}
        ${OpenCASCADE_ModelingAlgorithms_LIBRARIES}
        ${OpenCASCADE_DataExchange_LIBRARIES}
        ${OpenCASCADE_ApplicationFramework_LIBRARIES}
)

# Add WebEngine if found
if(HAVE_WEBENGINE)
    target_compile_definitions(ModelViewer PRIVATE HAVE_WEBENGINE)
    target_link_libraries(ModelViewer PRIVATE Qt6::WebEngineWidgets)
endif()


# ============================================================================
# Deploy image format plugins - MUST BE AFTER add_executable()
# ============================================================================
deploy_image_plugin(ModelViewer "qwebp")
deploy_image_plugin(ModelViewer "qjpeg")
deploy_image_plugin(ModelViewer "qgif")
deploy_image_plugin(ModelViewer "qico")
deploy_image_plugin(ModelViewer "qtiff")
deploy_image_plugin(ModelViewer "qwbmp")

# ============================================================================
# Deploy WebP libraries and their dependencies (optional, non-fatal) Windows only
# ============================================================================

if(WIN32)
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        if(CMAKE_BUILD_TYPE MATCHES "Debug")
            set(LIBWEBP_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
        else()
            set(LIBWEBP_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
        endif()
        
    # Check if WebP libraries exist - only deploy if available
        if(EXISTS "${LIBWEBP_BIN_DIR}/libwebp.dll")
            message(STATUS "WebP libraries found in vcpkg, will deploy (Windows only)")
            
        # List of WebP libraries AND their dependencies to deploy
            set(LIBWEBP_DLLS 
                "libwebp.dll"
                "libwebpdecoder.dll"
                "libwebpdemux.dll"
                "libwebpmux.dll"
                "libsharpyuv.dll"
            )
            
            foreach(WEBP_DLL ${LIBWEBP_DLLS})
                set(WEBP_DLL_SOURCE "${LIBWEBP_BIN_DIR}/${WEBP_DLL}")
                
                if(EXISTS "${WEBP_DLL_SOURCE}")
                    # Copy to bin/ directory
                    add_custom_command(TARGET ModelViewer POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${WEBP_DLL_SOURCE}"
                            "$<TARGET_FILE_DIR:ModelViewer>/${WEBP_DLL}"
                        VERBATIM
                    )
                    
                    # Also copy to plugins/imageformats/
                    add_custom_command(TARGET ModelViewer POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory
                            "$<TARGET_FILE_DIR:ModelViewer>/plugins/imageformats"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${WEBP_DLL_SOURCE}"
                            "$<TARGET_FILE_DIR:ModelViewer>/plugins/imageformats/${WEBP_DLL}"
                        VERBATIM
                    )
                    
                    message(STATUS "Will deploy: ${WEBP_DLL}")
                else()
                    message(WARNING "WebP library not found (optional): ${WEBP_DLL_SOURCE}")
                endif()
            endforeach()
        else()
        message(STATUS "WebP libraries not found in vcpkg - WebP support will not be available")
        # Non-fatal - build continues without WebP
        endif()
    endif()
else()
    # Linux / macOS: Qt finds plugins and libraries through standard paths
    message(STATUS "Non-Windows platform detected - skipping WebP DLL deployment (not needed)")
endif()


# ============================================================================
# Create qt.conf - Windows only
# ============================================================================

if(WIN32)
    add_custom_command(TARGET ModelViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "[Paths]" > "$<TARGET_FILE_DIR:ModelViewer>/qt.conf"
        COMMAND ${CMAKE_COMMAND} -E echo "Plugins = ../plugins" >> "$<TARGET_FILE_DIR:ModelViewer>/qt.conf"
        COMMENT "Creating qt.conf (Windows only)"
        VERBATIM
    )
endif()

if(WIN32)
    target_sources(ModelViewer
        PRIVATE
            ModelViewer.rc
    )
endif()

install(TARGETS ModelViewer)

# Install qt.conf - Windows only
if(WIN32)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()

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

# Install image format plugins deployed during build
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins"
        DESTINATION "."
        OPTIONAL
)

# Install WebP libraries - Windows only
if(WIN32 AND DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        set(LIBWEBP_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
    else()
        set(LIBWEBP_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    endif()
    
    install(FILES 
        "${LIBWEBP_BIN_DIR}/libwebp.dll"
        "${LIBWEBP_BIN_DIR}/libwebpdecoder.dll"
        "${LIBWEBP_BIN_DIR}/libwebpdemux.dll"
        "${LIBWEBP_BIN_DIR}/libwebpmux.dll"
        "${LIBWEBP_BIN_DIR}/libsharpyuv.dll"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        OPTIONAL
    )
endif()

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
