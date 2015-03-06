cmake_minimum_required(VERSION 2.8.3)
find_package(Threads REQUIRED)
# find_package(XMLRPC REQUIRED c++)
project(hfo)

# Change these to reflect the location of your librcsc-4.1.0 install
set(LIBRCSC_INCLUDE ~/.local/include)
set(LIBRCSC_LINK ~/.local/lib)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

include_directories(
  ${SOURCE_DIR}
  ${SOURCE_DIR}/chain_action
  ${LIBRCSC_INCLUDE}
  # ${XMLRPC_INCLUDE_DIRS}
)

link_directories(
  ${LIBRCSC_LINK}
)

file(GLOB SOURCES ${SOURCE_DIR}/*.cpp ${SOURCE_DIR}/chain_action/*.cpp)
list(REMOVE_ITEM SOURCES ${SOURCE_DIR}/main_coach.cpp ${SOURCE_DIR}/main_player.cpp ${SOURCE_DIR}/main_trainer.cpp ${SOURCE_DIR}/main_agent.cpp ${SOURCE_DIR}/sample_coach.cpp ${SOURCE_DIR}/sample_player.cpp ${SOURCE_DIR}/sample_trainer.cpp ${SOURCE_DIR}/agent.cpp)

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

target_link_libraries(sample_coach ${LINK_LIBS})
target_link_libraries(sample_player ${LINK_LIBS})
target_link_libraries(sample_trainer ${LINK_LIBS})
target_link_libraries(agent ${LINK_LIBS})
