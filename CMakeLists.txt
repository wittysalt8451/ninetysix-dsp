cmake_minimum_required(VERSION 3.14)
project(ninetysix-dsp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Host unit tests (doctest); Daisy firmware builds use their own Makefile.
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
  enable_testing()

  add_executable(ninetysix_tests
    tests/test_utils.cpp
    tests/test_spectral.cpp
    tests/test_reverb.cpp
    utils/Mapping.cpp
    utils/Tempo.cpp
    utils/Slew.cpp
    utils/EnvelopeFollower.cpp
    utils/Fft.cpp
    utils/SpscRingBuffer.cpp
    envelopes/Envelope.cpp
    effects/spectral/SpectralStretch.cpp
    effects/reverb/FDN4Reverb.cpp
  )

  target_include_directories(ninetysix_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  )

  target_compile_definitions(ninetysix_tests PRIVATE DOCTEST_CONFIG_SUPER_FAST_ASSERTS)

  add_test(NAME ninetysix_tests COMMAND ninetysix_tests)
endif()
