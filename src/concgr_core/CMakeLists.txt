cmake_minimum_required(VERSION 3.10)
project(concgr_core)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -O3)
endif()

find_package(ament_cmake REQUIRED)

include_directories(include)

add_library(concgr_core
  src/aci_calibrator.cpp
  src/conformal_dijkstra.cpp
  src/knife_edge_diffraction.cpp
  src/epidemic_router.cpp
  src/spray_and_wait_router.cpp
  src/prophet_router.cpp
  src/deterministic_cgr_router.cpp
  src/connected_fanet_router.cpp
  src/concgr_aoi_router.cpp
  src/swarm_simulator.cpp
)

target_include_directories(concgr_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Test executables
add_executable(test_aci_calibrator tests/test_aci_calibrator.cpp)
target_link_libraries(test_aci_calibrator concgr_core)

add_executable(test_conformal_dijkstra tests/test_conformal_dijkstra.cpp)
target_link_libraries(test_conformal_dijkstra concgr_core)

add_executable(test_diffraction tests/test_diffraction.cpp)
target_link_libraries(test_diffraction concgr_core)

add_executable(test_buffer_eviction tests/test_buffer_eviction.cpp)
target_link_libraries(test_buffer_eviction concgr_core)

add_executable(test_comparative_routers tests/test_comparative_routers.cpp)
target_link_libraries(test_comparative_routers concgr_core)

add_executable(run_real_benchmark tests/run_real_benchmark.cpp)
target_link_libraries(run_real_benchmark concgr_core)

# Install targets
install(TARGETS concgr_core run_real_benchmark
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include
)

ament_export_include_directories(include)
ament_export_libraries(concgr_core)
ament_package()
