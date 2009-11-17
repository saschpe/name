cmake_minimum_required(VERSION 2.6)
project(name)

find_package(GTK 2.0 COMPONENTS glib REQUIRED)

include_directories(${GTK_INCLUDE_DIR})

set(name_SRCS
    src/main.c
)

set(name_OBJS
    obj/clock.o
)

add_executable(name ${name_SRCS} ${name_OBJS})
target_link_libraries(name ${GTK_LIBRARIES})
