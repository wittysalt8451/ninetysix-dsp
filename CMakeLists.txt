cmake_minimum_required(VERSION 3.14)
project(sudwalfulkaan-dsp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Host unit tests (doctest); Daisy firmware builds use their own Makefile.
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
  enable_testing()

  add_executable(sudwalfulkaan_tests
    tests/test_utils.cpp
    tests/test_spectral.cpp
    utils/Mapping.cpp
    utils/Tempo.cpp
    utils/Slew.cpp
    utils/EnvelopeFollower.cpp
    utils/Fft.cpp
    utils/SpscRingBuffer.cpp
    envelopes/Envelope.cpp
    effects/spectral/SpectralStretch.cpp
  )

  target_include_directories(sudwalfulkaan_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  )

  target_compile_definitions(sudwalfulkaan_tests PRIVATE DOCTEST_CONFIG_SUPER_FAST_ASSERTS)

  add_test(NAME sudwalfulkaan_tests COMMAND sudwalfulkaan_tests)
endif()
