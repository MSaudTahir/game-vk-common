cmake_minimum_required(VERSION 3.16)
project(game-vk-common LANGUAGES CXX)

find_package(Vulkan QUIET)

# Small C++ / Vulkan helpers shared by the cod4mw and nfsmw ports.  Everything here is new,
# game-independent code (MIT); nothing ported from either game's decompilation lives in this repo.

add_library(gvc STATIC src/BcDecode.cpp src/TouchUi.cpp)
add_library(gvc::gvc ALIAS gvc)
target_include_directories(gvc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(gvc PUBLIC cxx_std_17)

# The VkFormat half of BcDecode compiles whenever Vulkan headers are reachable (the game builds
# always have them); without them the enum-based API still builds and tests.
if (Vulkan_FOUND)
    target_link_libraries(gvc PUBLIC Vulkan::Headers)
endif()

# --- Tests (plain asserts, exit code = number of failures) ---
set(GVC_BUILD_TESTS OFF CACHE BOOL "Build the gvc test programs")
if (GVC_BUILD_TESTS)
    enable_testing()
    add_executable(gvc_bc_decode_test tests/bc_decode_test.cpp)
    target_link_libraries(gvc_bc_decode_test PRIVATE gvc)
    add_test(NAME bc_decode COMMAND gvc_bc_decode_test)
    add_executable(gvc_touch_ui_test tests/touch_ui_test.cpp)
    target_link_libraries(gvc_touch_ui_test PRIVATE gvc)
    add_test(NAME touch_ui COMMAND gvc_touch_ui_test)
endif()
