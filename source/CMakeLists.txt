cmake_minimum_required(VERSION 3.25)

project(TapeStopper VERSION 1.42 LANGUAGES C CXX)

set(JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../_Tools/JUCE/_9.0.4")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE 9.0.4 was not found at: ${JUCE_DIR}")
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(TapeStopper
    VERSION 1.42
    COMPANY_NAME "sl23"
    BUNDLE_ID "com.sl23.tapestopper"
    PLUGIN_MANUFACTURER_CODE sl23
    PLUGIN_CODE TpSt
    FORMATS VST3
    PRODUCT_NAME "TapeStopper"
    DESCRIPTION "TapeStopper v1.42 built with JUCE 9.0.4"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE)

juce_generate_juce_header(TapeStopper)

target_sources(TapeStopper
    PRIVATE
        PluginProcessor.cpp
        PluginProcessor.h
        PluginEditor.cpp
        PluginEditor.h
        PortableSettings.cpp
        PortableSettings.h)

target_compile_features(TapeStopper PRIVATE cxx_std_17)

target_compile_definitions(TapeStopper
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TapeStopper
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
