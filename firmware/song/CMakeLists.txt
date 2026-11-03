# SPDX-License-Identifier: GPL-3.0-only
# Step 2a: song model, MKS2 file format, sequencer, mora table, demo song. Self-contained (no libc on device).
add_library(miku_song STATIC song.c song_io.c seq.c mora.c demo.c)
target_include_directories(miku_song PUBLIC ${CMAKE_SOURCE_DIR})
if(NOT MSVC)
  target_compile_options(miku_song PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(miku_song_tests tests/test_song.c)
target_link_libraries(miku_song_tests PRIVATE miku_song m)
if(NOT MSVC)
  target_compile_options(miku_song_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME song2 COMMAND miku_song_tests)
