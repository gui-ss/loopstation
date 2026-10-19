cmake_minimum_required(VERSION 3.24)

project(MK2Loopstation VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the small real-time vocoder optimised even in Debug. The rest of the
# application retains Debug checks/symbols; /RTC instrumentation in this hot
# translation unit exceeded the audio deadline at low NOTE (6H.2 measurements).
set_source_files_properties(Source/RobotEffect.cpp Source/OscBotEffect.cpp Source/FxSequence.cpp Source/ReverbEffect.cpp Source/StepSlicerEffect.cpp Source/G2BEffect.cpp PROPERTIES COMPILE_OPTIONS
    "$<$<CXX_COMPILER_ID:MSVC>:/O2;/Ob2>"
    VS_SETTINGS "BasicRuntimeChecks=Default")

include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.6.tar.gz
    URL_HASH SHA256=95752f8e8b37d802b4c10182bc757de7d88f0c8899e879afa8798589be306a11
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(MK2Loopstation PRODUCT_NAME "MK2 Loopstation")

target_sources(MK2Loopstation PRIVATE
    Source/InputFxShortcuts.h
    Source/Main.cpp
    Source/AudioSystem.h
    Source/MusicalClock.h
    Source/AudioSystem.cpp
    Source/LoopTrack.h
    Source/LoopStorage.h
    Source/LoopTrack.cpp
    Source/TrackEffects.h
    Source/TrackEffects.cpp
    Source/TrackIndicator.h
    Source/TrackFxRouting.h
    Source/RobotParameters.h
    Source/RobotEffect.h
    Source/RobotEffect.cpp
    Source/FxSequence.h
    Source/FxSequence.cpp
    Source/OscBotParameters.h
    Source/ReverbParameters.h
    Source/StepSlicerParameters.h
    Source/StepSlicerEffect.h
    Source/StepSlicerEffect.cpp
    Source/G2BParameters.h
    Source/G2BEffect.h
    Source/G2BEffect.cpp
    Source/ReverbEffect.h
    Source/ReverbEffect.cpp
    Source/OscBotEffect.h
    Source/OscBotEffect.cpp
    Source/TrackFxParameters.h
    Source/AudioSettingsWindow.h
    Source/AudioSettingsWindow.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/TrackComponent.h
    Source/TrackComponent.cpp
    Source/TrackControls.h
    Source/TrackControls.cpp
    Source/FxKnobPickup.h
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
    target_sources(MK2AudioTests PRIVATE Tests/RobotQualityTests.cpp)
    target_sources(MK2AudioTests PRIVATE Source/FxSequence.cpp Source/OscBotEffect.cpp Tests/OscBotEffectTests.cpp Tests/FxSequenceTests.cpp)
    target_sources(MK2AudioTests PRIVATE Source/ReverbEffect.cpp Tests/ReverbEffectTests.cpp Source/StepSlicerEffect.cpp Tests/StepSlicerEffectTests.cpp Source/G2BEffect.cpp Tests/G2BEffectTests.cpp)
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
