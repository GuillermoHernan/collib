# CMakeLists.txt en external/catch2

add_library(Catch2 STATIC catch_amalgamated.cpp)
target_include_directories(Catch2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Opcional: define el alias moderno para facilitar el uso en el resto del proyecto
add_library(Catch2::Catch2 ALIAS Catch2)
