add_library(pitwall_driving_session driving_session.cpp)
target_include_directories(pitwall_driving_session PUBLIC .)
target_link_libraries(pitwall_driving_session PUBLIC pitwall::simulation pitwall::track)

add_executable(pitwall_simulator main.cpp)
target_link_libraries(pitwall_simulator PRIVATE pitwall_driving_session)
target_compile_definitions(pitwall_simulator PRIVATE PITWALL_VERSION="${PROJECT_VERSION}")

if(PITWALL_BUILD_GUI)
    configure_file(pitwall.rc.in "${CMAKE_CURRENT_BINARY_DIR}/pitwall.rc" @ONLY)
    add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/imgui ${PROJECT_BINARY_DIR}/third_party/imgui)
    target_sources(pitwall_simulator PRIVATE gui_win32.cpp gui_view.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/pitwall.rc")
    target_link_libraries(pitwall_simulator PRIVATE pitwall_imgui)
    target_compile_definitions(pitwall_simulator PRIVATE PITWALL_HAS_GUI NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        # The manifest is embedded as resource 1 by pitwall.rc for both toolchains.
        target_link_options(pitwall_simulator PRIVATE /MANIFEST:NO)
    endif()
endif()
