// -*-c++-*-

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

#include "bhv_goalie_free_kick.h"

#include "bhv_goalie_basic_move.h"

#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/body_pass.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/rect_2d.h>

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_GoalieFreeKick::execute( rcsc::PlayerAgent * agent )
{
#ifdef __APPLE__
    static bool s_first_move = false;
    static bool s_second_move = false;
    static int s_second_wait_count = 0;
#else
    static thread_local bool s_first_move = false;
    static thread_local bool s_second_move = false;
    static thread_local int s_second_wait_count = 0;
#endif

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhf_GoalieFreeKick" );
    if ( agent->world().gameMode().type() != rcsc::GameMode::GoalieCatch_
         || agent->world().gameMode().side() != agent->world().ourSide()
         || ! agent->world().self().isKickable() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": Bhv_GoalieFreeKick. Not a goalie catch mode" );

        Bhv_GoalieBasicMove().execute( agent );
        return true;
    }


    const long time_diff
        = agent->world().time().cycle()
        - agent->effector().getCatchTime().cycle();
    //- M_catch_time.cycle();

    // reset flags & wait
    if ( time_diff <= 2 )
    {
        s_first_move = false;
        s_second_move = false;
        s_second_wait_count = 0;

        doWait( agent );
        return true;
    }

    // first move
    if ( ! s_first_move )
    {
        //rcsc::Vector2D move_target( rcsc::ServerParam::i().ourPenaltyAreaLine() - 0.8, 0.0 );
        rcsc::Vector2D move_target( rcsc::ServerParam::i().ourPenaltyAreaLineX() - 1.5,
                                    agent->world().ball().pos().y > 0.0 ? -13.0 : 13.0 );
        //rcsc::Vector2D move_target( -45.0, 0.0 );
        s_first_move = true;
        s_second_move = false;
        s_second_wait_count = 0;
        agent->doMove( move_target.x, move_target.y );
        agent->setNeckAction( new rcsc::Neck_ScanField );
        return true;
    }

    // after first move
    // check stamina recovery or wait teammate
    rcsc::Rect2D our_pen( rcsc::Vector2D( -52.5, -40.0 ),
                          rcsc::Vector2D( -36.0, 40.0 ) );
    if ( time_diff < 50
         || agent->world().setplayCount() < 3
         || ( time_diff < rcsc::ServerParam::i().dropBallTime() - 15
              && ( agent->world().self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.9
                   || agent->world().existTeammateIn( our_pen, 20, true )
                   )
              )
         )
    {
        doWait( agent );
        return true;
    }

    // second move
    if ( ! s_second_move )
    {
        rcsc::Vector2D kick_point = getKickPoint( agent );
        agent->doMove( kick_point.x, kick_point.y );
        agent->setNeckAction( new rcsc::Neck_ScanField );
        s_second_move = true;
        s_second_wait_count = 0;
        return true;
    }

    s_second_wait_count++;

    // after second move
    // wait see info
    if ( s_second_wait_count < 5
         || agent->world().seeTime() != agent->world().time() )
    {
        doWait( agent );
        return true;
    }

    s_first_move = false;
    s_second_move = false;
    s_second_wait_count = 0;

    // register kick intention
    doKick( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
rcsc::Vector2D
Bhv_GoalieFreeKick::getKickPoint( const rcsc::PlayerAgent * agent )
{
    static const double base_x = -43.0;
    static const double base_y = 10.0;

    std::vector< std::pair< rcsc::Vector2D, double > > candidates;
    candidates.reserve( 4 );
    candidates.push_back( std::make_pair( rcsc::Vector2D( base_x, base_y ), 0.0 ) );
    candidates.push_back( std::make_pair( rcsc::Vector2D( base_x, -base_y ), 0.0 ) );
    candidates.push_back( std::make_pair( rcsc::Vector2D( base_x, 0.0 ), 0.0 ) );

    const rcsc::PlayerPtrCont::const_iterator opps_end
        = agent->world().opponentsFromSelf().end();
    for ( rcsc::PlayerPtrCont::const_iterator o
              = agent->world().opponentsFromSelf().begin();
          o != opps_end;
          ++o )
    {
        for ( std::vector< std::pair< rcsc::Vector2D, double > >::iterator it = candidates.begin();
              it != candidates.end();
              ++it )
        {
            it->second += 1.0 / (*o)->pos().dist2( it->first );
        }
    }

    rcsc::Vector2D best_pos = candidates.front().first;
    double min_cong = 10000.0;
    for ( std::vector< std::pair< rcsc::Vector2D, double > >::iterator it = candidates.begin();
          it != candidates.end();
          ++it )
    {
        if ( it->second < min_cong )
        {
            best_pos = it->first;
            min_cong = it->second;
        }
    }

    return best_pos;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_GoalieFreeKick::doKick( rcsc::PlayerAgent * agent )
{
    rcsc::Vector2D target_point;
    double pass_speed = 0.0;

    if  ( rcsc::Body_Pass::get_best_pass( agent->world(), &target_point, &pass_speed, NULL )
          && target_point.dist( rcsc::Vector2D( -50.0, 0.0 ) ) > 20.0 )
    {
        double opp_dist = 100.0;
        const rcsc::PlayerObject * opp
            = agent->world().getOpponentNearestTo( target_point, 20, &opp_dist );
        agent->debugClient().addMessage( "GKickOppDist%.1f", opp ? opp_dist : 1000.0 );
        if ( ! opp || opp_dist > 3.0 )
        {
            rcsc::Body_KickOneStep( target_point,
                                    pass_speed ).execute( agent );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": register goalie kick intention. to (%.1f, %.1f)",
                                target_point.x,
                                target_point.y );
            agent->setNeckAction( new rcsc::Neck_ScanField() );
            return;
        }
    }

    rcsc::Body_ClearBall().execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_GoalieFreeKick::doWait( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();
    rcsc::Vector2D face_target( 0.0, 0.0 );

    if ( wm.self().pos().x > rcsc::ServerParam::i().ourPenaltyAreaLineX()
         - rcsc::ServerParam::i().ballSize()
         - wm.self().playerType().playerSize()
         - 0.5 )
    {
        face_target.assign( rcsc::ServerParam::i().ourPenaltyAreaLineX() - 1.0,
                            0.0 );
    }

    rcsc::Body_TurnToPoint( face_target ).execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );
}
