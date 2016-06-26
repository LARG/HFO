// -*-c++-*-

/*!
  \file bhv_pass_kick_find_receiver.cpp
  \brief search the pass receiver player and perform pass kick if possible
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_pass_kick_find_receiver.h"

#include "action_chain_holder.h"
#include "action_chain_graph.h"
#include "field_analyzer.h"

#include "neck_turn_to_receiver.h"

#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/body_stop_ball.h>
#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_point.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/view_synch.h>
#include <rcsc/action/kick_table.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>

#include <vector>
#include <algorithm>
#include <cmath>

// #define DEBUG_PRINT

using namespace rcsc;


class IntentionPassKickFindReceiver
    : public SoccerIntention {
private:
    int M_step;
    int M_receiver_unum;
    Vector2D M_receive_point;

public:

    IntentionPassKickFindReceiver( const int receiver_unum,
                                   const Vector2D & receive_point )
        : M_step( 0 ),
          M_receiver_unum( receiver_unum ),
          M_receive_point( receive_point )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );

private:

};

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionPassKickFindReceiver::finished( const PlayerAgent * agent )
{
    ++M_step;

    dlog.addText( Logger::TEAM,
                  __FILE__": (finished) step=%d",
                  M_step );

    if ( M_step >= 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) time over" );
        return true;
    }

    const WorldModel & wm = agent->world();

    //
    // check kickable
    //

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) no kickable" );
        return true;
    }

    //
    // check receiver
    //

    const AbstractPlayerObject * receiver = wm.ourPlayer( M_receiver_unum );

    if ( ! receiver )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) NULL receiver" );
        return true;
    }

    if ( receiver->unum() == wm.self().unum() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) receiver is self" );
        return true;
    }

    if ( receiver->posCount() <= 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) already seen" );
        return true;
    }

    //
    // check opponent
    //

    if ( wm.existKickableOpponent() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) exist kickable opponent" );
        return true;
    }

    if ( wm.interceptTable()->opponentReachCycle() <= 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) opponent may be kickable" );
        return true;
    }

    //
    // check next kickable
    //

    double kickable2 = std::pow( wm.self().playerType().kickableArea()
                                 - wm.self().vel().r() * ServerParam::i().playerRand()
                                 - wm.ball().vel().r() * ServerParam::i().ballRand()
                                 - 0.15,
                                 2 );
    Vector2D self_next = wm.self().pos() + wm.self().vel();
    Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    if ( self_next.dist2( ball_next ) > kickable2 )
    {
        // unkickable if turn is performed.
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) unkickable at next cycle" );
        return true;
    }

    return false;
}


/*-------------------------------------------------------------------*/
/*!

 */
Bhv_PassKickFindReceiver::Bhv_PassKickFindReceiver( const ActionChainGraph & chain_graph )
    : M_chain_graph( chain_graph )
{
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionPassKickFindReceiver::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const AbstractPlayerObject * receiver = wm.ourPlayer( M_receiver_unum );

    if ( ! receiver )
    {
        return false;
    }

    const Vector2D next_self_pos = agent->effector().queuedNextMyPos();
    //const AngleDeg next_self_body = agent->effector().queuedNextMyBody();
    const double next_view_width = agent->effector().queuedNextViewWidth().width() * 0.5;

    const Vector2D receiver_pos = receiver->pos() + receiver->vel();
    const AngleDeg receiver_angle = ( receiver_pos - next_self_pos ).th();

    Vector2D face_point = ( receiver_pos + M_receive_point ) * 0.5;
    AngleDeg face_angle = ( face_point - next_self_pos ).th();

    double rate = 0.5;
    while ( ( face_angle - receiver_angle ).abs() > next_view_width - 10.0 )
    {
        rate += 0.1;
        face_point
            = receiver_pos * rate
            + M_receive_point * ( 1.0 - rate );
        face_angle = ( face_point - next_self_pos ).th();

        if ( rate > 0.999 )
        {
            break;
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (intentin) facePoint=(%.1f %.1f) faceAngle=%.1f",
                  face_point.x, face_point.y,
                  face_angle.degree() );
    agent->debugClient().addMessage( "IntentionTurnToReceiver%.0f",
                                     face_angle.degree() );
    Body_TurnToPoint( face_point ).execute( agent );
    agent->setNeckAction( new Neck_TurnToPoint( face_point ) );

    return true;
}



/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PassKickFindReceiver::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_PassKickFindReceiver" );

    const WorldModel & wm = agent->world();

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": no kickable" );
        return false;
    }

    //
    // validate the pass
    //
    const CooperativeAction & pass = M_chain_graph.getFirstAction();

    if ( pass.category() != CooperativeAction::Pass )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": action(%d) is not a pass type.",
                      pass.category() );
        return false;
    }

    const AbstractPlayerObject * receiver = wm.ourPlayer( pass.targetPlayerUnum() );

    if ( ! receiver )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": NULL receiver." );

        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": pass receiver unum=%d (%.1f %.1f)",
                  receiver->unum(),
                  receiver->pos().x, receiver->pos().y );

    if ( wm.self().unum() == receiver->unum() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": receiver is my self." );
        return false;
    }

    // if ( receiver->isGhost() )
    // {
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__": receiver is a ghost." );
    //     return false;
    // }

    if ( wm.gameMode().type() != GameMode::PlayOn )
    {
        doPassKick( agent, pass );
        return true;
    }

    //
    // estimate kick step
    //
    int kick_step = pass.kickCount(); //kickStep( wm, pass );

    if ( kick_step == 1
         //&& pass.targetPoint().x > -20.0
         && receiver->seenPosCount() <= 3 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": 1 step kick." );
        doPassKick( agent, pass );
        return true;
    }

    //
    // trying to turn body and/or neck to the pass receiver
    //

    if ( doCheckReceiver( agent, pass ) )
    {
        agent->debugClient().addCircle( wm.self().pos(), 3.0 );
        agent->debugClient().addCircle( wm.self().pos(), 5.0 );
        agent->debugClient().addCircle( wm.self().pos(), 10.0 );

        return true;
    }

    if ( ( kick_step == 1
           && receiver->seenPosCount() > 3 )
         || receiver->isGhost() )
    {
        agent->debugClient().addMessage( "Pass:FindHold2" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) hold ball." );

        Body_HoldBall( true,
                       pass.targetPoint(),
                       pass.targetPoint() ).execute( agent );
        doSayPass( agent, pass );
        agent->setNeckAction( new Neck_TurnToReceiver( M_chain_graph ) );
        return true;
    }

    //
    // pass kick
    //

    doPassKick( agent, pass );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PassKickFindReceiver::doPassKick( PlayerAgent * agent,
                                      const CooperativeAction & pass )
{
    agent->debugClient().setTarget( pass.targetPlayerUnum() );
    agent->debugClient().setTarget( pass.targetPoint() );
    dlog.addText( Logger::TEAM,
                  __FILE__" (Bhv_PassKickFindReceiver) pass to "
                  "%d receive_pos=(%.1f %.1f) speed=%.3f",
                  pass.targetPlayerUnum(),
                  pass.targetPoint().x, pass.targetPoint().y,
                  pass.firstBallSpeed() );

    if ( pass.kickCount() == 1
         || agent->world().gameMode().type() != GameMode::PlayOn )
    {
        Body_KickOneStep( pass.targetPoint(),
                          pass.firstBallSpeed() ).execute( agent );

    }
    else
    {
        Body_SmartKick( pass.targetPoint(),
                        pass.firstBallSpeed(),
                        pass.firstBallSpeed() * 0.96,
                        3 ).execute( agent );
    }
    agent->setNeckAction( new Neck_TurnToReceiver( M_chain_graph ) );
    doSayPass( agent, pass );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_PassKickFindReceiver::kickStep( const WorldModel & wm,
                                    const CooperativeAction & pass )
{
    Vector2D max_vel
        = KickTable::calc_max_velocity( ( pass.targetPoint() - wm.ball().pos() ).th(),
                                        wm.self().kickRate(),
                                        wm.ball().vel() );

    // dlog.addText( Logger::TEAM,
    //               __FILE__": (kickStep) maxSpeed=%.3f passSpeed=%.3f",
    //               max_vel.r(), pass.firstBallSpeed() );

    if ( max_vel.r2() >= std::pow( pass.firstBallSpeed(), 2 ) )
    {
        return 1;
    }

    if ( pass.firstBallSpeed() > 2.5 ) // Magic Number
    {
        return 3;
    }

    return 2;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PassKickFindReceiver::doCheckReceiver( PlayerAgent * agent,
                                           const CooperativeAction & pass )
{
    const WorldModel & wm = agent->world();

    double nearest_opp_dist = 65535.0;
    wm.getOpponentNearestTo( wm.ball().pos(), 10, &nearest_opp_dist );
    if ( nearest_opp_dist < 4.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckReceiver) exist near opponent. dist=%.2f",
                      nearest_opp_dist );
        return false;
    }

    const AbstractPlayerObject * receiver = wm.ourPlayer( pass.targetPlayerUnum() );

    if ( ! receiver )
    {
        return false;
    }

    if ( receiver->seenPosCount() == 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckReceiver) receiver already seen." );
        return false;
    }

    //
    // set view mode
    //
    agent->setViewAction( new View_Synch() );

    const double next_view_half_width = agent->effector().queuedNextViewWidth().width() * 0.5;
    const double view_min = ServerParam::i().minNeckAngle() - next_view_half_width + 10.0;
    const double view_max = ServerParam::i().maxNeckAngle() + next_view_half_width - 10.0;

    //
    // check if turn_neck is necessary or not
    //
    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D player_pos = receiver->pos() + receiver->vel();
    const AngleDeg player_angle = ( player_pos - next_self_pos ).th();

    const double angle_diff = ( player_angle - wm.self().body() ).abs();

    if ( angle_diff < view_min
         || view_max < angle_diff )
    {
        //
        // need turn
        //

        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckReceiver) need turn."
                      " angleDiff=%.1f viewRange=[%.1f %.1f]",
                      angle_diff, view_min, view_max );

        //
        // TODO: check turn moment?
        //


        //
        // stop the ball
        //

        const double next_ball_dist = next_self_pos.dist( wm.ball().pos() + wm.ball().vel() );
        const double noised_kickable_area = wm.self().playerType().kickableArea()
            - wm.ball().vel().r() * ServerParam::i().ballRand()
            - wm.self().vel().r() * ServerParam::i().playerRand()
            - 0.15;

        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckReceiver) next_ball_dist=%.3f"
                      " noised_kickable=%.3f(kickable=%.3f)",
                      next_ball_dist,
                      noised_kickable_area,
                      wm.self().playerType().kickableArea() );

        if ( next_ball_dist > noised_kickable_area )
        {
            if ( doKeepBall( agent, pass ) )
            {
                return true;
            }

            if ( Body_StopBall().execute( agent ) )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (doCheckReceiver) stop the ball" );
                agent->debugClient().addMessage( "PassKickFind:StopBall" );
                agent->debugClient().setTarget( receiver->unum() );

                return true;
            }

            dlog.addText( Logger::TEAM,
                          __FILE__": (doCheckReceiver) Could not stop the ball???" );
        }

        //
        // turn to receiver
        //

        doTurnBodyNeckToReceiver( agent, pass );
        return true;
    }

    //
    // can see the receiver without turn
    //
    dlog.addText( Logger::TEAM,
                  __FILE__": (doCheckReceiver) can see receiver[%d](%.2f %.2f)"
                  " angleDiff=%.1f viewRange=[%.1f %.1f]",
                  pass.targetPlayerUnum(),
                  player_pos.x, player_pos.y,
                  angle_diff, view_min, view_max );

    return false;

    // agent->debugClient().addMessage( "Pass:FindHold1" );
    // dlog.addText( Logger::TEAM,
    //               __FILE__": (doCheckReceiver) hold ball." );

    // Body_HoldBall( true,
    //                pass.targetPoint(),
    //                pass.targetPoint() ).execute( agent );
    // doSayPass( agent, pass );
    // agent->setNeckAction( new Neck_TurnToReceiver() );

    // return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PassKickFindReceiver::doKeepBall( rcsc::PlayerAgent * agent,
                                      const CooperativeAction & pass )
{
    Vector2D ball_vel = getKeepBallVel( agent->world() );

    if ( ! ball_vel.isValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKeepBall) no candidate." );

        return false;
    }

    //
    // perform first kick
    //

    Vector2D kick_accel = ball_vel - agent->world().ball().vel();
    double kick_power = kick_accel.r() / agent->world().self().kickRate();
    AngleDeg kick_angle = kick_accel.th() - agent->world().self().body();

    if ( kick_power > ServerParam::i().maxPower() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKeepBall) over kick power" );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doKeepBall) "
                  " ballVel=(%.2f %.2f)"
                  " kickPower=%.1f kickAngle=%.1f",
                  ball_vel.x, ball_vel.y,
                  kick_power,
                  kick_angle.degree() );

    agent->debugClient().addMessage( "PassKickFind:KeepBall" );
    agent->debugClient().setTarget( pass.targetPlayerUnum() );
    agent->debugClient().setTarget( agent->world().ball().pos()
                                    + ball_vel
                                    + ball_vel * ServerParam::i().ballDecay() );

    agent->doKick( kick_power, kick_angle );
    agent->setNeckAction( new Neck_TurnToReceiver( M_chain_graph ) );

    //
    // set turn intention
    //

    dlog.addText( Logger::TEAM,
                  __FILE__": (doKeepBall) register intention" );
    agent->setIntention( new IntentionPassKickFindReceiver( pass.targetPlayerUnum(),
                                                            pass.targetPoint() ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
rcsc::Vector2D
Bhv_PassKickFindReceiver::getKeepBallVel( const rcsc::WorldModel & wm )
{
#ifdef __APPLE__
    static GameTime s_update_time( 0, 0 );
    static Vector2D s_best_ball_vel( 0.0, 0.0 );
#else
    static thread_local GameTime s_update_time( 0, 0 );
    static thread_local Vector2D s_best_ball_vel( 0.0, 0.0 );
#endif

    if ( s_update_time == wm.time() )
    {
        return s_best_ball_vel;
    }
    s_update_time = wm.time();

    //
    //
    //

    const int ANGLE_DIVS = 12;

    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();
    const double collide_dist2 = std::pow( ptype.playerSize()
                                           + SP.ballSize(),
                                           2 );
    const double keep_dist = ptype.playerSize()
        + ptype.kickableMargin() * 0.5
        + ServerParam::i().ballSize();

    const Vector2D next_self_pos
        = wm.self().pos() + wm.self().vel();
    const Vector2D next2_self_pos
        = next_self_pos
        + wm.self().vel() * ptype.playerDecay();

    //
    // create keep target point
    //

    Vector2D best_ball_vel = Vector2D::INVALIDATED;
    int best_opponent_step = 0;
    double best_ball_speed = 1000.0;


    for ( int a = 0; a < ANGLE_DIVS; ++a )
    {
        Vector2D keep_pos
            = next2_self_pos
            + Vector2D::from_polar( keep_dist,
                                    360.0/ANGLE_DIVS * a );
        if ( keep_pos.absX() > SP.pitchHalfLength() - 0.2
             || keep_pos.absY() > SP.pitchHalfWidth() - 0.2 )
        {
            continue;
        }

        Vector2D ball_move = keep_pos - wm.ball().pos();
        double ball_speed = ball_move.r() / ( 1.0 + SP.ballDecay() );

        Vector2D max_vel
            = KickTable::calc_max_velocity( ball_move.th(),
                                            wm.self().kickRate(),
                                            wm.ball().vel() );
        if ( max_vel.r2() < std::pow( ball_speed, 2 ) )
        {
            continue;
        }

        Vector2D ball_next_next = keep_pos;

        Vector2D ball_vel = ball_move.setLengthVector( ball_speed );
        Vector2D ball_next = wm.ball().pos() + ball_vel;

        if ( next_self_pos.dist2( ball_next ) < collide_dist2 )
        {
            ball_next_next = ball_next;
            ball_next_next += ball_vel * ( SP.ballDecay() * -0.1 );
        }

#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "(getKeepBallVel) %d: ball_move th=%.1f speed=%.2f max=%.2f",
                      a,
                      ball_move.th().degree(),
                      ball_speed,
                      max_vel.r() );
        dlog.addText( Logger::TEAM,
                      "__ ball_next=(%.2f %.2f) ball_next2=(%.2f %.2f)",
                      ball_next.x, ball_next.y,
                      ball_next_next.x, ball_next_next.y );
#endif

        //
        // check opponent
        //

        int min_step = 1000;
        for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
              o != wm.opponentsFromSelf().end();
              ++o )
        {
            if ( (*o)->distFromSelf() > 10.0 )
            {
                break;
            }

            int o_step = FieldAnalyzer::predict_player_reach_cycle( *o,
                                                                    ball_next_next,
                                                                    (*o)->playerTypePtr()->kickableArea(),
                                                                    0.0, // penalty distance
                                                                    1, // body count thr
                                                                    1, // default turn step
                                                                    0, // wait cycle
                                                                    true );

            if ( o_step <= 0 )
            {
                break;
            }

            if ( o_step < min_step )
            {
                min_step = o_step;
            }
        }
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      __FILE__": (getKeepBallVel) %d: keepPos=(%.2f %.2f)"
                      " ballNext2=(%.2f %.2f) ballVel=(%.2f %.2f) speed=%.2f o_step=%d",
                      a,
                      keep_pos.x, keep_pos.y,
                      ball_next_next.x, ball_next_next.y,
                      ball_vel.x, ball_vel.y,
                      ball_speed,
                      min_step );
#endif
        if ( min_step > best_opponent_step )
        {
            best_ball_vel = ball_vel;
            best_opponent_step = min_step;
            best_ball_speed = ball_speed;
        }
        else if ( min_step == best_opponent_step )
        {
            if ( best_ball_speed > ball_speed )
            {
                best_ball_vel = ball_vel;
                best_opponent_step = min_step;
                best_ball_speed = ball_speed;
            }
        }
    }

    s_best_ball_vel = best_ball_vel;
    return s_best_ball_vel;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PassKickFindReceiver::doTurnBodyNeckToReceiver( PlayerAgent * agent,
                                                    const CooperativeAction & pass )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * receiver = wm.ourPlayer( pass.targetPlayerUnum() );

    const double next_view_half_width = agent->effector().queuedNextViewWidth().width() * 0.5;
    const double view_min = SP.minNeckAngle() - next_view_half_width + 10.0;
    const double view_max = SP.maxNeckAngle() + next_view_half_width - 10.0;

    //
    // create candidate body target points
    //

    std::vector< Vector2D > body_points;
    body_points.reserve( 16 );

    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D receiver_pos = receiver->pos() + receiver->vel();
    const AngleDeg receiver_angle = ( receiver_pos - next_self_pos ).th();

    const double max_x = SP.pitchHalfLength() - 7.0;
    const double min_x = ( max_x - 10.0 < next_self_pos.x
                           ? max_x - 10.0
                           : next_self_pos.x );
    const double max_y = std::max( SP.pitchHalfLength() - 5.0,
                                   receiver_pos.absY() );
    const double y_step = std::max( 3.0, max_y / 5.0 );
    const double y_sign = sign( receiver_pos.y );

    // on the static x line (x = max_x)
    for ( double y = 0.0; y < max_y + 0.001; y += y_step )
    {
        body_points.push_back( Vector2D( max_x, y * y_sign ) );
    }

    // on the static y line (y == receiver_pos.y)
    for ( double x_rate = 0.9; x_rate >= 0.0; x_rate -= 0.1 )
    {
        double x = std::min( max_x,
                             max_x * x_rate + min_x * ( 1.0 - x_rate ) );
        body_points.push_back( Vector2D( x, receiver_pos.y ) );
    }

    //
    // evaluate candidate points
    //

    const double max_turn
        = wm.self().playerType().effectiveTurn( SP.maxMoment(),
                                                wm.self().vel().r() );
    Vector2D best_point = Vector2D::INVALIDATED;
    double min_turn = 360.0;

    const std::vector< Vector2D >::const_iterator p_end = body_points.end();
    for ( std::vector< Vector2D >::const_iterator p = body_points.begin();
          p != p_end;
          ++p )
    {
        AngleDeg target_body_angle = ( *p - next_self_pos ).th();
        double turn_moment_abs = ( target_body_angle - wm.self().body() ).abs();

#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "____ body_point=(%.1f %.1f) angle=%.1f moment=%.1f",
                      p->x, p->y,
                      target_body_angle.degree(),
                      turn_moment_abs );
#endif

        double angle_diff = ( receiver_angle - target_body_angle ).abs();
        if ( view_min < angle_diff
             && angle_diff < view_max )
        {
            if ( turn_moment_abs < max_turn )
            {
                best_point = *p;
#ifdef DEBUG_PRINT
                dlog.addText( Logger::TEAM,
                              "____ oooo can turn and look" );
#endif
                break;
            }

            if ( turn_moment_abs < min_turn )
            {
                best_point = *p;
                min_turn = turn_moment_abs;
#ifdef DEBUG_PRINT
                dlog.addText( Logger::TEAM,
                              "____ ---- update candidate point min_turn=%.1f",
                              min_turn );
#endif
            }
        }
#ifdef DEBUG_PRINT
        else
        {
            dlog.addText( Logger::TEAM,
                          "____ xxxx cannot look" );
        }
#endif
    }

    if ( ! best_point.isValid() )
    {
        best_point = pass.targetPoint();
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      __FILE__": (doTurnBodyNeckToPoint) could not find the target point." );
#endif
    }

    //
    // perform the action
    //
    agent->debugClient().addMessage( "PassKickFind:Turn" );
    agent->debugClient().setTarget( receiver->unum() );

    dlog.addText( Logger::TEAM,
                  __FILE__": (doTurnBodyNeckToReceiver)"
                  " receiver=%d receivePos=(%.2f %.2f)"
                  " turnTo=(%.2f %.2f)",
                  pass.targetPlayerUnum(),
                  pass.targetPoint().x, pass.targetPoint().y,
                  best_point.x, best_point.y );
    agent->debugClient().addLine( next_self_pos, best_point );

    Body_TurnToPoint( best_point ).execute( agent );
    doSayPass( agent, pass );
    agent->setNeckAction( new Neck_TurnToReceiver( M_chain_graph ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_PassKickFindReceiver::doSayPass( PlayerAgent * agent,
                                     const CooperativeAction & pass )
{
    const int receiver_unum = pass.targetPlayerUnum();
    const Vector2D & receive_pos = pass.targetPoint();

    if ( agent->config().useCommunication()
         && receiver_unum != Unum_Unknown
         && ! agent->effector().queuedNextBallKickable()
         )
    {
        const AbstractPlayerObject * receiver = agent->world().ourPlayer( receiver_unum );
        if ( ! receiver )
        {
            return;
        }

        dlog.addText( Logger::ACTION | Logger::TEAM,
                      __FILE__": (doSayPass) set pass communication." );

        Vector2D target_buf( 0.0, 0.0 );

        agent->debugClient().addMessage( "Say:pass" );

        Vector2D ball_vel( 0.0, 0.0 );
        if ( ! agent->effector().queuedNextBallKickable() )
        {
            ball_vel = agent->effector().queuedNextBallVel();
        }

        agent->addSayMessage( new PassMessage( receiver_unum,
                                               receive_pos + target_buf,
                                               agent->effector().queuedNextBallPos(),
                                               ball_vel ) );
    }
}
