// -*-c++-*-

/*!
  \file field_analyzer.h
  \brief miscellaneous field analysis class Header File
*/

/*
 *Copyright:

  Copyright (C) Hidehisa AKIYAMA

  This code is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3, or (at your option)
  any later version.

  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this code; see the file COPYING.  If not, write to
  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef FIELD_ANALYZER_H
#define FIELD_ANALYZER_H

#include "predict_state.h"

#include <rcsc/geom/voronoi_diagram.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/player_object.h>
#include <cmath>

namespace rcsc {
class AbstractPlayerObject;
class PlayerType;
class ServerParam;
class StaminaModel;
class WorldModel;
}

class PassChecker;

class FieldAnalyzer {
private:

    rcsc::VoronoiDiagram M_all_players_voronoi_diagram;
    rcsc::VoronoiDiagram M_teammates_voronoi_diagram;
    rcsc::VoronoiDiagram M_pass_voronoi_diagram;

    FieldAnalyzer();
public:

    static
    FieldAnalyzer & instance();

    static
    const FieldAnalyzer & i()
      {
          return instance();
      }

    const rcsc::VoronoiDiagram & allPlayersVoronoiDiagram() const
      {
          return M_all_players_voronoi_diagram;
      }

    const rcsc::VoronoiDiagram & teammatesVoronoiDiagram() const
      {
          return M_teammates_voronoi_diagram;
      }

    const rcsc::VoronoiDiagram & passVoronoiDiagram() const
      {
          return M_pass_voronoi_diagram;
      }

    void update( const rcsc::WorldModel & wm );


private:

    void updateVoronoiDiagram( const rcsc::WorldModel & wm );

    void writeDebugLog();

private:
    /*!
      \brief get cross point of ball and field side line or goal line
      \param ball_from curent ball point
      \param ball_to feature ball point
      \param p1 point on line
      \param p2 point on line
      \param field_back_offset distance from line
     */
    static
    rcsc::Vector2D get_ball_field_line_cross_point( const rcsc::Vector2D & ball_from,
                                                    const rcsc::Vector2D & ball_to,
                                                    const rcsc::Vector2D & p1,
                                                    const rcsc::Vector2D & p2,
                                                    const double field_back_offset );

public:
    static
    int estimate_min_reach_cycle( const rcsc::Vector2D & player_pos,
                                  const double & player_speed_max,
                                  const rcsc::Vector2D & target_point,
                                  const rcsc::AngleDeg & target_move_angle );

    static
    double estimate_virtual_dash_distance( const rcsc::AbstractPlayerObject * player );

    static
    int predict_player_turn_cycle( const rcsc::PlayerType * player_type,
                                   const rcsc::AngleDeg & player_body,
                                   const double & player_speed,
                                   const double & target_dist,
                                   const rcsc::AngleDeg & target_angle,
                                   const double & dist_thr,
                                   const bool use_back_dash );

    static
    int predict_self_reach_cycle( const rcsc::WorldModel & wm,
                                  const rcsc::Vector2D & target_point,
                                  const double & dist_thr,
                                  const int wait_cycle,
                                  const bool save_recovery,
                                  rcsc::StaminaModel * stamina );

    /*!
      \param dist_thr kickable_area or catchable_area
      \param penalty_distance result of estimate_virtual_dash_distance()
      \param wait_cycle penalty of kick, tackle or observation delay.
     */
    static
    int predict_player_reach_cycle( const rcsc::AbstractPlayerObject * player,
                                    const rcsc::Vector2D & target_point,
                                    const double & dist_thr,
                                    const double & penalty_distance,
                                    const int body_count_thr,
                                    const int default_n_turn,
                                    const int wait_cycle,
                                    const bool use_back_dash );
    static
    int predict_kick_count( const rcsc::WorldModel & wm,
                            const rcsc::AbstractPlayerObject * kicker,
                            const double & first_ball_speed,
                            const rcsc::AngleDeg & ball_move_angle );

    /*!
      \brief get predict ball position, if ball shall be out of field,
      returns cross point with side line or goal line. if specifys
      field_back_offset, returns back of cross point.
      \param wm world model
      \param predict_step max time for prediction
      \param field_back_offset distance from line
      \return position of ball
     */
    static
    rcsc::Vector2D get_field_bound_predict_ball_pos( const rcsc::WorldModel & wm,
                                                     const int predict_step,
                                                     const double field_back_offset );

    static
    bool can_shoot_from( const bool is_self,
                         const rcsc::Vector2D & pos,
                         const rcsc::AbstractPlayerCont & opponents,
                         const int valid_opponent_threshold );

    static
    bool opponent_can_shoot_from( const rcsc::Vector2D & pos,
                                  const rcsc::AbstractPlayerCont & teammates,
                                  const int valid_teammate_threshold,
                                  const double shoot_dist_threshold = -1.0,
                                  const double shoot_angle_threshold = -1.0,
                                  const double teammate_dist_threshold = -1.0,
                                  double * max_angle_diff_result = static_cast< double * >( 0 ),
                                  const bool calculate_detail = false );


    static
    double get_dist_player_nearest_to_point( const rcsc::Vector2D & point,
                                             const rcsc::PlayerCont & cont,
                                             const int count_thr = -1 );

    static
    rcsc::Vector2D get_our_team_near_goal_post_pos( const rcsc::Vector2D & point );

    static
    rcsc::Vector2D get_our_team_far_goal_post_pos( const rcsc::Vector2D & point );

    static
    rcsc::Vector2D get_opponent_team_near_goal_post_pos( const rcsc::Vector2D & point );

    static
    rcsc::Vector2D get_opponent_team_far_goal_post_pos( const rcsc::Vector2D & point );

    static
    double get_dist_from_our_near_goal_post( const rcsc::Vector2D & point );

    static
    double get_dist_from_opponent_near_goal_post( const rcsc::Vector2D & point );

    static
    bool is_ball_moving_to_our_goal( const rcsc::Vector2D & ball_pos,
                                     const rcsc::Vector2D & ball_vel,
                                     const double & post_buffer );

    static
    int get_pass_count( const PredictState & state,
                        const PassChecker & pass_checker,
                        const double first_ball_speed,
                        int max_count = -1 );

    static
    const rcsc::AbstractPlayerObject * get_blocker( const rcsc::WorldModel & wm,
                                                    const rcsc::Vector2D & opponent_pos );

    static
    const rcsc::AbstractPlayerObject * get_blocker( const rcsc::WorldModel & wm,
                                                    const rcsc::Vector2D & opponent_pos,
                                                    const rcsc::Vector2D & base_pos );

public:
    static
    bool to_be_final_action( const PredictState & state );

    static
    bool to_be_final_action( const rcsc::WorldModel & wm );

private:
    static
    bool to_be_final_action( const rcsc::Vector2D & ball_pos,
                             const double their_defense_player_line_x );
};


#include <rcsc/player/abstract_player_object.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>


/*-------------------------------------------------------------------*/
/*!

 */
inline
int
FieldAnalyzer::estimate_min_reach_cycle( const rcsc::Vector2D & player_pos,
                                         const double & player_speed_max,
                                         const rcsc::Vector2D & target_first_point,
                                         const rcsc::AngleDeg & target_move_angle )
{
    rcsc::Vector2D target_to_player = ( player_pos - target_first_point ).rotatedVector( -target_move_angle );
    return ( target_to_player.x < -1.0
             ? -1
             : std::max( 1, static_cast< int >( std::floor( target_to_player.absY() / player_speed_max ) ) ) );
}

#endif
