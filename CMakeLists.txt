cmake_minimum_required(VERSION 3.20)

project(
    PsSource
    VERSION 3.0.0
    LANGUAGES CXX
)

find_package(Geant4 REQUIRED)

add_library(
    PsSource
    ConfigurablePsModel.cc
    FixedParameterizedPsModel.cc
    OrePowellPsModel.cc
    PsTerminalStateBuilder.cc
    PsSourceAnnihilationProcess.cc
    PsSourceAnnihilationPhysics.cc
)

target_compile_features(
    PsSource
    PUBLIC
        cxx_std_17
)

target_include_directories(
    PsSource
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    PsSource
    PUBLIC
        ${Geant4_LIBRARIES}
)

target_include_directories(
    PsSource
    SYSTEM PUBLIC
        ${Geant4_INCLUDE_DIRS}
)

add_executable(
    standalone_transport_example
    standalone_transport_example.cc
)

target_link_libraries(
    standalone_transport_example
    PRIVATE
        PsSource
)
