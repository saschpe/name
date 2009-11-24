cmake_minimum_required(VERSION 2.6)
project(name)

find_package(GTK 2.0 COMPONENTS glib REQUIRED)

include_directories(${GTK_INCLUDE_DIR})

set(name_OBJS
    obj/clock.o
)
set(name_SRCS
    src/hash.c
    src/main.c
    src/name.c
)

add_executable(name ${name_OBJS} ${name_SRCS})
target_link_libraries(name ${GTK_LIBRARIES})
