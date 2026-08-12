cmake_minimum_required(VERSION 3.16)
project(LogosViewGenerator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt Core only. This tool reads metadata.json and scrapes a class name out of a
# .rep; it never parses LIDL, so it links no generator frontend and this repo
# stays a leaf (see the note in main.cpp).
find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(logos-view-generator
    main.cpp
    lidl_gen_ui.cpp
)

target_link_libraries(logos-view-generator PRIVATE Qt6::Core)

install(TARGETS logos-view-generator RUNTIME DESTINATION bin)
