cmake_minimum_required(VERSION 2.6)
project(name)

set(name_OBJS
    obj/clock.o
)
set(name_SRCS
    src/name.cpp
    src/main.cpp
)

add_executable(name ${name_OBJS} ${name_SRCS})
