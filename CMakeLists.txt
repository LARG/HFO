cmake_minimum_required(VERSION 2.8.12)
project(hfo)
include(ExternalProject)

option(BUILD_SOCCERWINDOW "Build Soccerwindow Visualizer" ON)
option(INSTALL_ADDITIONAL_TEAMS "Install additional team binaries" ON)

add_definitions(-DELOG)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC")

find_package(Boost COMPONENTS system filesystem REQUIRED)
find_package(ZLIB REQUIRED)
find_package(FLEX REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

ExternalProject_Add(rcssserver
  GIT_REPOSITORY "https://github.com/mhauskn/rcssserver.git"
  GIT_TAG "master"
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
  UPDATE_COMMAND ""
  INSTALL_COMMAND "")
ExternalProject_Get_Property(rcssserver SOURCE_DIR)
include_directories(${SOURCE_DIR}/rcssbase ${SOURCE_DIR}/src)
ExternalProject_Get_Property(rcssserver BINARY_DIR)
set(RCSSSERVER_BINARY_DIR ${BINARY_DIR}/bin)

ExternalProject_Add(librcsc
  GIT_REPOSITORY "https://github.com/mhauskn/librcsc.git"
  GIT_TAG "master"
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
  UPDATE_COMMAND ""
  INSTALL_COMMAND "")
ExternalProject_Get_Property(librcsc SOURCE_DIR)
include_directories(${SOURCE_DIR})
set(LIBRCSC_INCLUDE_DIR ${SOURCE_DIR})
ExternalProject_Get_Property(librcsc BINARY_DIR)
link_directories(${BINARY_DIR}/lib)
set(LIBRCSC_LINK_DIR ${BINARY_DIR}/lib)

if(BUILD_SOCCERWINDOW)
  find_package(Qt4 REQUIRED COMPONENTS QTCORE QTGUI QTNETWORK)
  if(NOT Qt4_FOUND)
    message(FATAL_ERROR "Qt4 is needed for soccerwindow2 visualizer. Install Qt4 or disable soccerwindow2: \"cmake -DBUILD_SOCCERWINDOW=False\"")
  endif()
  ExternalProject_Add(soccerwindow2
    DEPENDS librcsc
    GIT_REPOSITORY "https://github.com/mhauskn/soccerwindow2.git"
    GIT_TAG "master"
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DLIBRCSC_INCLUDE_DIR=${LIBRCSC_INCLUDE_DIR} -DLIBRCSC_LINK_DIR=${LIBRCSC_LINK_DIR}
    UPDATE_COMMAND ""
    INSTALL_COMMAND "")
  ExternalProject_Get_Property(soccerwindow2 BINARY_DIR)
  set(SOCCERWINDOW2_BINARY_DIR ${BINARY_DIR}/bin)
  install(DIRECTORY ${SOCCERWINDOW2_BINARY_DIR} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR} USE_SOURCE_PERMISSIONS)
endif()

if(INSTALL_ADDITIONAL_TEAMS)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    ExternalProject_Add(hfo-teams
      GIT_REPOSITORY "https://github.com/mhauskn/hfo-teams.git"
      GIT_TAG "master"
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      UPDATE_COMMAND ""
      INSTALL_COMMAND "")
    ExternalProject_Get_Property(hfo-teams SOURCE_DIR)
    set(TEAMS_SOURCE_DIR ${SOURCE_DIR}/teams)
    install(DIRECTORY ${TEAMS_SOURCE_DIR} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin USE_SOURCE_PERMISSIONS)
  else()
    MESSAGE("Additional team binaries only supported in Linux. Skipping Install.")
  endif()
endif()

if(APPLE)
  set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
endif()

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
include_directories(${SOURCE_DIR} ${SOURCE_DIR}/chain_action)

set(PLAYER_CPP bhv_basic_move.cpp bhv_basic_offensive_kick.cpp
  bhv_basic_tackle.cpp bhv_custom_before_kick_off.cpp
  bhv_force_pass.cpp bhv_go_to_static_ball.cpp
  bhv_goalie_basic_move.cpp bhv_goalie_chase_ball.cpp
  bhv_goalie_free_kick.cpp bhv_prepare_set_play_kick.cpp
  bhv_set_play.cpp bhv_set_play_free_kick.cpp
  bhv_set_play_goal_kick.cpp bhv_set_play_indirect_free_kick.cpp
  bhv_set_play_kick_in.cpp bhv_set_play_kick_off.cpp
  bhv_their_goal_kick_move.cpp bhv_penalty_kick.cpp
  feature_extractor.cpp lowlevel_feature_extractor.cpp
  highlevel_feature_extractor.cpp neck_default_intercept_neck.cpp
  neck_goalie_turn_neck.cpp neck_offensive_intercept_neck.cpp
  view_tactical.cpp intention_receive.cpp
  intention_wait_after_set_play_kick.cpp soccer_role.cpp
  role_center_back.cpp role_center_forward.cpp role_defensive_half.cpp
  role_goalie.cpp role_offensive_half.cpp role_sample.cpp
  role_side_back.cpp role_side_forward.cpp role_side_half.cpp
  role_keepaway_keeper.cpp role_keepaway_taker.cpp
  sample_communication.cpp keepaway_communication.cpp
  sample_field_evaluator.cpp sample_player.cpp strategy.cpp)

foreach(src ${PLAYER_CPP})
  list(APPEND PLAYER_SOURCES ${SOURCE_DIR}/${src})
endforeach(src ${PLAYER_CPP})
file(GLOB CHAIN_ACTION_SOURCES ${SOURCE_DIR}/chain_action/*.cpp)

list(APPEND RCSC_LINK_LIBS rcsc_agent rcsc_geom rcsc_param rcsc_ann rcsc_net rcsc_gz rcsc_time rcsc_rcg z)

add_library(player_chain_action STATIC ${PLAYER_SOURCES} ${CHAIN_ACTION_SOURCES})
add_executable(sample_coach ${SOURCE_DIR}/main_coach.cpp ${SOURCE_DIR}/sample_coach.cpp)
add_executable(sample_player ${SOURCE_DIR}/HFO.cpp ${SOURCE_DIR}/main_player.cpp ${SOURCE_DIR}/sample_player.cpp ${SOURCE_DIR}/agent.cpp) 
add_executable(sample_trainer ${SOURCE_DIR}/main_trainer.cpp ${SOURCE_DIR}/sample_trainer.cpp)
add_executable(agent ${SOURCE_DIR}/HFO.cpp ${SOURCE_DIR}/main_agent.cpp ${SOURCE_DIR}/agent.cpp)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/teams/base)
set_target_properties(sample_coach PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/teams/base)
set_target_properties(sample_player PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/teams/base)
set_target_properties(sample_trainer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/teams/base)
set_target_properties(agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/teams/base)

add_library(hfo-lib SHARED ${SOURCE_DIR}/HFO.hpp ${SOURCE_DIR}/HFO.cpp ${SOURCE_DIR}/agent.cpp)
set_target_properties(hfo-lib PROPERTIES OUTPUT_NAME hfo)
set_target_properties(hfo-lib PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_library(hfo-c-lib SHARED ${CMAKE_CURRENT_SOURCE_DIR}/hfo/hfo_c_wrapper.cpp ${SOURCE_DIR}/HFO.cpp ${SOURCE_DIR}/agent.cpp)
set_target_properties(hfo-c-lib PROPERTIES OUTPUT_NAME hfo_c)
set_target_properties(hfo-c-lib PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/hfo)

add_dependencies(player_chain_action librcsc)
add_dependencies(sample_coach librcsc)
add_dependencies(sample_player librcsc)
add_dependencies(sample_trainer librcsc)
add_dependencies(agent librcsc player_chain_action)
add_dependencies(hfo-lib player_chain_action)
add_dependencies(hfo-c-lib player_chain_action)

target_link_libraries(player_chain_action ${RCSC_LINK_LIBS})
target_link_libraries(sample_coach ${RCSC_LINK_LIBS})
target_link_libraries(sample_player ${RCSC_LINK_LIBS} player_chain_action)
target_link_libraries(sample_trainer ${RCSC_LINK_LIBS})
target_link_libraries(agent player_chain_action ${RCSC_LINK_LIBS})
target_link_libraries(hfo-lib ${RCSC_LINK_LIBS} player_chain_action)
target_link_libraries(hfo-c-lib ${RCSC_LINK_LIBS} player_chain_action)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_executable(hfo_example_agent ${CMAKE_CURRENT_SOURCE_DIR}/example/hfo_example_agent.cpp)
set_target_properties(hfo_example_agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example)
target_link_libraries(hfo_example_agent hfo-lib)
add_executable(low_level_random_agent ${CMAKE_CURRENT_SOURCE_DIR}/example/low_level_random_agent.cpp)
set_target_properties(low_level_random_agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example)
target_link_libraries(low_level_random_agent hfo-lib)
add_executable(high_level_random_agent ${CMAKE_CURRENT_SOURCE_DIR}/example/high_level_random_agent.cpp)
set_target_properties(high_level_random_agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example)
target_link_libraries(high_level_random_agent hfo-lib)

add_executable(mid_level_move_agent ${CMAKE_CURRENT_SOURCE_DIR}/example/mid_level_move_agent.cpp)
set_target_properties(mid_level_move_agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example)
target_link_libraries(mid_level_move_agent hfo-lib)

add_executable(mid_level_kick_agent ${CMAKE_CURRENT_SOURCE_DIR}/example/mid_level_kick_agent.cpp)
set_target_properties(mid_level_kick_agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example)
target_link_libraries(mid_level_kick_agent hfo-lib)

add_executable(mid_level_dribble_agent ${CMAKE_CURRENT_SOURCE_DIR}/example/mid_level_dribble_agent.cpp)
set_target_properties(mid_level_dribble_agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example)
target_link_libraries(mid_level_dribble_agent hfo-lib)

add_executable(communication_agent ${CMAKE_CURRENT_SOURCE_DIR}/example/communication_agent.cpp)
set_target_properties(communication_agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example)
target_link_libraries(communication_agent hfo-lib)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example DESTINATION ${CMAKE_CURRENT_SOURCE_DIR} USE_SOURCE_PERMISSIONS)
install(DIRECTORY ${RCSSSERVER_BINARY_DIR} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR} USE_SOURCE_PERMISSIONS)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/teams DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin USE_SOURCE_PERMISSIONS)
