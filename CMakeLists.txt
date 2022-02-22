cmake_minimum_required(VERSION 3.10)
project(threadpool-benchmark)

include(ExternalProject)

ExternalProject_Add(GoogleBenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.6.1
  CMAKE_ARGS -DBENCHMARK_DOWNLOAD_DEPENDENCIES=ON
)
## ExternalProject_Get_Property(GoogleBenchmark BINARY_DIR)
## ExternalProject_Get_Property(GoogleBenchmark SOURCE_DIR)
set(GoogleBenchmarkPrefix ${CMAKE_CURRENT_BINARY_DIR}/GoogleBenchmark-prefix/src)
set(GoogleBenchmarkIncludeDir ${GoogleBenchmarkPrefix}/GoogleBenchmark/include)
set(GoogleBenchmarkLibDir ${GoogleBenchmarkPrefix}/GoogleBenchmark-build)

include_directories(${GoogleBenchmarkIncludeDir})


add_executable(bench threadpool.cc)
#add_dependencies(bench GoogleBenchmark)
target_link_libraries(bench
  PRIVATE ${GoogleBenchmarkLibDir}/src/libbenchmark.a
  PRIVATE ${GoogleBenchmarkLibDir}/src/libbenchmark_main.a
  #  PRIVATE ${GoogleBenchmarkLibDir}/lib/libgmock.a
  #  PRIVATE ${GoogleBenchmarkLibDir}/lib/libgmock_main.a
  #  PRIVATE ${GoogleBenchmarkLibDir}/lib/libgtest.a
)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
  target_link_libraries(bench PRIVATE OpenMP::OpenMP_CXX)
endif()
