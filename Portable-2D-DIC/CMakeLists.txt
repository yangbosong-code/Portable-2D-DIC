cmake_minimum_required(VERSION 3.20)

project(Portable2DDIC VERSION 0.1.0 LANGUAGES CXX)

option(P2DIC_BUILD_TESTS "Build unit tests" ON)
option(P2DIC_WITH_GALAXY "Build the DAHENG GalaxySDK camera plugin" OFF)
option(P2DIC_WITH_CUDA "Build the CUDA DIC engine" OFF)
option(P2DIC_BUILD_STUDIO "Build the Qt 6 DIC Studio application" OFF)

set(GALAXY_SDK_ROOT "" CACHE PATH "GalaxySDK development directory")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CMake's compiler probe can mojibake the localized MSVC /showIncludes prefix
# on Chinese Windows, which silently breaks Ninja header dependency tracking.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "注意: 包含文件:  ")
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "注意: 包含文件:  ")
endif()

if(P2DIC_WITH_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(NOT CMAKE_CUDA_COMPILER)
        message(FATAL_ERROR "P2DIC_WITH_CUDA=ON but no CUDA compiler was found")
    endif()
    enable_language(CUDA)
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
        set(CMAKE_CUDA_ARCHITECTURES "87;120" CACHE STRING
            "CUDA architectures: 87=Jetson Orin, 120=RTX 50 series")
    endif()
endif()

add_library(p2dic_core
    src/control_protocol.cpp
    src/async_result_writer.cpp
    src/edge_state.cpp
    src/edge_config.cpp
    src/frame_buffer.cpp
    src/hybrid_grid_engine.cpp
    src/pgm_io.cpp
    src/pipeline.cpp
    src/preview_frame.cpp
    src/replay_camera.cpp
    src/result_packet.cpp
    src/result_stream_client.cpp
    src/result_stream_server.cpp
    src/result_summary.cpp
    src/result_writer.cpp
    src/session_binary.cpp
    src/subpixel.cpp
    src/strain_estimator.cpp
    src/synthetic_camera.cpp
    src/tcp_control_server.cpp
    src/tcp_control_client.cpp
    src/zncc.cpp
    src/zncc_grid_engine.cpp
)

target_include_directories(p2dic_core PUBLIC include)

if(WIN32)
    target_link_libraries(p2dic_core PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(p2dic_core PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
else()
    target_compile_options(p2dic_core PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>)
endif()

if(P2DIC_WITH_GALAXY)
    find_path(GALAXY_SDK_INCLUDE_DIR
        NAMES GxIAPI.h
        HINTS
            "${GALAXY_SDK_ROOT}/VC SDK/inc"
            "${GALAXY_SDK_ROOT}/inc"
    )
    find_library(GALAXY_SDK_LIBRARY
        NAMES GxIAPI gxiapi
        HINTS
            "${GALAXY_SDK_ROOT}/VC SDK/lib/x64"
            "${GALAXY_SDK_ROOT}/lib/armv8"
            "${GALAXY_SDK_ROOT}/lib/aarch64"
            "${GALAXY_SDK_ROOT}/lib"
    )
    if(NOT GALAXY_SDK_INCLUDE_DIR OR NOT GALAXY_SDK_LIBRARY)
        message(FATAL_ERROR "GalaxySDK headers/library were not found. Set GALAXY_SDK_ROOT.")
    endif()
    target_sources(p2dic_core PRIVATE src/galaxy_camera.cpp)
    target_include_directories(p2dic_core PRIVATE "${GALAXY_SDK_INCLUDE_DIR}")
    target_link_libraries(p2dic_core PRIVATE "${GALAXY_SDK_LIBRARY}")
    target_compile_definitions(p2dic_core PUBLIC P2DIC_WITH_GALAXY=1)
endif()

if(P2DIC_WITH_CUDA)
    target_sources(p2dic_core PRIVATE src/cuda_grid_engine.cu)
    find_package(CUDAToolkit REQUIRED)
    target_link_libraries(p2dic_core PUBLIC CUDA::cudart)
    target_compile_definitions(p2dic_core PUBLIC P2DIC_WITH_CUDA=1)
    set_target_properties(p2dic_core PROPERTIES
        CUDA_STANDARD 17
        CUDA_STANDARD_REQUIRED ON
        CUDA_SEPARABLE_COMPILATION ON)
endif()

if(P2DIC_WITH_CUDA)
    add_executable(dic_cuda_benchmark apps/dic_cuda_benchmark.cpp)
    target_link_libraries(dic_cuda_benchmark PRIVATE p2dic_core)
endif()

add_executable(dic_benchmark apps/dic_benchmark.cpp)
target_link_libraries(dic_benchmark PRIVATE p2dic_core)

add_executable(dic_edge_sim apps/dic_edge_sim.cpp)
target_link_libraries(dic_edge_sim PRIVATE p2dic_core)

add_executable(dic_ctl apps/dic_ctl.cpp)
target_link_libraries(dic_ctl PRIVATE p2dic_core)

add_executable(dic_edge apps/dic_edge.cpp)
target_link_libraries(dic_edge PRIVATE p2dic_core)

add_executable(dic_export_csv apps/dic_export_csv.cpp)
target_link_libraries(dic_export_csv PRIVATE p2dic_core)

add_executable(dic_perf_monitor apps/dic_perf_monitor.cpp)
target_link_libraries(dic_perf_monitor PRIVATE p2dic_core)

if(P2DIC_BUILD_STUDIO)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network)
    qt_standard_project_setup()
    qt_add_executable(dic_studio
        studio/edge_client.cpp
        studio/edge_client.hpp
        studio/field_view.cpp
        studio/field_view.hpp
        studio/main.cpp
        studio/main_window.cpp
        studio/main_window.hpp
        studio/preview_client.cpp
        studio/preview_client.hpp
        studio/preview_view.cpp
        studio/preview_view.hpp
        studio/result_client.cpp
        studio/result_client.hpp
        studio/signal_view.cpp
        studio/signal_view.hpp)
    target_link_libraries(dic_studio PRIVATE p2dic_core Qt6::Widgets Qt6::Network)
    set_target_properties(dic_studio PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "DIC Studio")

    if(P2DIC_BUILD_TESTS)
        qt_add_executable(test_edge_client
            tests/test_edge_client.cpp
            studio/edge_client.cpp
            studio/edge_client.hpp)
        target_include_directories(test_edge_client PRIVATE studio)
        target_link_libraries(test_edge_client PRIVATE Qt6::Network)
        add_test(NAME edge_client_timeout COMMAND test_edge_client)
        set_tests_properties(edge_client_timeout PROPERTIES
            TIMEOUT 5
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

        qt_add_executable(test_result_client
            tests/test_result_client.cpp
            studio/result_client.cpp
            studio/result_client.hpp)
        target_include_directories(test_result_client PRIVATE studio)
        target_link_libraries(test_result_client PRIVATE p2dic_core Qt6::Network)
        add_test(NAME result_client_roundtrip COMMAND test_result_client)
        set_tests_properties(result_client_roundtrip PROPERTIES
            TIMEOUT 5
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

        qt_add_executable(test_field_view
            tests/test_field_view.cpp
            studio/field_view.cpp
            studio/field_view.hpp)
        target_include_directories(test_field_view PRIVATE studio)
        target_link_libraries(test_field_view PRIVATE p2dic_core Qt6::Widgets)
        add_test(NAME field_view_render COMMAND test_field_view)
        set_tests_properties(field_view_render PROPERTIES
            TIMEOUT 5
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

        qt_add_executable(test_signal_view
            tests/test_signal_view.cpp
            studio/signal_view.cpp
            studio/signal_view.hpp)
        target_include_directories(test_signal_view PRIVATE studio)
        target_link_libraries(test_signal_view PRIVATE p2dic_core Qt6::Widgets)
        add_test(NAME signal_view_render COMMAND test_signal_view)
        set_tests_properties(signal_view_render PROPERTIES
            TIMEOUT 5
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

        qt_add_executable(test_preview_client
            tests/test_preview_client.cpp
            studio/preview_client.cpp
            studio/preview_client.hpp)
        target_include_directories(test_preview_client PRIVATE studio)
        target_link_libraries(test_preview_client PRIVATE p2dic_core Qt6::Network)
        add_test(NAME preview_client_roundtrip COMMAND test_preview_client)
        set_tests_properties(preview_client_roundtrip PROPERTIES
            TIMEOUT 5
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

        qt_add_executable(test_preview_view
            tests/test_preview_view.cpp
            studio/preview_view.cpp
            studio/preview_view.hpp)
        target_include_directories(test_preview_view PRIVATE studio)
        target_link_libraries(test_preview_view PRIVATE p2dic_core Qt6::Widgets Qt6::Network)
        add_test(NAME preview_view_render COMMAND test_preview_view)
        set_tests_properties(preview_view_render PROPERTIES
            TIMEOUT 5
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endif()

if(P2DIC_BUILD_TESTS)
    enable_testing()
    add_executable(test_zncc tests/test_zncc.cpp)
    target_link_libraries(test_zncc PRIVATE p2dic_core)
    add_test(NAME zncc_translation COMMAND test_zncc)

    add_executable(test_synthetic_motion tests/test_synthetic_motion.cpp)
    target_link_libraries(test_synthetic_motion PRIVATE p2dic_core)
    add_test(NAME synthetic_bounded_motion COMMAND test_synthetic_motion)

    add_executable(test_performance_stats tests/test_performance_stats.cpp)
    target_link_libraries(test_performance_stats PRIVATE p2dic_core)
    add_test(NAME performance_stats COMMAND test_performance_stats)

    add_executable(test_runtime tests/test_runtime.cpp)
    target_link_libraries(test_runtime PRIVATE p2dic_core)
    add_test(NAME runtime_pipeline COMMAND test_runtime)

    add_executable(test_pgm tests/test_pgm.cpp)
    target_link_libraries(test_pgm PRIVATE p2dic_core)
    add_test(NAME pgm_roundtrip COMMAND test_pgm)

    add_executable(test_preview_frame tests/test_preview_frame.cpp)
    target_link_libraries(test_preview_frame PRIVATE p2dic_core)
    add_test(NAME preview_frame COMMAND test_preview_frame)

    add_executable(test_subpixel tests/test_subpixel.cpp)
    target_link_libraries(test_subpixel PRIVATE p2dic_core)
    add_test(NAME subpixel_translation COMMAND test_subpixel)

    add_executable(test_strain_estimator tests/test_strain_estimator.cpp)
    target_link_libraries(test_strain_estimator PRIVATE p2dic_core)
    add_test(NAME strain_estimator COMMAND test_strain_estimator)

    add_executable(test_edge_state tests/test_edge_state.cpp)
    target_link_libraries(test_edge_state PRIVATE p2dic_core)
    add_test(NAME edge_state_machine COMMAND test_edge_state)

    add_executable(test_result_writer tests/test_result_writer.cpp)
    target_link_libraries(test_result_writer PRIVATE p2dic_core)
    add_test(NAME result_writer COMMAND test_result_writer)

    add_executable(test_session_binary tests/test_session_binary.cpp)
    target_link_libraries(test_session_binary PRIVATE p2dic_core)
    add_test(NAME session_binary_recovery COMMAND test_session_binary)

    add_executable(test_async_result_writer tests/test_async_result_writer.cpp)
    target_link_libraries(test_async_result_writer PRIVATE p2dic_core)
    add_test(NAME async_result_writer COMMAND test_async_result_writer)

    add_executable(test_result_summary tests/test_result_summary.cpp)
    target_link_libraries(test_result_summary PRIVATE p2dic_core)
    add_test(NAME result_summary COMMAND test_result_summary)

    add_executable(test_result_packet tests/test_result_packet.cpp)
    target_link_libraries(test_result_packet PRIVATE p2dic_core)
    add_test(NAME result_packet COMMAND test_result_packet)

    add_executable(test_result_stream tests/test_result_stream.cpp)
    target_link_libraries(test_result_stream PRIVATE p2dic_core)
    add_test(NAME result_stream_roundtrip COMMAND test_result_stream)
    set_tests_properties(result_stream_roundtrip PROPERTIES TIMEOUT 5)

    add_executable(test_control_protocol tests/test_control_protocol.cpp)
    target_link_libraries(test_control_protocol PRIVATE p2dic_core)
    add_test(NAME control_protocol COMMAND test_control_protocol)

    add_executable(test_tcp_control tests/test_tcp_control.cpp)
    target_link_libraries(test_tcp_control PRIVATE p2dic_core)
    add_test(NAME tcp_control_roundtrip COMMAND test_tcp_control)

    add_test(
        NAME dic_edge_self_test
        COMMAND dic_edge --self-test "${CMAKE_CURRENT_BINARY_DIR}/edge-self-test-data" 39848)
    set_tests_properties(dic_edge_self_test PROPERTIES
        TIMEOUT 20
        FIXTURES_SETUP edge_self_test)

    add_executable(test_edge_config tests/test_edge_config.cpp)
    target_link_libraries(test_edge_config PRIVATE p2dic_core)
    add_test(NAME edge_config COMMAND test_edge_config)

    add_executable(test_deployment_files tests/test_deployment_files.cpp)
    target_compile_definitions(test_deployment_files PRIVATE
        P2DIC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME deployment_files COMMAND test_deployment_files)

    if(P2DIC_WITH_CUDA)
        add_executable(test_cuda_grid tests/test_cuda_grid.cpp)
        target_link_libraries(test_cuda_grid PRIVATE p2dic_core)
    add_test(NAME cuda_grid_accuracy COMMAND test_cuda_grid)
    add_test(NAME cuda_grid_ic_accuracy COMMAND test_cuda_grid ic)
        set_tests_properties(cuda_grid_accuracy PROPERTIES TIMEOUT 30)
    endif()
endif()
