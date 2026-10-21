cmake_minimum_required(VERSION 3.22)

project(Interference VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/MP /permissive- /bigobj)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(INTERFERENCE_ASAN "Build with AddressSanitizer (MSVC /fsanitize=address)" OFF)
if(INTERFERENCE_ASAN AND MSVC)
    add_compile_options(/fsanitize=address /Zi)
    add_link_options(/INCREMENTAL:NO)
endif()

# ---------------------------------------------------------------------------
# JUCE 8.0.8 (pinned). Looked up in this order: -DINTERFERENCE_JUCE_DIR, ./JUCE,
# ../JUCE_8_0_8, else fetched from GitHub at the pinned tag.
# ---------------------------------------------------------------------------
set(INTERFERENCE_JUCE_DIR "" CACHE PATH "Path to a JUCE 8.0.8 checkout")
if(INTERFERENCE_JUCE_DIR AND EXISTS "${INTERFERENCE_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${INTERFERENCE_JUCE_DIR}" JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE_8_0_8/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE_8_0_8" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.8 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

set(IFX_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    IFX_VERSION_STRING="${PROJECT_VERSION}"
)

# Shared, host-independent sources (no GUI): state, presets, rendering.
set(IFX_SHARED_SOURCES
    Source/Plugin/StateIO.cpp
    Source/Plugin/FactoryPresets.cpp
    Source/Render/OfflineRenderer.cpp
)

set(IFX_UI_SOURCES
    Source/UI/Theme.cpp
    Source/UI/Widgets.cpp
    Source/UI/Illustrations.cpp
    Source/UI/SlotPanel.cpp
    Source/UI/LaneEditor.cpp
    Source/UI/StudioPanel.cpp
    Source/UI/Overlays.cpp
    Source/UI/HelpContent.cpp
    Source/UI/MainView.cpp
)

set(IFX_MODULES
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

set(IFX_HAVE_PLUGIN OFF)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Plugin/PluginProcessor.cpp")
    set(IFX_HAVE_PLUGIN ON)
endif()

# ---------------------------------------------------------------------------
# Plugin (VST3 + Standalone; AU added on macOS)
# ---------------------------------------------------------------------------
if(IFX_HAVE_PLUGIN)
    set(IFX_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND IFX_FORMATS AU)
    endif()

    juce_add_plugin(Interference
        COMPANY_NAME                "Interference Audio"
        COMPANY_WEBSITE             "https://interference-audio.example"
        COMPANY_EMAIL               "support@interference-audio.example"
        BUNDLE_ID                   "audio.interference.interference"
        PLUGIN_MANUFACTURER_CODE    Ifrn
        PLUGIN_CODE                 Ifx1
        FORMATS                     ${IFX_FORMATS}
        PRODUCT_NAME                "Interference"
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            TRUE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        VST3_CATEGORIES             "Fx" "Distortion"
        AU_MAIN_TYPE                kAudioUnitType_Effect
        ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
        ICON_SMALL                  "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
        MICROPHONE_PERMISSION_ENABLED FALSE
    )
    target_sources(Interference PRIVATE
        ${IFX_SHARED_SOURCES}
        ${IFX_UI_SOURCES}
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp
        Source/Plugin/Diagnostics.cpp
        Source/Plugin/CrashHookWin.cpp
    )
    target_include_directories(Interference PRIVATE Source)
    target_link_libraries(Interference PRIVATE ${IFX_MODULES}
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
    juce_generate_juce_header(Interference)
    target_compile_definitions(Interference PUBLIC ${IFX_DEFS}
        JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_MODAL_LOOPS_PERMITTED=0)
endif()

# ---------------------------------------------------------------------------
# Offline DSP / state / render tests (console)
# ---------------------------------------------------------------------------
juce_add_console_app(InterferenceTests PRODUCT_NAME "InterferenceTests")
set(IFX_TEST_SOURCES
    Tests/TestMain.cpp
    Tests/TestCore.cpp
    Tests/TestSources.cpp
    Tests/TestEngine.cpp
    Tests/TestStudio.cpp
    Tests/TestPattern.cpp
    Tests/TestState.cpp
    Tests/TestRender.cpp
    Tests/TestPerf.cpp
    Tests/TestEnvironment.cpp
    Tests/TestLimits.cpp
    Tests/TestScenarios.cpp
    Tests/TestRf.cpp
    ${IFX_SHARED_SOURCES}
)
target_sources(InterferenceTests PRIVATE ${IFX_TEST_SOURCES})
target_include_directories(InterferenceTests PRIVATE Source)
target_link_libraries(InterferenceTests PRIVATE ${IFX_MODULES}
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
juce_generate_juce_header(InterferenceTests)
target_compile_definitions(InterferenceTests PUBLIC ${IFX_DEFS}
    JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 IFX_HEADLESS=1)

# ---------------------------------------------------------------------------
# GUI tests: builds the real editor headlessly
# ---------------------------------------------------------------------------
if(IFX_HAVE_PLUGIN AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/TestGui.cpp")
    juce_add_console_app(InterferenceGuiTests PRODUCT_NAME "InterferenceGuiTests")
    target_sources(InterferenceGuiTests PRIVATE
        ${IFX_SHARED_SOURCES} ${IFX_UI_SOURCES}
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp
        Source/Plugin/Diagnostics.cpp
        Source/Plugin/CrashHookWin.cpp
        Tests/TestMain.cpp
        Tests/TestGui.cpp)
    target_include_directories(InterferenceGuiTests PRIVATE Source)
    target_link_libraries(InterferenceGuiTests PRIVATE ${IFX_MODULES}
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    juce_generate_juce_header(InterferenceGuiTests)
    target_compile_definitions(InterferenceGuiTests PUBLIC ${IFX_DEFS}
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 IFX_HEADLESS=1 IFX_GUI_TESTS=1
        JucePlugin_Name="Interference" JucePlugin_Build_Standalone=0)
endif()
