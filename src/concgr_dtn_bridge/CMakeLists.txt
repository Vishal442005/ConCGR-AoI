cmake_minimum_required(VERSION 3.10)
project(concgr_dtn_bridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -O3)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(concgr_msgs REQUIRED)
find_package(concgr_core REQUIRED)
find_package(std_msgs REQUIRED)

include_directories(
  include
)

# 1. Bridge Node
add_executable(concgr_bridge_node src/bridge_node.cpp)
ament_target_dependencies(concgr_bridge_node
  rclcpp
  concgr_msgs
  concgr_core
  std_msgs
)
target_link_libraries(concgr_bridge_node /home/jarvis/concgr_dtn_ws/install/concgr_core/lib/libconcgr_core.a)

# 2. Sensor Publisher Node (Authentic Payloads)
add_executable(concgr_sensor_publisher src/sensor_publisher_node.cpp)
ament_target_dependencies(concgr_sensor_publisher
  rclcpp
  concgr_msgs
  std_msgs
)

# 3. Ground Station Sink Node
add_executable(concgr_sink_node src/sink_node.cpp)
ament_target_dependencies(concgr_sink_node
  rclcpp
  concgr_msgs
  std_msgs
)

install(TARGETS
  concgr_bridge_node
  concgr_sensor_publisher
  concgr_sink_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include/
)

ament_package()
