cmake_minimum_required(VERSION 3.25)

project(TapeStopper VERSION 0.6.1 LANGUAGES C CXX)

set(JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../_Tools/JUCE/_8.0.15")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE 8.0.15 was not found at: ${JUCE_DIR}")
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(TapeStopper
    COMPANY_NAME "Provisional"
    BUNDLE_ID "com.provisional.tapestopper"
    PLUGIN_MANUFACTURER_CODE TstM
    PLUGIN_CODE TstP
    FORMATS VST3
    PRODUCT_NAME "TapeStopper"
    DESCRIPTION "TapeStopper Stage 6.1 timing-bar hit-area correction"
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
