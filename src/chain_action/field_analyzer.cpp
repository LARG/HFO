// -*-c++-*-

/*!
  \file field_analyzer.cpp
  \brief miscellaneous field analysis Source File
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA, Hiroki SHIMORA

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "field_analyzer.h"
#include "predict_state.h"
#include "pass_checker.h"

#include <rcsc/action/kick_table.h>
#include <rcsc/player/world_model.h>
#include <rcsc/player/player_predicate.h>
#include <rcsc/player/world_model.h>
#include <rcsc/player/player_object.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>
#include <rcsc/math_util.h>

#include <algorithm>

//#define DEBUG_PRINT
// #define DEBUG_PREDICT_PLAYER_TURN_CYCLE
// #define DEBUG_CAN_SHOOT_FROM

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
FieldAnalyzer::FieldAnalyzer()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
FieldAnalyzer &
FieldAnalyzer::instance()
{
#ifdef __APPLE__
    static FieldAnalyzer s_instance;
#else
    static thread_local FieldAnalyzer s_instance;
#endif
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldAnalyzer::estimate_virtual_dash_distance( const rcsc::AbstractPlayerObject * player )
{
    const int pos_count = std::min( 10, // Magic Number
                                    std::min( player->seenPosCount(),
                                              player->posCount() ) );
    const double max_speed = player->playerTypePtr()->realSpeedMax() * 0.8; // Magic Number

    double d = 0.0;
    for ( int i = 1; i <= pos_count; ++i ) // start_value==1 to set the initial_value<1
    {
        //d += max_speed * std::exp( - (i*i) / 20.0 ); // Magic Number
        d += max_speed * std::exp( - (i*i) / 15.0 ); // Magic Number
    }

    return d;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
FieldAnalyzer::predict_player_turn_cycle( const rcsc::PlayerType * ptype,
                                          const rcsc::AngleDeg & player_body,
                                          const double & player_speed,
                                          const double & target_dist,
                                          const rcsc::AngleDeg & target_angle,
                                          const double & dist_thr,
                                          const bool use_back_dash )
{
    const ServerParam & SP = ServerParam::i();

    int n_turn = 0;

    double angle_diff = ( target_angle - player_body ).abs();

    if ( use_back_dash
         && target_dist < 5.0 // Magic Number
         && angle_diff > 90.0
         && SP.minDashPower() < -SP.maxDashPower() + 1.0 )
    {
        angle_diff = std::fabs( angle_diff - 180.0 );    // assume backward dash
    }

    double turn_margin = 180.0;
    if ( dist_thr < target_dist )
    {
        turn_margin = std::max( 15.0, // Magic Number
                                rcsc::AngleDeg::asin_deg( dist_thr / target_dist ) );
    }

    double speed = player_speed;
    while ( angle_diff > turn_margin )
    {
        angle_diff -= ptype->effectiveTurn( SP.maxMoment(), speed );
        speed *= ptype->playerDecay();
        ++n_turn;
    }

#ifdef DEBUG_PREDICT_PLAYER_TURN_CYCLE
    dlog.addText( Logger::ANALYZER,
                  "(predict_player_turn_cycle) angleDiff=%.3f turnMargin=%.3f nTurn=%d",
                  angle_diff);
#endif

    return n_turn;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
FieldAnalyzer::predict_self_reach_cycle( const WorldModel & wm,
                                         const Vector2D & target_point,
                                         const double & dist_thr,
                                         const int wait_cycle,
                                         const bool save_recovery,
                                         StaminaModel * stamina )
{
    if ( wm.self().inertiaPoint( wait_cycle ).dist2( target_point ) < std::pow( dist_thr, 2 ) )
    {
        return 0;
    }

    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();
    const double recover_dec_thr = SP.recoverDecThrValue();

    const double first_speed = wm.self().vel().r() * std::pow( ptype.playerDecay(), wait_cycle );

    StaminaModel first_stamina_model = wm.self().staminaModel();
    if ( wait_cycle > 0 )
    {
        first_stamina_model.simulateWaits( ptype, wait_cycle );
    }

    for ( int cycle = std::max( 0, wait_cycle ); cycle < 30; ++cycle )
    {
        const Vector2D inertia_pos = wm.self().inertiaPoint( cycle );
        const double target_dist = inertia_pos.dist( target_point );

        if ( target_dist < dist_thr )
        {
            return cycle;
        }

        double dash_dist = target_dist - dist_thr * 0.5;

        if ( dash_dist > ptype.realSpeedMax() * ( cycle - wait_cycle ) )
        {
            continue;
        }

        AngleDeg target_angle = ( target_point - inertia_pos ).th();

        //
        // turn
        //

        int n_turn = predict_player_turn_cycle( &ptype,
                                                wm.self().body(),
                                                first_speed,
                                                target_dist,
                                                target_angle,
                                                dist_thr,
                                                false );
        if ( wait_cycle + n_turn >= cycle )
        {
            continue;
        }

        StaminaModel stamina_model = first_stamina_model;
        if ( n_turn > 0 )
        {
            stamina_model.simulateWaits( ptype, n_turn );
        }

        //
        // dash
        //

        int n_dash = ptype.cyclesToReachDistance( dash_dist );
        if ( wait_cycle + n_turn + n_dash > cycle )
        {
            continue;
        }

        double speed = first_speed * std::pow( ptype.playerDecay(), n_turn );
        double reach_dist = 0.0;

        n_dash = 0;
        while ( wait_cycle + n_turn + n_dash < cycle )
        {
            double dash_power = std::min( SP.maxDashPower(), stamina_model.stamina() );
            if ( save_recovery
                 && stamina_model.stamina() - dash_power < recover_dec_thr )
            {
                dash_power = std::max( 0.0, stamina_model.stamina() - recover_dec_thr );
                if ( dash_power < 1.0 )
                {
                    break;
                }
            }

            double accel = dash_power * ptype.dashPowerRate() * stamina_model.effort();
            speed += accel;
            if ( speed > ptype.playerSpeedMax() )
            {
                speed = ptype.playerSpeedMax();
            }

            reach_dist += speed;
            speed *= ptype.playerDecay();

            stamina_model.simulateDash( ptype, dash_power );

            ++n_dash;

            if ( reach_dist >= dash_dist )
            {
                break;
            }
        }

        if ( reach_dist >= dash_dist )
        {
            if ( stamina )
            {
                *stamina = stamina_model;
            }
            return wait_cycle + n_turn + n_dash;
        }
    }

    return 1000;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
FieldAnalyzer::predict_player_reach_cycle( const AbstractPlayerObject * player,
                                           const Vector2D & target_point,
                                           const double & dist_thr,
                                           const double & penalty_distance,
                                           const int body_count_thr,
                                           const int default_n_turn,
                                           const int wait_cycle,
                                           const bool use_back_dash )
{
    const PlayerType * ptype = player->playerTypePtr();

    const Vector2D & first_player_pos = ( player->seenPosCount() <= player->posCount()
                                          ? player->seenPos()
                                          : player->pos() );
    const Vector2D & first_player_vel = ( player->seenVelCount() <= player->velCount()
                                          ? player->seenVel()
                                          : player->vel() );
    const double first_player_speed = first_player_vel.r() * std::pow( ptype->playerDecay(), wait_cycle );

    int final_reach_cycle = -1;
    {
        Vector2D inertia_pos = ptype->inertiaFinalPoint( first_player_pos, first_player_vel );
        double target_dist = inertia_pos.dist( target_point );

        int n_turn = ( player->bodyCount() > body_count_thr
                       ? default_n_turn
                       : predict_player_turn_cycle( ptype,
                                                    player->body(),
                                                    first_player_speed,
                                                    target_dist,
                                                    ( target_point - inertia_pos ).th(),
                                                    dist_thr,
                                                    use_back_dash ) );
        int n_dash = ptype->cyclesToReachDistance( target_dist + penalty_distance );

        final_reach_cycle = wait_cycle + n_turn + n_dash;
    }

    const int max_cycle = 6; // Magic Number

    if ( final_reach_cycle > max_cycle )
    {
        return final_reach_cycle;
    }

    for ( int cycle = std::max( 0, wait_cycle ); cycle <= max_cycle; ++cycle )
    {
        Vector2D inertia_pos = ptype->inertiaPoint( first_player_pos, first_player_vel, cycle );
        double target_dist = inertia_pos.dist( target_point ) + penalty_distance;

        if ( target_dist < dist_thr )
        {
            return cycle;
        }

        double dash_dist = target_dist - dist_thr * 0.5;

        if ( dash_dist < 0.001 )
        {
            return cycle;
        }

        if ( dash_dist > ptype->realSpeedMax() * ( cycle - wait_cycle ) )
        {
            continue;
        }

        int n_dash = ptype->cyclesToReachDistance( dash_dist );

        if ( wait_cycle + n_dash > cycle )
        {
            continue;
        }

        int n_turn = ( player->bodyCount() > body_count_thr
                       ? default_n_turn
                       : predict_player_turn_cycle( ptype,
                                                    player->body(),
                                                    first_player_speed,
                                                    target_dist,
                                                    ( target_point - inertia_pos ).th(),
                                                    dist_thr,
                                                    use_back_dash ) );

        if ( wait_cycle + n_turn + n_dash <= cycle )
        {
            return cycle;
        }

    }

    return final_reach_cycle;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
FieldAnalyzer::predict_kick_count( const WorldModel & wm,
                                   const AbstractPlayerObject * kicker,
                                   const double & first_ball_speed,
                                   const AngleDeg & ball_move_angle )
{
    if ( wm.gameMode().type() != GameMode::PlayOn
         && ! wm.gameMode().isPenaltyKickMode() )
    {
        return 1;
    }

    if ( kicker->unum() == wm.self().unum()
         && wm.self().isKickable() )
    {
        Vector2D max_vel = KickTable::calc_max_velocity( ball_move_angle,
                                                         wm.self().kickRate(),
                                                         wm.ball().vel() );
        if ( max_vel.r2() >= std::pow( first_ball_speed, 2 ) )
        {
            return 1;
        }
    }

    if ( first_ball_speed > 2.5 )
    {
        return 3;
    }
    else if ( first_ball_speed > 1.5 )
    {
        return 2;
    }

    return 1;
}


/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
FieldAnalyzer::get_ball_field_line_cross_point( const Vector2D & ball_from,
                                                const Vector2D & ball_to,
                                                const Vector2D & p1,
                                                const Vector2D & p2,
                                                const double field_back_offset )
{
    const Segment2D line( p1, p2 );
    const Segment2D ball_segment( ball_from, ball_to );

    const Vector2D cross_point = ball_segment.intersection( line, true );

    if ( cross_point.isValid() )
    {
        if ( Vector2D( ball_to - ball_from ).r() <= EPS )
        {
            return cross_point;
        }

        return cross_point
            + Vector2D::polar2vector
            ( field_back_offset,
              Vector2D( ball_from - ball_to ).th() );
    }

    return ball_to;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
FieldAnalyzer::get_field_bound_predict_ball_pos( const WorldModel & wm,
                                                 const int predict_step,
                                                 const double field_back_offset )
{
    const ServerParam & SP = ServerParam::i();

    const Vector2D current_pos = wm.ball().pos();
    const Vector2D predict_pos = wm.ball().inertiaPoint( predict_step );

    const double wid = SP.pitchHalfWidth();
    const double len = SP.pitchHalfLength();

    const Vector2D corner_1( +len, +wid );
    const Vector2D corner_2( +len, -wid );
    const Vector2D corner_3( -len, -wid );
    const Vector2D corner_4( -len, +wid );

    const Rect2D pitch_rect = Rect2D::from_center( Vector2D( 0.0, 0.0 ),
                                                   len * 2, wid * 2 );

    if ( ! pitch_rect.contains( current_pos )
         && ! pitch_rect.contains( predict_pos ) )
    {
        const Vector2D result( bound( -len, current_pos.x, +len ),
                               bound( -wid, current_pos.y, +wid ) );

        dlog.addText( Logger::TEAM,
                      __FILE__": getBoundPredictBallPos "
                      "out of field, "
                      "current_pos = [%f, %f], predict_pos = [%f, %f], "
                      "result = [%f, %f]",
                      current_pos.x, current_pos.y,
                      predict_pos.x, predict_pos.y,
                      result.x, result.y );

        return result;
    }


    const Vector2D p0 = predict_pos;
    const Vector2D p1 = get_ball_field_line_cross_point( current_pos, p0, corner_1, corner_2, field_back_offset );
    const Vector2D p2 = get_ball_field_line_cross_point( current_pos, p1, corner_2, corner_3, field_back_offset );
    const Vector2D p3 = get_ball_field_line_cross_point( current_pos, p2, corner_3, corner_4, field_back_offset );
    const Vector2D p4 = get_ball_field_line_cross_point( current_pos, p3, corner_4, corner_1, field_back_offset );

    dlog.addText( Logger::TEAM,
                  __FILE__": getBoundPredictBallPos "
                  "current_pos = [%f, %f], predict_pos = [%f, %f], "
                  "result = [%f, %f]",
                  current_pos.x, current_pos.y,
                  predict_pos.x, predict_pos.y,
                  p4.x, p4.y );

    return p4;
}



/*-------------------------------------------------------------------*/
/*!

 */
namespace {

struct Player {
    const AbstractPlayerObject * player_;
    AngleDeg angle_from_pos_;
    double hide_angle_;

    Player( const AbstractPlayerObject * player,
            const Vector2D & pos )
        : player_( player ),
          angle_from_pos_(),
          hide_angle_( 0.0 )
      {
          Vector2D inertia_pos = player->inertiaFinalPoint();
          double control_dist = ( player->goalie()
                                  ? ServerParam::i().catchAreaLength()
                                  : player->playerTypePtr()->kickableArea() );
          double hide_angle_radian = std::asin( std::min( control_dist / inertia_pos.dist( pos ),
                                                          1.0 ) );

          angle_from_pos_ = ( inertia_pos - pos ).th();
          hide_angle_ = hide_angle_radian * AngleDeg::RAD2DEG;
      }

    struct Compare {
        bool operator()( const Player & lhs,
                         const Player & rhs ) const
          {
              return lhs.angle_from_pos_.degree() < rhs.angle_from_pos_.degree();
          }
    };
};

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FieldAnalyzer::can_shoot_from( const bool is_self,
                               const Vector2D & pos,
                               const AbstractPlayerCont & opponents,
                               const int valid_opponent_threshold )
{
    static const double SHOOT_DIST_THR2 = std::pow( 17.0, 2 );
    //static const double SHOOT_ANGLE_THRESHOLD = 20.0;
    static const double SHOOT_ANGLE_THRESHOLD = ( is_self
                                                  ? 20.0
                                                  : 15.0 );
    static const double OPPONENT_DIST_THR2 = std::pow( 20.0, 2 );

    if ( ServerParam::i().theirTeamGoalPos().dist2( pos )
         > SHOOT_DIST_THR2 )
    {
        return false;
    }

#ifdef DEBUG_CAN_SHOOT_FROM
    dlog.addText( Logger::SHOOT,
                  "===== "__FILE__": (can_shoot_from) pos=(%.1f %.1f) ===== ",
                  pos.x, pos.y );
#endif

    const Vector2D goal_minus( ServerParam::i().pitchHalfLength(),
                               -ServerParam::i().goalHalfWidth() + 0.5 );
    const Vector2D goal_plus( ServerParam::i().pitchHalfLength(),
                              +ServerParam::i().goalHalfWidth() - 0.5 );

    const AngleDeg goal_minus_angle = ( goal_minus - pos ).th();
    const AngleDeg goal_plus_angle = ( goal_plus - pos ).th();

    //
    // create opponent list
    //

    std::vector< Player > opponent_candidates;
    opponent_candidates.reserve( opponents.size() );

    const AbstractPlayerCont::const_iterator o_end = opponents.end();
    for ( AbstractPlayerCont::const_iterator o = opponents.begin();
          o != o_end;
          ++o )
    {
        if ( (*o)->posCount() > valid_opponent_threshold )
        {
            continue;
        }

        if ( (*o)->pos().dist2( pos ) > OPPONENT_DIST_THR2 )
        {
            continue;
        }

        opponent_candidates.push_back( Player( *o, pos ) );
#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(can_shoot_from) (opponent:%d) pos=(%.1f %.1f) angleFromPos=%.1f hideAngle=%.1f",
                      opponent_candidates.back().player_->unum(),
                      opponent_candidates.back().player_->pos().x,
                      opponent_candidates.back().player_->pos().y,
                      opponent_candidates.back().angle_from_pos_.degree(),
                      opponent_candidates.back().hide_angle_ );
#endif
    }

    //
    // TODO: improve the search algorithm (e.g. consider only angle width between opponents)
    //
    // std::sort( opponent_candidates.begin(), opponent_candidates.end(),
    //            Opponent::Compare() );

    const double angle_width = ( goal_plus_angle - goal_minus_angle ).abs();
    const double angle_step = std::max( 2.0, angle_width / 10.0 );

    const std::vector< Player >::const_iterator end = opponent_candidates.end();

    double max_angle_diff = 0.0;

    for ( double a = 0.0; a < angle_width + 0.001; a += angle_step )
    {
        const AngleDeg shoot_angle = goal_minus_angle + a;

        double min_angle_diff = 180.0;
        for ( std::vector< Player >::const_iterator o = opponent_candidates.begin();
              o != end;
              ++o )
        {
            double angle_diff = ( o->angle_from_pos_ - shoot_angle ).abs();

#ifdef DEBUG_CAN_SHOOT_FROM
            dlog.addText( Logger::SHOOT,
                          "(can_shoot_from) __ opp=%d rawAngleDiff=%.1f -> %.1f",
                          o->player_->unum(),
                          angle_diff, angle_diff - o->hide_angle_*0.5 );
#endif
            if ( is_self )
            {
                angle_diff -= o->hide_angle_;
            }
            else
            {
                angle_diff -= o->hide_angle_*0.5;
            }

            if ( angle_diff < min_angle_diff )
            {
                min_angle_diff = angle_diff;

                if ( min_angle_diff < SHOOT_ANGLE_THRESHOLD )
                {
                    break;
                }
            }
        }

        if ( min_angle_diff > max_angle_diff )
        {
            max_angle_diff = min_angle_diff;
        }

#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(can_shoot_from) shootAngle=%.1f minAngleDiff=%.1f",
                      shoot_angle.degree(),
                      min_angle_diff );
#endif
    }

#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(can_shoot_from) maxAngleDiff=%.1f",
                      max_angle_diff );
#endif

    return max_angle_diff >= SHOOT_ANGLE_THRESHOLD;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FieldAnalyzer::opponent_can_shoot_from( const Vector2D & pos,
                                        const AbstractPlayerCont & teammates,
                                        const int valid_teammate_threshold,
                                        const double shoot_dist_threshold,
                                        const double shoot_angle_threshold,
                                        const double teammate_dist_threshold,
                                        double * max_angle_diff_result,
                                        const bool calculate_detail )
{
    const double DEFAULT_SHOOT_DIST_THR = 40.0;
    const double DEFAULT_SHOOT_ANGLE_THR = 12.0;
    const double DEFAULT_TEAMMATE_DIST_THR2 = std::pow( 40.0, 2 );

    const double SHOOT_DIST_THR = ( shoot_dist_threshold > 0.0
                                    ? shoot_dist_threshold
                                    : DEFAULT_SHOOT_DIST_THR );
    const double SHOOT_ANGLE_THR = ( shoot_angle_threshold > 0.0
                                     ? shoot_angle_threshold
                                     : DEFAULT_SHOOT_ANGLE_THR );
    const double TEAMMATE_DIST_THR2 = ( teammate_dist_threshold > 0.0
                                        ? std::pow( teammate_dist_threshold, 2 )
                                        : DEFAULT_TEAMMATE_DIST_THR2 );

#ifdef DEBUG_CAN_SHOOT_FROM
    dlog.addText( Logger::SHOOT,
                  "===== "__FILE__": (opponent_can_shoot_from) from pos=(%.1f %.1f), n_teammates = %u ===== ",
                  pos.x, pos.y, static_cast< unsigned int >( teammates.size() ) );

    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) valid_teammate_threshold = %d",
                  valid_teammate_threshold );
    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) shoot_angle_threshold = %.2f",
                  SHOOT_ANGLE_THR );
    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) shoot_dist_threshold = %.2f",
                  SHOOT_DIST_THR );
    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) teammate_dist_threshold^2 = %.2f",
                  TEAMMATE_DIST_THR2 );
#endif

    if ( get_dist_from_our_near_goal_post( pos ) > SHOOT_DIST_THR )
    {
        if ( max_angle_diff_result )
        {
            *max_angle_diff_result = 0.0;
        }

        return false;
    }

    //
    // create teammate list
    //
    std::vector< Player > teammate_candidates;
    teammate_candidates.reserve( teammates.size() );

    const AbstractPlayerCont::const_iterator t_end = teammates.end();
    for ( AbstractPlayerCont::const_iterator t = teammates.begin();
          t != t_end;
          ++t )
    {
        if ( (*t)->posCount() > valid_teammate_threshold )
        {
#ifdef DEBUG_CAN_SHOOT_FROM
            dlog.addText( Logger::SHOOT,
                          "(opponent_can_shoot_from) skip teammate %d, too big pos count, pos count = %d",
                          (*t)->unum(), (*t)->posCount() );
#endif
            continue;
        }

        if ( (*t)->pos().dist2( pos ) > TEAMMATE_DIST_THR2 )
        {
#ifdef DEBUG_CAN_SHOOT_FROM
            dlog.addText( Logger::SHOOT,
                          "(opponent_can_shoot_from) skip teammate %d, too far from point, dist^2 = %f, pos = (%.2f, %.2f), teammate pos = (%.2f, %.2f)",
                          (*t)->unum(), (*t)->pos().dist2( pos ),
                          pos.x, pos.y, (*t) ->pos().x, (*t) ->pos().y );
#endif
            continue;
        }

        teammate_candidates.push_back( Player( *t, pos ) );
#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(opponent_can_shoot_from) (teammate:%d) pos=(%.1f %.1f) angleFromPos=%.1f hideAngle=%.1f",
                      teammate_candidates.back().player_->unum(),
                      teammate_candidates.back().player_->pos().x,
                      teammate_candidates.back().player_->pos().y,
                      teammate_candidates.back().angle_from_pos_.degree(),
                      teammate_candidates.back().hide_angle_ );
#endif
    }

    //
    // TODO: improve the search algorithm (e.g. consider only angle width between opponents)
    //
    // std::sort( opponent_candidates.begin(), opponent_candidates.end(),
    //            Opponent::Compare() );

    const Vector2D goal_minus( -ServerParam::i().pitchHalfLength(),
                               -ServerParam::i().goalHalfWidth() + 0.5 );
    const Vector2D goal_plus( -ServerParam::i().pitchHalfLength(),
                              +ServerParam::i().goalHalfWidth() - 0.5 );

    const AngleDeg goal_minus_angle = ( goal_minus - pos ).th();
    const AngleDeg goal_plus_angle = ( goal_plus - pos ).th();

    const double angle_width = ( goal_plus_angle - goal_minus_angle ).abs();
#ifdef DEBUG_CAN_SHOOT_FROM
    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) angle_width = %.2f,"
                  " goal_plus_angle = %.2f, goal_minus_angle = %2f",
                  angle_width, goal_plus_angle.degree(), goal_minus_angle.degree() );
#endif
    const double angle_step = std::max( 2.0, angle_width / 10.0 );

    double max_angle_diff = 0.0;

    const std::vector< Player >::const_iterator begin = teammate_candidates.begin();
    const std::vector< Player >::const_iterator end = teammate_candidates.end();

    for ( double a = 0.0; a < angle_width + 0.001; a += angle_step )
    {
        const AngleDeg shoot_angle = goal_minus_angle - a;
#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(opponent_can_shoot_from) shoot_angle = %.2f",
                      shoot_angle.degree() );
#endif

        double min_angle_diff = 180.0;
        for ( std::vector< Player >::const_iterator t = begin;
              t != end;
              ++t )
        {
            double angle_diff = ( t->angle_from_pos_ - shoot_angle ).abs();

#ifdef DEBUG_CAN_SHOOT_FROM
            dlog.addText( Logger::SHOOT,
                          "(opponent_can_shoot_from)__ teammate=%d rawAngleDiff=%.2f -> %.2f",
                          (*t).player_->unum(),
                          angle_diff, angle_diff - t->hide_angle_ );
#endif

            //angle_diff -= t->hide_angle_;
            angle_diff -= t->hide_angle_*0.5;

            if ( angle_diff < min_angle_diff )
            {
                min_angle_diff = angle_diff;

                if ( min_angle_diff < SHOOT_ANGLE_THR )
                {
                    if ( ! calculate_detail )
                    {
#ifdef DEBUG_CAN_SHOOT_FROM
                        dlog.addText( Logger::SHOOT,
                                      "(opponent_can_shoot_from)__ min_angle_diff < SHOOT_ANGLE_THR: skip other teammates" );
#endif

                        break;
                    }
                }
            }
        }

        if ( min_angle_diff > max_angle_diff )
        {
            max_angle_diff = min_angle_diff;
        }

#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(opponent_can_shoot_from) shootAngle=%.2f minAngleDiff=%.2f",
                      shoot_angle.degree(),
                      min_angle_diff );
#endif
    }

    const bool result = ( max_angle_diff >= SHOOT_ANGLE_THR );
    if ( max_angle_diff_result )
    {
        *max_angle_diff_result = max_angle_diff;
    }

#ifdef DEBUG_CAN_SHOOT_FROM
    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) maxAngleDiff=%.2f, result = %s",
                  max_angle_diff, ( result? "true" : "false" ) );
#endif

    return result;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldAnalyzer::get_dist_player_nearest_to_point( const Vector2D & point,
                                                 const PlayerCont & players,
                                                 const int count_thr )
{
    double min_dist2 = 65535.0;

    const PlayerCont::const_iterator end = players.end();
    for ( PlayerCont::const_iterator it = players.begin();
          it != end;
          ++it )
    {
        if ( (*it).isGhost() )
        {
            continue;
        }

        if ( count_thr != -1 )
        {
            if ( (*it).posCount() > count_thr )
            {
                continue;
            }
        }

        double d2 = (*it).pos().dist2( point );

        if ( d2 < min_dist2 )
        {
            min_dist2 = d2;
        }
    }

    return std::sqrt( min_dist2 );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
FieldAnalyzer::get_our_team_near_goal_post_pos( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return Vector2D( -SP.pitchHalfLength(),
                     +sign( point.y ) * SP.goalHalfWidth() );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
FieldAnalyzer::get_our_team_far_goal_post_pos( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return Vector2D( -SP.pitchHalfLength(),
                     -sign( point.y ) * SP.goalHalfWidth() );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
FieldAnalyzer::get_opponent_team_near_goal_post_pos( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return Vector2D( +SP.pitchHalfLength(),
                     +sign( point.y ) * SP.goalHalfWidth() );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
FieldAnalyzer::get_opponent_team_far_goal_post_pos( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return Vector2D( +SP.pitchHalfLength(),
                     -sign( point.y ) * SP.goalHalfWidth() );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldAnalyzer::get_dist_from_our_near_goal_post( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return std::min( point.dist( Vector2D
                                 ( - SP.pitchHalfLength(),
                                   - SP.goalHalfWidth() ) ),
                     point.dist( Vector2D
                                 ( - SP.pitchHalfLength(),
                                   + SP.goalHalfWidth() ) ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldAnalyzer::get_dist_from_opponent_near_goal_post( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return std::min( point.dist( Vector2D
                                 ( + SP.pitchHalfLength(),
                                   - SP.goalHalfWidth() ) ),
                     point.dist( Vector2D
                                 ( + SP.pitchHalfLength(),
                                   + SP.goalHalfWidth() ) ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
int
FieldAnalyzer::get_pass_count( const PredictState & state,
                               const PassChecker & pass_checker,
                               const double first_ball_speed,
                               const int max_count )
{
    const AbstractPlayerObject * from = state.ballHolder();

    if ( ! from )
    {
        dlog.addText( Logger::ANALYZER,
                      "get_pass_count: invalid ball holder" );
        return 0;
    }


    int pass_count = 0;
    for ( PredictPlayerPtrCont::const_iterator
              it = state.ourPlayers().begin(),
              end = state.ourPlayers().end();
          it != end;
          ++it )
    {
        if ( ! (*it)->isValid()
             || (*it)->unum() == from->unum() )
        {
            continue;
        }

        if ( pass_checker( state, *from, **it, (*it)->pos(), first_ball_speed ) )
        {
            pass_count ++;

            if ( max_count >= 0
                 && pass_count >= max_count )
            {
                return max_count;
            }
        }
    }

    return pass_count;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FieldAnalyzer::is_ball_moving_to_our_goal( const Vector2D & ball_pos,
                                           const Vector2D & ball_vel,
                                           const double & post_buffer )
{
    const double goal_half_width = ServerParam::i().goalHalfWidth();
    const double goal_line_x = ServerParam::i().ourTeamGoalLineX();
    const Vector2D goal_plus_post( goal_line_x,
                                   +goal_half_width + post_buffer );
    const Vector2D goal_minus_post( goal_line_x,
                                    -goal_half_width - post_buffer );
    const AngleDeg ball_angle = ball_vel.th();

    return ( ( ( goal_plus_post - ball_pos ).th() - ball_angle ).degree() < 0
             && ( ( goal_minus_post - ball_pos ).th() - ball_angle ).degree() > 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FieldAnalyzer::to_be_final_action( const PredictState & state )
{
    return to_be_final_action( state.ball().pos(), state.theirDefensePlayerLineX() );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FieldAnalyzer::to_be_final_action( const WorldModel & wm )
{
    return to_be_final_action( wm.ball().pos(), wm.theirDefensePlayerLineX() );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FieldAnalyzer::to_be_final_action( const Vector2D & ball_pos,
                                   const double their_defense_player_line_x )
{
    // if near opponent goal, search next action such as shoot
    if ( ball_pos.x > 30.0 )
    {
        return false;
    }

    // if over offside line, final action
    if ( ball_pos.x > their_defense_player_line_x )
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
const AbstractPlayerObject *
FieldAnalyzer::get_blocker( const WorldModel & wm,
                            const Vector2D & opponent_pos )
{
    return get_blocker( wm,
                        opponent_pos,
                        Vector2D( -ServerParam::i().pitchHalfLength()*0.6
                                  + ServerParam::i().ourPenaltyAreaLineX()*0.4,
                                  0.0 ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
const AbstractPlayerObject *
FieldAnalyzer::get_blocker( const WorldModel & wm,
                            const Vector2D & opponent_pos,
                            const Vector2D & base_pos )
{
    static const double min_dist_thr2 = std::pow( 1.0, 2 );
    static const double max_dist_thr2 = std::pow( 4.0, 2 );
    static const double angle_thr = 15.0;

    const AngleDeg attack_angle = ( base_pos - opponent_pos ).th();

    for ( AbstractPlayerCont::const_iterator
              t = wm.ourPlayers().begin(),
              end = wm.ourPlayers().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( (*t)->posCount() >= 5 ) continue;
        if ( (*t)->unumCount() >= 10 ) continue;
        if ( (*t)->ghostCount() >= 2 ) continue;

        double d2 = opponent_pos.dist2( (*t)->pos() );
        if ( d2 < min_dist_thr2
             || max_dist_thr2 < d2 )
        {
            continue;
        }

        AngleDeg teammate_angle = ( (*t)->pos() - opponent_pos ).th();

        if ( ( teammate_angle - attack_angle ).abs() < angle_thr )
        {
            return *t;
        }
    }

    return static_cast< const AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::update( const WorldModel & wm )
{
#ifdef __APPLE__
    static GameTime s_update_time( 0, 0 );
#else
    static thread_local GameTime s_update_time( 0, 0 );
#endif

    if ( s_update_time == wm.time() )
    {
        return;
    }
    s_update_time = wm.time();

    if ( wm.gameMode().type() == GameMode::BeforeKickOff
         || wm.gameMode().type() == GameMode::AfterGoal_
         || wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }

#ifdef DEBUG_PRINT
    Timer timer;
#endif

    // updateVoronoiDiagram( wm );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM,
                  "FieldAnalyzer::update() elapsed %f [ms]",
                  timer.elapsedReal() );
    writeDebugLog();
#endif

}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::updateVoronoiDiagram( const WorldModel & wm )
{
    const Rect2D rect = Rect2D::from_center( 0.0, 0.0,
                                             ServerParam::i().pitchLength() - 10.0,
                                             ServerParam::i().pitchWidth() - 10.0 );

    M_all_players_voronoi_diagram.clear();
    M_teammates_voronoi_diagram.clear();
    M_pass_voronoi_diagram.clear();

    const SideID our = wm.ourSide();

    const AbstractPlayerCont::const_iterator end = wm.allPlayers().end();
    for ( AbstractPlayerCont::const_iterator p = wm.allPlayers().begin();
          p != end;
          ++p )
    {
        M_all_players_voronoi_diagram.addPoint( (*p)->pos() );

        if ( (*p)->side() == our )
        {
            M_teammates_voronoi_diagram.addPoint( (*p)->pos() );
        }
        else
        {
            M_pass_voronoi_diagram.addPoint( (*p)->pos() );
        }
    }

    // our goal
    M_pass_voronoi_diagram.addPoint( Vector2D( - ServerParam::i().pitchHalfLength() + 5.5, 0.0 ) );
    //     M_pass_voronoi_diagram.addPoint( Vector2D( - ServerParam::i().pitchHalfLength() + 5.5,
    //                                                - ServerParam::i().goalHalfWidth() ) );
    //     M_pass_voronoi_diagram.addPoint( Vector2D( - ServerParam::i().pitchHalfLength() + 5.5,
    //                                                + ServerParam::i().goalHalfWidth() ) );

    // opponent side corners
    M_pass_voronoi_diagram.addPoint( Vector2D( + ServerParam::i().pitchHalfLength() + 10.0,
                                               - ServerParam::i().pitchHalfWidth() - 10.0 ) );
    M_pass_voronoi_diagram.addPoint( Vector2D( + ServerParam::i().pitchHalfLength() + 10.0,
                                               + ServerParam::i().pitchHalfWidth() + 10.0 ) );

    M_pass_voronoi_diagram.setBoundingRect( rect );

    M_all_players_voronoi_diagram.compute();
    M_teammates_voronoi_diagram.compute();
    M_pass_voronoi_diagram.compute();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::writeDebugLog()
{

    if ( dlog.isEnabled( Logger::PASS ) )
    {
        const VoronoiDiagram::Segment2DCont::const_iterator s_end = M_pass_voronoi_diagram.resultSegments().end();
        for ( VoronoiDiagram::Segment2DCont::const_iterator s = M_pass_voronoi_diagram.resultSegments().begin();
              s != s_end;
              ++s )
        {
            dlog.addLine( Logger::PASS,
                          s->origin(), s->terminal(),
                          "#0000ff" );
        }

        const VoronoiDiagram::Ray2DCont::const_iterator r_end = M_pass_voronoi_diagram.resultRays().end();
        for ( VoronoiDiagram::Ray2DCont::const_iterator r = M_pass_voronoi_diagram.resultRays().begin();
              r != r_end;
              ++r )
        {
            dlog.addLine( Logger::PASS,
                          r->origin(), r->origin() + Vector2D::polar2vector( 20.0, r->dir() ),
                          "#0000ff" );
        }
    }
}
