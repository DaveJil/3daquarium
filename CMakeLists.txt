cmake_minimum_required(VERSION 3.16)
project(AquariumGL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW (window + input)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.9
)
FetchContent_MakeAvailable(glfw)

# glm (math)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

add_executable(Aquarium src/main.cpp)
target_include_directories(Aquarium PRIVATE src)

# Link Apple's OpenGL framework (no loader needed)
target_link_libraries(Aquarium PRIVATE glfw glm::glm "-framework OpenGL")

# Copy shaders to the build folder after each build
add_custom_command(TARGET Aquarium POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/shaders
          ${CMAKE_CURRENT_BINARY_DIR}/shaders)
