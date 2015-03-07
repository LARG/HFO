cmake_minimum_required(VERSION 2.8.3)
find_package(Threads REQUIRED)
project(hfo)

# Change these to reflect the location of your librcsc-4.1.0 install
set(LIBRCSC_INCLUDE ~/.local/include)
set(LIBRCSC_LINK ~/.local/lib)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SOURCE_DIR}
  ${SOURCE_DIR}/chain_action
  ${LIBRCSC_INCLUDE}
)

link_directories(
  ${LIBRCSC_LINK}
)

file(GLOB SOURCES ${SOURCE_DIR}/*.cpp ${SOURCE_DIR}/chain_action/*.cpp)
list(REMOVE_ITEM SOURCES ${SOURCE_DIR}/main_coach.cpp ${SOURCE_DIR}/main_player.cpp ${SOURCE_DIR}/main_trainer.cpp ${SOURCE_DIR}/main_agent.cpp ${SOURCE_DIR}/sample_coach.cpp ${SOURCE_DIR}/sample_player.cpp ${SOURCE_DIR}/sample_trainer.cpp ${SOURCE_DIR}/agent.cpp ${SOURCE_DIR}/HFO.cpp)

list(APPEND LINK_LIBS
  rcsc_agent
  rcsc_geom
  rcsc_param
  rcsc_ann
  rcsc_net
  rcsc_gz
  rcsc_time
  rcsc_rcg
  ${CMAKE_THREAD_LIBS_INIT}
)

add_executable(sample_coach ${SOURCE_DIR}/main_coach.cpp ${SOURCE_DIR}/sample_coach.cpp ${SOURCES})
add_executable(sample_player ${SOURCE_DIR}/main_player.cpp ${SOURCE_DIR}/sample_player.cpp ${SOURCES})
add_executable(sample_trainer ${SOURCE_DIR}/main_trainer.cpp ${SOURCE_DIR}/sample_trainer.cpp ${SOURCES})
add_executable(agent ${SOURCE_DIR}/main_agent.cpp ${SOURCE_DIR}/agent.cpp ${SOURCES})
add_library(hfo-lib SHARED ${CMAKE_CURRENT_SOURCE_DIR}/include/HFO.hpp ${SOURCE_DIR}/HFO.cpp)
set_target_properties(hfo-lib PROPERTIES OUTPUT_NAME hfo)
set_target_properties(hfo-lib PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

target_link_libraries(sample_coach ${LINK_LIBS})
target_link_libraries(sample_player ${LINK_LIBS})
target_link_libraries(sample_trainer ${LINK_LIBS})
target_link_libraries(agent ${LINK_LIBS})


link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_executable(hfo_example_agent ${CMAKE_CURRENT_SOURCE_DIR}/example/hfo_example_agent.cpp)
set_target_properties(hfo_example_agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/example)
target_link_libraries(hfo_example_agent hfo)
add_dependencies(hfo_example_agent hfo-lib)
