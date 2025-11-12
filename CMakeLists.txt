# ==============================================================================
# VoidEngine - VST3/AU Audio Plugin
# Based on JUCE AudioPluginDemo example
# ==============================================================================

cmake_minimum_required(VERSION 3.22)

# If you've installed JUCE somehow (via a package manager, or directly using the CMake install
# target), you'll need to tell this project that it depends on the installed copy of JUCE. If you've
# included JUCE directly in your source tree (perhaps as a submodule), you'll need to tell CMake to
# include that subdirectory as part of the build.

# Option 1: If JUCE is installed system-wide
# find_package(JUCE CONFIG REQUIRED)

# Option 2: If JUCE is in a subdirectory (uncomment and adjust path)
# add_subdirectory(JUCE)

# Option 3: Specify JUCE path via CMake variable (recommended)
# Run: cmake -B build -DJUCE_DIR=/path/to/JUCE
if(NOT JUCE_DIR)
    message(FATAL_ERROR "JUCE_DIR must be set. Example: cmake -B build -DJUCE_DIR=/path/to/JUCE")
endif()

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE_DIR must point to JUCE root directory containing CMakeLists.txt")
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

# If you are building a VST2 or AAX plugin, CMake needs to be told where to find these SDKs on your
# system. This setup should be done before calling `juce_add_plugin`.

# juce_set_vst2_sdk_path(...)
# juce_set_aax_sdk_path(...)

# If building VST3, you may need to set VST3_SDK_DIR
# Run: cmake -B build -DVST3_SDK_DIR=/path/to/VST3_SDK

project(AudioPluginDemo VERSION 1.6.0)

# `juce_add_plugin` adds a static library target with the name passed as the first argument.
# This function accepts many optional arguments. Check the readme at `docs/CMake API.md` in the JUCE repo.

juce_add_plugin(AudioPluginDemo
    VERSION 1.6.0
    COMPANY_NAME "JUCE"
    COMPANY_WEBSITE "http://juce.com"
    COMPANY_EMAIL ""
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Juce
    PLUGIN_CODE Dem0
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "AudioPluginDemo")

# `target_sources` adds source files to a target.

target_sources(AudioPluginDemo
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
        Source/HelpTooltip.cpp
        Source/DSP/GranularEngine.cpp
        Source/DSP/SpectralEngine.cpp
        Source/DSP/SpaceEngine.cpp
        Source/DSP/DynamicLayer.cpp
        Source/DSP/MotionMod.cpp
        Source/DSP/BinauralFlow.cpp
        Source/DSP/HarmonicGlide.cpp)

# `target_compile_definitions` adds some preprocessor definitions to our target.

target_compile_definitions(AudioPluginDemo
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

# Убедимся что компилятор использует UTF-8 для исходных файлов
# Это критично для правильного отображения кириллицы в строках
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Указываем компилятору, что исходные файлы в UTF-8
    target_compile_options(AudioPluginDemo PRIVATE 
        "$<$<COMPILE_LANGUAGE:CXX>:-finput-charset=UTF-8>"
        "$<$<COMPILE_LANGUAGE:CXX>:-fexec-charset=UTF-8>")
endif()

# Для macOS/Xcode явно указываем кодировку исходных файлов
if(APPLE AND CMAKE_GENERATOR MATCHES "Xcode")
    set_target_properties(AudioPluginDemo PROPERTIES
        XCODE_ATTRIBUTE_GCC_INPUT_FILETYPE "sourcecode.cpp.objcpp")
endif()

# `target_link_libraries` links libraries and JUCE modules to other libraries or executables.

# Offline renderer executable
add_executable(offline_render
    Source/offline_render.cpp
    Source/OfflineRenderer.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/HelpTooltip.cpp
    Source/DSP/GranularEngine.cpp
    Source/DSP/SpectralEngine.cpp
    Source/DSP/SpaceEngine.cpp
    Source/DSP/DynamicLayer.cpp
    Source/DSP/MotionMod.cpp
    Source/DSP/BinauralFlow.cpp
    Source/DSP/HarmonicGlide.cpp
)

target_link_libraries(offline_render
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
)

target_compile_definitions(offline_render
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AudioPluginDemo
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ==============================================================================
# Тесты
# ==============================================================================

# Простой исполняемый тест (без GUI)
add_executable(test_basic
    tests/test_basic.cpp
    Source/DSP/SpectralEngine.cpp
    Source/DSP/SpaceEngine.cpp
    Source/DSP/MotionMod.cpp
    Source/DSP/GranularEngine.cpp
    Source/DSP/DynamicLayer.cpp
)

target_link_libraries(test_basic
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Для macOS нужно указать минимальную версию
if(APPLE)
    set_target_properties(test_basic PROPERTIES
        MACOSX_BUNDLE FALSE
        MACOSX_BUNDLE_INFO_PLIST "")
endif()

