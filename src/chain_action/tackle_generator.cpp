// -*-c++-*-

/*!
  \file tackle_generator.cpp
  \brief tackle/foul generator class Source File
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tackle_generator.h"

#include "field_analyzer.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/math_util.h>
#include <rcsc/timer.h>

#include <limits>

#define ASUUME_OPPONENT_KICK

#define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PRINT_EVALUATE

// #define DEBUG_PREDICT_OPPONENT_REACH_STEP
// #define DEBUG_PREDICT_OPPONENT_REACH_STEP_LEVEL2


using namespace rcsc;

namespace {

const int ANGLE_DIVS = 40;

/*-------------------------------------------------------------------*/
/*!

 */
struct DeflectingEvaluator {
    constexpr static const double not_shoot_ball_eval = 10000;

    double operator()( const WorldModel & wm,
                       TackleGenerator::TackleResult & result ) const
      {
          double eval = 0.0;

          if ( ! FieldAnalyzer::is_ball_moving_to_our_goal( wm.ball().pos(),
                                                            result.ball_vel_,
                                                            2.0 ) )
          {
              eval += not_shoot_ball_eval;
          }

          Vector2D final_ball_pos = inertia_final_point( wm.ball().pos(),
                                                         result.ball_vel_,
                                                         ServerParam::i().ballDecay() );

          if ( result.ball_vel_.x < 0.0
               && wm.ball().pos().x > ServerParam::i().ourTeamGoalLineX() + 0.5 )
          {
              //const double goal_half_width = ServerParam::i().goalHalfWidth();
              const double pitch_half_width = ServerParam::i().pitchHalfWidth();
              const double goal_line_x = ServerParam::i().ourTeamGoalLineX();
              const Vector2D corner_plus_post( goal_line_x, +pitch_half_width );
              const Vector2D corner_minus_post( goal_line_x, -pitch_half_width );

              const Line2D goal_line( corner_plus_post, corner_minus_post );

              const Segment2D ball_segment( wm.ball().pos(), final_ball_pos );
              Vector2D cross_point = ball_segment.intersection( goal_line );
              if ( cross_point.isValid() )
              {
                  eval += 1000.0;

                  double c = std::min( std::fabs( cross_point.y ),
                                       ServerParam::i().pitchHalfWidth() );
                  //if ( c > goal_half_width
                  //     && ( cross_point.y * wm.self().pos().y >= 0.0
                  //          && c > wm.self().pos().absY() ) )
                  {
                      eval += c;
                  }
              }
          }
          else
          {
              if ( final_ball_pos.x > ServerParam::i().ourTeamGoalLineX() + 5.0 )
              {
                  eval += 1000.0;
              }

              eval += sign( wm.ball().pos().y ) * result.ball_vel_.y;
          }

          return eval;
      }
};

}

/*-------------------------------------------------------------------*/
/*!

 */
TackleGenerator::TackleResult::TackleResult()
{
    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
TackleGenerator::TackleResult::TackleResult( const rcsc::AngleDeg & angle,
                                             const rcsc::Vector2D & vel )
    : tackle_angle_( angle ),
      ball_vel_( vel ),
      ball_speed_( vel.r() ),
      ball_move_angle_( vel.th() ),
      score_( 0.0 )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
TackleGenerator::TackleResult::clear()
{
    tackle_angle_ = 0.0;
    ball_vel_.assign( 0.0, 0.0 );
    ball_speed_ = 0.0;
    ball_move_angle_ = 0.0;
    score_ = -std::numeric_limits< double >::max();
}

/*-------------------------------------------------------------------*/
/*!

 */
TackleGenerator::TackleGenerator()
{
    M_candidates.reserve( ANGLE_DIVS );
    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
TackleGenerator &
TackleGenerator::instance()
{
#ifdef __APPLE__
    static TackleGenerator s_instance;
#else
    static thread_local TackleGenerator s_instance;
#endif
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
TackleGenerator::clear()
{
    M_best_result = TackleResult();
    M_candidates.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
TackleGenerator::generate( const WorldModel & wm )
{
#ifdef __APPLE__
    static GameTime s_update_time( 0, 0 );
#else
    static thread_local GameTime s_update_time( 0, 0 );
#endif

    if ( s_update_time == wm.time() )
    {
        // dlog.addText( Logger::CLEAR,
        //               __FILE__": already updated" );
        return;
    }
    s_update_time = wm.time();

    clear();

    if ( wm.self().isKickable() )
    {
        // dlog.addText( Logger::CLEAR,
        //               __FILE__": kickable" );
        return;
    }

    if ( wm.self().tackleProbability() < 0.001
         && wm.self().foulProbability() < 0.001 )
    {
        // dlog.addText( Logger::CLEAR,
        //               __FILE__": never tacklable" );
        return;
    }

    if ( wm.time().stopped() > 0 )
    {
        // dlog.addText( Logger::CLEAR,
        //               __FILE__": time stopped" );
        return;
    }

    if ( wm.gameMode().type() != GameMode::PlayOn
         && ! wm.gameMode().isPenaltyKickMode() )
    {
        // dlog.addText( Logger::CLEAR,
        //               __FILE__": illegal playmode " );
        return;
    }


#ifdef DEBUG_PROFILE
    MSecTimer timer;
#endif

    calculate( wm );

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::CLEAR,
                  __FILE__": PROFILE. elapsed=%.3f [ms]",
                  timer.elapsedReal() );
#endif

}

/*-------------------------------------------------------------------*/
/*!

 */
void
TackleGenerator::calculate( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();

    const double min_angle = SP.minMoment();
    const double max_angle = SP.maxMoment();
    const double angle_step = std::fabs( max_angle - min_angle ) / ANGLE_DIVS;

#ifdef ASSUME_OPPONENT_KICK
    const Vector2D goal_pos = SP.ourTeamGoalPos();
    const bool shootable_area = ( wm.ball().pos().dist2( goal_pos ) < std::pow( 18.0, 2 ) );
    const Vector2D shoot_accel = ( goal_pos - wm.ball().pos() ).setLengthVector( 2.0 );
#endif

    const AngleDeg ball_rel_angle = wm.ball().angleFromSelf() - wm.self().body();
    const double tackle_rate
        = SP.tacklePowerRate()
        * ( 1.0 - 0.5 * ball_rel_angle.abs() / 180.0 );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::CLEAR,
                  __FILE__": min_angle=%.1f max_angle=%.1f angle_step=%.1f",
                  min_angle, max_angle, angle_step );
    dlog.addText( Logger::CLEAR,
                  __FILE__": ball_rel_angle=%.1f tackle_rate=%.1f",
                  ball_rel_angle.degree(), tackle_rate );
#endif

    for ( int a = 0; a < ANGLE_DIVS; ++a )
    {
        const AngleDeg dir = min_angle + angle_step * a;

        double eff_power= ( SP.maxBackTacklePower()
                            + ( SP.maxTacklePower() - SP.maxBackTacklePower() )
                            * ( 1.0 - ( dir.abs() / 180.0 ) ) );
        eff_power *= tackle_rate;

        AngleDeg angle = wm.self().body() + dir;
        Vector2D accel = Vector2D::from_polar( eff_power, angle );

#ifdef ASSUME_OPPONENT_KICK
        if ( shootable_area
             && wm.existKickableOpponent() )
        {
            accel += shoot_accel;
            double d = accel.r();
            if ( d > SP.ballAccelMax() )
            {
                accel *= ( SP.ballAccelMax() / d );
            }
        }
#endif

        Vector2D vel = wm.ball().vel() + accel;

        double speed = vel.r();
        if ( speed > SP.ballSpeedMax() )
        {
            vel *= ( SP.ballSpeedMax() / speed );
        }

        M_candidates.push_back( TackleResult( angle, vel ) );
#ifdef DEBUG_PRINT
        const TackleResult & result = M_candidates.back();
        dlog.addText( Logger::CLEAR,
                      "%d: angle=%.1f(dir=%.1f), result: vel(%.2f %.2f ) speed=%.2f move_angle=%.1f",
                      a,
                      result.tackle_angle_.degree(), dir.degree(),
                      result.ball_vel_.x, result.ball_vel_.y,
                      result.ball_speed_, result.ball_move_angle_.degree() );
#endif
    }


    M_best_result.clear();

    const Container::iterator end = M_candidates.end();
    for ( Container::iterator it = M_candidates.begin();
          it != end;
          ++it )
    {
        it->score_ = evaluate( wm, *it );

#ifdef DEBUG_PRINT
        Vector2D ball_end_point = inertia_final_point( wm.ball().pos(),
                                                       it->ball_vel_,
                                                       SP.ballDecay() );
        dlog.addLine( Logger::CLEAR,
                      wm.ball().pos(),
                      ball_end_point,
                      "#0000ff" );
        char buf[16];
        snprintf( buf, 16, "%.3f", it->score_ );
        dlog.addMessage( Logger::CLEAR,
                         ball_end_point, buf, "#ffffff" );
#endif

        if ( it->score_ > M_best_result.score_ )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          ">>>> updated" );
#endif
            M_best_result = *it;
        }
    }


#ifdef DEBUG_PRINT
    dlog.addLine( Logger::CLEAR,
                  wm.ball().pos(),
                  inertia_final_point( wm.ball().pos(),
                                       M_best_result.ball_vel_,
                                       SP.ballDecay() ),
                  "#ff0000" );
    dlog.addText( Logger::CLEAR,
                  "==== best_angle=%.1f score=%f speed=%.3f move_angle=%.1f",
                  M_best_result.tackle_angle_.degree(),
                  M_best_result.score_,
                  M_best_result.ball_speed_,
                  M_best_result.ball_move_angle_.degree() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
double
TackleGenerator::evaluate( const WorldModel & wm,
                           const TackleResult & result )
{
    const ServerParam & SP = ServerParam::i();

    const Vector2D ball_end_point = inertia_final_point( wm.ball().pos(),
                                                         result.ball_vel_,
                                                         SP.ballDecay() );
    const Segment2D ball_line( wm.ball().pos(), ball_end_point );
    const double ball_speed = result.ball_speed_;
    const AngleDeg ball_move_angle = result.ball_move_angle_;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::CLEAR,
                  "(evaluate) angle=%.1f speed=%.2f move_angle=%.1f end_point=(%.2f %.2f)",
                  result.tackle_angle_.degree(),
                  ball_speed,
                  ball_move_angle.degree(),
                  ball_end_point.x, ball_end_point.y );
#endif

    //
    // moving to their goal
    //
    if ( ball_end_point.x > SP.pitchHalfLength()
         && wm.ball().pos().dist2( SP.theirTeamGoalPos() ) < std::pow( 20.0, 2 ) )
    {
        const Line2D goal_line( Vector2D( SP.pitchHalfLength(), 10.0 ),
                                Vector2D( SP.pitchHalfLength(), -10.0 ) );
        const Vector2D intersect = ball_line.intersection( goal_line );
        if ( intersect.isValid()
             && intersect.absY() < SP.goalHalfWidth() )
        {
            double shoot_score = 1000000.0;
            double speed_rate = 1.0 - std::exp( - std::pow( ball_speed, 2 )
                                                / ( 2.0 * std::pow( SP.ballSpeedMax()*0.5, 2 ) ) );
            double y_rate = std::exp( - std::pow( intersect.absY(), 2 )
                                      / ( 2.0 * std::pow( SP.goalWidth(), 2 ) ) );
            shoot_score *= speed_rate;
            shoot_score *= y_rate;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "__ shoot %f (speed_rate=%f y_rate=%f)",
                          shoot_score, speed_rate, y_rate );
#endif
            return shoot_score;
        }
    }

    //
    // moving to our goal
    //
    if ( ball_end_point.x < -SP.pitchHalfLength() )
    {
        const Line2D goal_line( Vector2D( -SP.pitchHalfLength(), 10.0 ),
                                Vector2D( -SP.pitchHalfLength(), -10.0 ) );
        const Vector2D intersect = ball_line.intersection( goal_line );
        if ( intersect.isValid()
             && intersect.absY() < SP.goalHalfWidth() + 1.0 )
        {
            double shoot_score = 0.0;
            double y_penalty = ( -10000.0
                                 * std::exp( - std::pow( intersect.absY() - SP.goalHalfWidth(), 2 )
                                             / ( 2.0 * std::pow( SP.goalHalfWidth(), 2 ) ) ) );
            double speed_bonus = ( +10000.0
                                   * std::exp( - std::pow( ball_speed, 2 )
                                               / ( 2.0 * std::pow( SP.ballSpeedMax()*0.5, 2 ) ) ) );
            shoot_score = y_penalty + speed_bonus;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "__ in our goal %f (y_pealty=%f speed_bonus=%f)",
                          shoot_score, y_penalty, speed_bonus );
#endif
            return shoot_score;
        }
    }

    //
    // normal evaluation
    //

    int opponent_reach_step = predictOpponentsReachStep( wm,
                                                         wm.ball().pos(),
                                                         result.ball_vel_,
                                                         ball_move_angle );
    Vector2D final_point = inertia_n_step_point( wm.ball().pos(),
                                                 result.ball_vel_,
                                                 opponent_reach_step,
                                                 SP.ballDecay() );
    {
        Segment2D final_segment( wm.ball().pos(), final_point );
        Rect2D pitch = Rect2D::from_center( 0.0, 0.0, SP.pitchLength(), SP.pitchWidth() );
        Vector2D intersection;
        int n = pitch.intersection( final_segment, &intersection, NULL );
        if ( n > 0 )
        {
            final_point = intersection;
        }
    }

#if 1

    AngleDeg our_goal_angle = ( SP.ourTeamGoalPos() - wm.ball().pos() ).th();
    double our_goal_angle_diff = ( our_goal_angle - ball_move_angle ).abs();
    double our_goal_angle_rate = 1.0 - std::exp( - std::pow( our_goal_angle_diff, 2 )
                                                 / ( 2.0 * std::pow( 40.0, 2 ) ) );

    double y_rate = ( final_point.absY() > SP.pitchHalfWidth() - 0.1
                      ? 1.0
                      : std::exp( - std::pow( final_point.absY() - SP.pitchHalfWidth(), 2 )
                                  / ( 2.0 * std::pow( SP.pitchHalfWidth() * 0.7, 2 ) ) ) );
    double opp_rate = 1.0 - std::exp( - std::pow( (double)opponent_reach_step, 2 )
                                      / ( 2.0 * std::pow( 30.0, 2 ) ) );

    double score = 10000.0 * our_goal_angle_rate * y_rate * opp_rate;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::CLEAR,
                  "__ goal_angle_rate=%f",
                  our_goal_angle_rate, y_rate, score );
    dlog.addText( Logger::CLEAR,
                  "__ y_rate=%f",
                  our_goal_angle_rate, y_rate, score );
    dlog.addText( Logger::CLEAR,
                  "__ opp_rate=%f",
                  opp_rate );

    dlog.addText( Logger::CLEAR,
                  ">>> score=%f",
                  score );
#endif

    return score;

#else

    double x_val = ( final_point.x > SP.pitchHalfLength()
                     ? 1.0
                     : std::exp( - std::pow( final_point.x - SP.pitchHalfLength(), 2 )
                                 / ( 2.0 * std::pow( SP.pitchLength() * 0.4, 2 ) ) ) );
    double y_val = ( final_point.absY() > SP.pitchHalfWidth()
                     ? 1.0
                     : std::exp( - std::pow( final_point.absY() - SP.pitchHalfWidth(), 2 )
                                 / ( 2.0 * std::pow( SP.pitchHalfWidth() * 0.7, 2 ) ) ) );


    double opp_goal_dist = SP.theirTeamGoalPos().dist( final_point );
    double opp_goal_dist_val = std::exp( - std::pow( opp_goal_dist, 2 )
                                         / ( 2.0 * std::pow( 20.0, 2 ) ) );

    double our_goal_dist = SP.ourTeamGoalPos().dist( final_point );
    double our_goal_dist_rate = 1.0 - std::exp( - std::pow( our_goal_dist, 2 )
                                                / ( 2.0 * std::pow( 20.0, 2 ) ) );


    double opp_rate = 1.0 - std::exp( - std::pow( (double)opponent_reach_step, 2 )
                                      / ( 2.0 * std::pow( 30.0, 2 ) ) );

    double score = 0.0;

    score += 1000.0 * x_val;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::CLEAR,
                  "__ x_val %f (%f)", 10000.0 * x_val, score );
#endif

    score += 1000.0 * y_val;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::CLEAR,
                  "__ y_val %f (%f)", 1000.0 * y_val, score );
#endif

    score += 1000.0 * opp_goal_dist_val;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::CLEAR,
                  "__ opp_goal_dist_val %f (%f)", 1000.0 * opp_goal_dist_val, score );
#endif

    score *= our_goal_dist_rate;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::CLEAR,
                  "__ our_goal_dist=%.2f rate=%f (%f)",
                  our_goal_dist, our_goal_dist_rate, score );
#endif

    score *= opp_rate;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::CLEAR,
                  "__ opponent_reach_step=%d rate=%f (%f)",
                  opponent_reach_step, opp_rate, score );
#endif

    return score;

#endif
}


/*-------------------------------------------------------------------*/
/*!

 */
int
TackleGenerator::predictOpponentsReachStep( const WorldModel & wm,
                                            const Vector2D & first_ball_pos,
                                            const Vector2D & first_ball_vel,
                                            const AngleDeg & ball_move_angle )
{
    int first_min_step = 50;

#if 1
    const ServerParam & SP = ServerParam::i();
    const Vector2D ball_end_point = inertia_final_point( first_ball_pos,
                                                         first_ball_vel,
                                                         SP.ballDecay() );
    if ( ball_end_point.absX() > SP.pitchHalfLength()
         || ball_end_point.absY() > SP.pitchHalfWidth() )
    {
        Rect2D pitch = Rect2D::from_center( 0.0, 0.0, SP.pitchLength(), SP.pitchWidth() );
        Ray2D ball_ray( first_ball_pos, ball_move_angle );
        Vector2D sol1, sol2;
        int n_sol = pitch.intersection( ball_ray, &sol1, &sol2 );
        if ( n_sol == 1 )
        {
            first_min_step = SP.ballMoveStep( first_ball_vel.r(), first_ball_pos.dist( sol1 ) );
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "(predictOpponent) ball will be out. step=%d reach_point=(%.2f %.2f)",
                          first_min_step,
                          sol1.x, sol1.y );
#endif
        }
    }
#endif

    int min_step = first_min_step;
    for ( AbstractPlayerCont::const_iterator
              o = wm.theirPlayers().begin(),
              end = wm.theirPlayers().end();
          o != end;
          ++o )
    {
        int step = predictOpponentReachStep( *o,
                                             first_ball_pos,
                                             first_ball_vel,
                                             ball_move_angle,
                                             min_step );
        if ( step < min_step )
        {
            min_step = step;
        }
    }

    return ( min_step == first_min_step
             ? 1000
             : min_step );
}


/*-------------------------------------------------------------------*/
/*!

 */
int
TackleGenerator::predictOpponentReachStep( const AbstractPlayerObject * opponent,
                                           const Vector2D & first_ball_pos,
                                           const Vector2D & first_ball_vel,
                                           const AngleDeg & ball_move_angle,
                                           const int max_cycle )
{
    const ServerParam & SP = ServerParam::i();

    const PlayerType * ptype = opponent->playerTypePtr();
    const double opponent_speed = opponent->vel().r();

    int min_cycle = FieldAnalyzer::estimate_min_reach_cycle( opponent->pos(),
                                                             ptype->realSpeedMax(),
                                                             first_ball_pos,
                                                             ball_move_angle );
    if ( min_cycle < 0 )
    {
        min_cycle = 10;
    }

    for ( int cycle = min_cycle; cycle < max_cycle; ++cycle )
    {
        Vector2D ball_pos = inertia_n_step_point( first_ball_pos,
                                                  first_ball_vel,
                                                  cycle,
                                                  SP.ballDecay() );

        if ( ball_pos.absX() > SP.pitchHalfLength()
             || ball_pos.absY() > SP.pitchHalfWidth() )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
            dlog.addText( Logger::CLEAR,
                          "__ opponent=%d(%.1f %.1f) step=%d ball is out of pitch. ",
                          opponent->unum(),
                          opponent->pos().x, opponent->pos().y,
                          cycle  );
#endif
            return 1000;
        }

        Vector2D inertia_pos = opponent->inertiaPoint( cycle );
        double target_dist = inertia_pos.dist( ball_pos );

        if ( target_dist - ptype->kickableArea() < 0.001 )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
            dlog.addText( Logger::CLEAR,
                          "____ opponent=%d(%.1f %.1f) step=%d already there. dist=%.1f",
                          opponent->unum(),
                          opponent->pos().x, opponent->pos().y,
                          cycle,
                          target_dist );
#endif
            return cycle;
        }

        double dash_dist = target_dist;
        if ( cycle > 1 )
        {
            dash_dist -= ptype->kickableArea();
            dash_dist -= 0.5; // special bonus
        }

        if ( dash_dist > ptype->realSpeedMax() * cycle )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP_LEVEL2
            dlog.addText( Logger::CLEAR,
                          "______ opponent=%d(%.1f %.1f) cycle=%d dash_dist=%.1f reachable=%.1f",
                          opponent->unum(),
                          opponent->pos().x, opponent->pos().y,
                          cycle, dash_dist, ptype->realSpeedMax()*cycle );
#endif
            continue;
        }

        //
        // dash
        //

        int n_dash = ptype->cyclesToReachDistance( dash_dist );

        if ( n_dash > cycle )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP_LEVEL2
            dlog.addText( Logger::CLEAR,
                          "______ opponent=%d(%.1f %.1f) cycle=%d dash_dist=%.1f n_dash=%d",
                          opponent->unum(),
                          opponent->pos().x, opponent->pos().y,
                          cycle, dash_dist, n_dash );
#endif
            continue;
        }

        //
        // turn
        //
        int n_turn = ( opponent->bodyCount() > 1
                       ? 0
                       : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                   opponent->body(),
                                                                   opponent_speed,
                                                                   target_dist,
                                                                   ( ball_pos - inertia_pos ).th(),
                                                                   ptype->kickableArea(),
                                                                   true ) );

        int n_step = ( n_turn == 0
                       ? n_turn + n_dash
                       : n_turn + n_dash + 1 ); // 1 step penalty for observation delay
        if ( opponent->isTackling() )
        {
            n_step += 5; // Magic Number
        }

        n_step -= std::min( 3, opponent->posCount() );

        if ( n_step <= cycle )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
            dlog.addText( Logger::CLEAR,
                          "____ opponent=%d(%.1f %.1f) step=%d(t:%d,d:%d) bpos=(%.2f %.2f) dist=%.2f dash_dist=%.2f",
                          opponent->unum(),
                          opponent->pos().x, opponent->pos().y,
                          cycle, n_turn, n_dash,
                          ball_pos.x, ball_pos.y,
                          target_dist,
                          dash_dist );
#endif
            return cycle;
        }

    }

    return 1000;
}
