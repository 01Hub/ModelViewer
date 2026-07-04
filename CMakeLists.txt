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
    message(STATUS "Found OpenCASCADE " ${OpenCASCADE_VERSION})
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

set(APP_VERSION_MAJOR 2026)
set(APP_VERSION_MINOR 7)
set(APP_VERSION_PATCH 0)


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
# Set target properties
# ============================================================================
if(WIN32)
    set_target_properties(ModelViewer PROPERTIES WIN32_EXECUTABLE ON)
endif()

# OCCT's STEP transfer recurses deeply through TransferEntity → TransferShape
# chains for assembly files.  In debug builds (no inlining, /RTC1 frame checks)
# the default 1 MB stack is insufficient.  16 MB is safe headroom without waste.
if(MSVC)
    target_link_options(ModelViewer PRIVATE $<$<CONFIG:Debug>:/STACK:16777216>)
endif()

# ============================================================================
# Link libraries - NOW the target exists
# ============================================================================
# On Linux, force the linker (GNU ld) to keep rescanning assimp::assimp until
# no new undefined references remain, instead of a single left-to-right pass.
# Needed because Assimp's aiMatrix4x4t<float> default constructor is an extern
# template instantiation living in the compiled archive rather than inline in
# headers, and static-archive member selection depends on which object files
# reference it - which can differ between Debug/Release inlining, causing an
# intermittent "undefined reference to aiMatrix4x4t<float>::aiMatrix4x4t()"
# link failure without this. $<LINK_GROUP:RESCAN,...> is a GNU ld-specific
# feature (CMake 3.24+) that MSVC's linker does not support, so it's scoped to
# Linux only here.
if(UNIX AND NOT APPLE)
    set(_assimp_link_item "$<LINK_GROUP:RESCAN,assimp::assimp>")
else()
    set(_assimp_link_item "assimp::assimp")
endif()

target_link_libraries(ModelViewer
    PRIVATE
        ${_assimp_link_item}
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

install(DIRECTORY fonts shaders themes test-models data
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}"
)

# textures/envmap/skyboxes/HDRI contains ~65 optional, browsable environment-map
# packs (~1.3GB) in addition to the 3 default presets (studio/outdoor/office.hdr)
# referenced directly by ViewportWidget. textures/materials contains ~129
# optional, browsable PBR material presets (~2.2GB) with no code-level
# defaults. Debug installs (local dev/testing) get the full libraries so you
# can browse/test everything; Release installs (used to build the
# installer/AppImage) ship only a curated minimum of each, to keep package
# size reasonable. CONFIGURATIONS is used rather than a configure-time
# if(CMAKE_BUILD_TYPE...) check because multi-config generators (e.g. Visual
# Studio) only know the configuration at build/install time, not configure time.
install(DIRECTORY textures
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}"
        CONFIGURATIONS Debug
)

# Release installs: rather than relying on install(DIRECTORY)'s REGEX/PATTERN
# EXCLUDE (whose path-matching semantics proved ambiguous/unreliable in
# practice), explicitly install every part of textures/ EXCEPT HDRI and
# materials by name, with no exclude patterns involved at all. The curated
# HDRI/materials subsets are installed individually just below.
install(DIRECTORY textures/khronos
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/textures"
        CONFIGURATIONS Release
)
install(DIRECTORY textures/patterns
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/textures"
        CONFIGURATIONS Release
)
install(DIRECTORY textures/envmap/floor
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/textures/envmap"
        CONFIGURATIONS Release
)
install(DIRECTORY textures/envmap/skyboxes/LDRI
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/textures/envmap/skyboxes"
        CONFIGURATIONS Release
)

# The 3 loose default .hdr files (not inside a named pack subfolder) are
# referenced directly by ViewportWidget's Studio/Outdoor/Office quick presets,
# so they must ship regardless of which packs are curated below.
install(FILES
            "textures/envmap/skyboxes/HDRI/studio.hdr"
            "textures/envmap/skyboxes/HDRI/outdoor.hdr"
            "textures/envmap/skyboxes/HDRI/office.hdr"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/textures/envmap/skyboxes/HDRI"
        CONFIGURATIONS Release
)

set(_hdri_included_packs
    "@Default" "Canary_Wharf" "Colorful_Studio" "Dancing_Hall"
    "Evening_Museum_Courtyard" "Footprint_Court" "Frozenwaterfall"
    "Newport_Loft" "Venice_Sunset"
)
foreach(_pack ${_hdri_included_packs})
    install(DIRECTORY "textures/envmap/skyboxes/HDRI/${_pack}"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/textures/envmap/skyboxes/HDRI"
            CONFIGURATIONS Release
    )
endforeach()

set(_material_included_presets
    "ClearCoat_Metallic_Blue" "Fabric_Denim_02" "Fabric_Fabric050"
    "Fabric_Leather026" "Fabric_Leather030" "Fabric_Leather038"
    "Ground_Cobblestone_Floor_07" "Metal_Aluminium" "Metal_Bronze"
    "Metal_Bronze-1" "Metal_Brushed-Metal" "Metal_Chrome" "Metal_Copper"
    "Metal_Dull_Brass" "Metal_Dull_Copper" "Metal_Gold" "Metal_Iron"
    "Metal_Lead" "Metal_Magnesium" "Metal_Nickel" "Metal_Ornate_Brass"
    "Metal_Pewter" "Metal_Pirate_Gold" "Metal_Pitted_Metal" "Metal_Platinum"
    "Metal_Rose_Gold" "Metal_Rusted_Iron" "Metal_Silver_Dull"
    "Metal_Stainless_Steel" "Metal_Steel" "Metal_Titanium" "Metal_Zinc"
    "Organic_Human_Skin-Freckled" "Organic_Human_Skin1" "Paper_Paper001"
    "Paper_Paper005" "Plastic_ABS_Glossy" "Plastic_ABS_Matte"
    "Plastic_Plastic007" "Plastic_Plastic012a" "Plastic_Plastic016a"
    "Plastic_Scuffed-1" "Rubber_Rubber002" "Rubber_Tire" "Stone_Granite006a"
    "Stone_Marble009" "Stone_Rock_06" "Synthetic_Sponge001" "Wall_Bricks"
    "Wall_Bricks_Plaster" "Wall_Glass_Window" "Wood_Antique-Veneer1"
    "Wood_Bark_Brown_02" "Wood_KitchenWood" "Wood_Plywood" "Wood_Wood051"
)
foreach(_preset ${_material_included_presets})
    install(DIRECTORY "textures/materials/${_preset}"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/textures/materials"
            CONFIGURATIONS Release
    )
endforeach()

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
