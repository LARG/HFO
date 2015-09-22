cmake_minimum_required(VERSION 2.8.3)
project(hfo)
include(ExternalProject)

option(BUILD_SOCCERWINDOW "Build Soccerwindow Visualizer" ON)

add_definitions(-DELOG)

ExternalProject_Add(rcssserver
  GIT_REPOSITORY "git@github.com:mhauskn/rcssserver.git"
  GIT_TAG "master"
  UPDATE_COMMAND ""
  INSTALL_COMMAND "")
ExternalProject_Get_Property(rcssserver SOURCE_DIR)
include_directories(${SOURCE_DIR}/rcssbase ${SOURCE_DIR}/src)
ExternalProject_Get_Property(rcssserver BINARY_DIR)
link_directories(${BINARY_DIR}/lib)
set(RCSSSERVER_BINARY_DIR ${BINARY_DIR}/bin)

ExternalProject_Add(librcsc
  GIT_REPOSITORY "git@github.com:mhauskn/librcsc.git"
  GIT_TAG "master"
  UPDATE_COMMAND ""
  INSTALL_COMMAND "")
ExternalProject_Get_Property(librcsc SOURCE_DIR)
include_directories(${SOURCE_DIR})
set(LIBRCSC_INCLUDE_DIR ${SOURCE_DIR})
message(STATUS "LIBRCSC_INCLUDE_DIR: ${LIBRCSC_INCLUDE_DIR}")
ExternalProject_Get_Property(librcsc BINARY_DIR)
link_directories(${BINARY_DIR}/lib)
set(LIBRCSC_LINK_DIR ${BINARY_DIR}/lib)
message(STATUS "LIBRCSC_LINK_DIR: ${LIBRCSC_LINK_DIR}")

if(BUILD_SOCCERWINDOW)
  ExternalProject_Add(soccerwindow2
    DEPENDS librcsc
    GIT_REPOSITORY "git@github.com:mhauskn/soccerwindow2.git"
    GIT_TAG "master"
    CMAKE_ARGS -DLIBRCSC_INCLUDE_DIR=${LIBRCSC_INCLUDE_DIR} -DLIBRCSC_LINK_DIR=${LIBRCSC_LINK_DIR}
    UPDATE_COMMAND ""
    INSTALL_COMMAND "")
  ExternalProject_Get_Property(soccerwindow2 BINARY_DIR)
  set(SOCCERWINDOW2_BINARY_DIR ${BINARY_DIR}/bin)
  install(DIRECTORY ${SOCCERWINDOW2_BINARY_DIR} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/ USE_SOURCE_PERMISSIONS)
endif()

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
include_directories(${SOURCE_DIR} ${SOURCE_DIR}/chain_action)

file(GLOB SOURCES ${SOURCE_DIR}/*.cpp ${SOURCE_DIR}/chain_action/*.cpp)
list(REMOVE_ITEM SOURCES ${SOURCE_DIR}/main_coach.cpp ${SOURCE_DIR}/main_player.cpp ${SOURCE_DIR}/main_trainer.cpp ${SOURCE_DIR}/main_agent.cpp ${SOURCE_DIR}/sample_coach.cpp ${SOURCE_DIR}/sample_player.cpp ${SOURCE_DIR}/sample_trainer.cpp ${SOURCE_DIR}/agent.cpp ${SOURCE_DIR}/HFO.cpp)

list(APPEND LINK_LIBS rcsc_agent rcsc_geom rcsc_param rcsc_ann
  rcsc_net rcsc_gz rcsc_time rcsc_rcg)

add_executable(sample_coach ${SOURCE_DIR}/main_coach.cpp ${SOURCE_DIR}/sample_coach.cpp ${SOURCES})
add_executable(sample_player ${SOURCE_DIR}/HFO.cpp ${SOURCE_DIR}/main_player.cpp ${SOURCE_DIR}/sample_player.cpp ${SOURCE_DIR}/agent.cpp ${SOURCES})
add_executable(sample_trainer ${SOURCE_DIR}/main_trainer.cpp ${SOURCE_DIR}/sample_trainer.cpp ${SOURCES})
add_executable(agent ${SOURCE_DIR}/main_agent.cpp ${SOURCE_DIR}/agent.cpp ${SOURCES})
add_library(hfo-lib SHARED ${SOURCE_DIR}/HFO.hpp ${SOURCE_DIR}/HFO.cpp)
set_target_properties(hfo-lib PROPERTIES OUTPUT_NAME hfo)
set_target_properties(hfo-lib PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_dependencies(sample_coach librcsc)
add_dependencies(sample_player librcsc)
add_dependencies(sample_trainer librcsc)
add_dependencies(agent librcsc)
add_dependencies(hfo-lib librcsc)

target_link_libraries(sample_coach ${LINK_LIBS})
target_link_libraries(sample_player ${LINK_LIBS})
target_link_libraries(sample_trainer ${LINK_LIBS})
target_link_libraries(agent ${LINK_LIBS})

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_executable(hfo_example_agent ${CMAKE_CURRENT_SOURCE_DIR}/example/hfo_example_agent.cpp)
set_target_properties(hfo_example_agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/example)
target_link_libraries(hfo_example_agent hfo)
add_dependencies(hfo_example_agent hfo-lib)

install(DIRECTORY ${RCSSSERVER_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/bin DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/ USE_SOURCE_PERMISSIONS)
