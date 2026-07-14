cmake_minimum_required(VERSION 3.20)

project(
    PsSource
    VERSION 3.0.0
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Geant4 REQUIRED)

set(
    PS_SOURCE_PUBLIC_HEADERS
    ConfigurablePsModel.hh
    FixedParameterizedPsModel.hh
    OrePowellPsModel.hh
    PsPhysicsModel.hh
    PsSourceAnnihilationConfig.hh
    PsSourceAnnihilationPhysics.hh
    PsSourceAnnihilationProcess.hh
    PsTerminalState.hh
    PsTerminalStateBuilder.hh
    TimedEventModel.hh
)

add_library(
    PsSource
    ConfigurablePsModel.cc
    FixedParameterizedPsModel.cc
    OrePowellPsModel.cc
    PsTerminalStateBuilder.cc
    PsSourceAnnihilationProcess.cc
    PsSourceAnnihilationPhysics.cc
)

add_library(
    PsSource::PsSource
    ALIAS
    PsSource
)

target_compile_features(
    PsSource
    PUBLIC
        cxx_std_17
)

target_include_directories(
    PsSource
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/PsSource>
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

set_target_properties(
    PsSource
    PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 3
        PUBLIC_HEADER "${PS_SOURCE_PUBLIC_HEADERS}"
        EXPORT_NAME PsSource
)

add_executable(
    standalone_transport_example
    standalone_transport_example.cc
)

target_link_libraries(
    standalone_transport_example
    PRIVATE
        PsSource::PsSource
)

install(
    TARGETS PsSource
    EXPORT PsSourceTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PsSource
)

install(
    EXPORT PsSourceTargets
    FILE PsSourceTargets.cmake
    NAMESPACE PsSource::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PsSource
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PsSourceConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/PsSourceConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PsSource
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/PsSourceConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/PsSourceConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/PsSourceConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PsSource
)
