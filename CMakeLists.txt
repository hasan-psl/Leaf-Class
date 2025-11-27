cmake_minimum_required(VERSION 3.20)

project(LeafClass C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)

add_executable(LeafClass src/main.c)

target_include_directories(LeafClass PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS})
target_link_libraries(LeafClass PRIVATE ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES})
target_compile_options(LeafClass PRIVATE ${GTK3_CFLAGS_OTHER} ${WEBKIT_CFLAGS_OTHER})
