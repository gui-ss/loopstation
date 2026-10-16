cmake_minimum_required(VERSION 3.24)

project(MK2Loopstation VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.6.tar.gz
    URL_HASH SHA256=95752f8e8b37d802b4c10182bc757de7d88f0c8899e879afa8798589be306a11
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(MK2Loopstation PRODUCT_NAME "MK2 Loopstation")

target_sources(MK2Loopstation PRIVATE
    Source/Main.cpp
    Source/AudioSystem.h
    Source/MusicalClock.h
    Source/AudioSystem.cpp
    Source/LoopTrack.h
    Source/LoopStorage.h
    Source/LoopTrack.cpp
    Source/TrackEffects.h
    Source/TrackEffects.cpp
    Source/TrackFxRouting.h
    Source/RobotParameters.h
    Source/RobotEffect.h
    Source/RobotEffect.cpp
    Source/TrackFxParameters.h
    Source/AudioSettingsWindow.h
    Source/AudioSettingsWindow.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/TrackComponent.h
    Source/TrackComponent.cpp
    Source/TrackControls.h
    Source/TrackControls.cpp
    Source/PanelControls.h
    Source/PanelControls.cpp
    Source/TopPanelComponent.h
    Source/TopPanelComponent.cpp
    Source/VisualStyle.h
    Source/VisualStyle.cpp
    Source/InputFxEditor.h
    Source/DisplayModel.h
    Source/DisplaySimulation.h
    Source/MemoryDisplay.h
    Source/MemoryDisplay.cpp
)

target_compile_definitions(MK2Loopstation PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_compile_options(MK2Loopstation PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(MK2Loopstation PRIVATE
    juce::juce_gui_basics
    juce::juce_audio_devices
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT MK2Loopstation)

option(MK2_BUILD_AUDIO_TESTS "Build isolated audio-device lifecycle tests" OFF)
if(MK2_BUILD_AUDIO_TESTS)
    enable_testing()
    juce_add_console_app(MK2AudioTests PRODUCT_NAME "MK2 Audio Tests")
    target_sources(MK2AudioTests PRIVATE Tests/AudioSystemTests.cpp Tests/LoopTrackTests.cpp Tests/MusicalClockTests.cpp Source/AudioSystem.cpp Source/LoopTrack.cpp Source/TrackEffects.cpp Source/RobotEffect.cpp Tests/RobotEffectTests.cpp)
    target_include_directories(MK2AudioTests PRIVATE Source)
    target_sources(MK2AudioTests PRIVATE Tests/TempoPanelTests.cpp Source/TopPanelComponent.cpp
        Source/MemoryDisplay.cpp Source/PanelControls.cpp Source/VisualStyle.cpp
        Source/MainComponent.cpp Source/TrackComponent.cpp Source/TrackControls.cpp)
    target_compile_definitions(MK2AudioTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_compile_options(MK2AudioTests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_link_libraries(MK2AudioTests PRIVATE juce::juce_audio_devices juce::juce_dsp juce::juce_gui_basics
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME AudioDeviceLifecycle COMMAND MK2AudioTests)
endif()
