cmake_minimum_required(VERSION 3.24)
project(urkern)

add_library(urkern STATIC
  src/uuid.cc
  src/thread_pool.cc
  src/platform.cc
)
target_include_directories(urkern PUBLIC include)
target_compile_features(urkern PUBLIC cxx_std_23)
