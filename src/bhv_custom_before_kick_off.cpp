// -*-c++-*-

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

#include "bhv_custom_before_kick_off.h"

#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/neck_turn_to_relative.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

// #define DEBUG_PRINT

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_CustomBeforeKickOff::execute( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    if ( wm.time().cycle() == 0
         && wm.time().stopped() < 5 )
    {
        agent->doTurn( 0.0 );
        agent->setNeckAction( new Neck_TurnToRelative( 0.0 ) );
        return false;
    }

    if ( wm.gameMode().type() == GameMode::AfterGoal_
         && wm.self().vel().r() > 0.05 )
    {
        agent->doTurn( 180.0 );
        agent->setNeckAction( new Neck_TurnToRelative( 0.0 ) );
        return true;
    }

    // check center circle
    SideID kickoff_side = NEUTRAL;

    if ( wm.gameMode().type() == GameMode::AfterGoal_ )
    {
        // after our goal
        if ( wm.gameMode().side() != wm.ourSide() )
        {
            kickoff_side = wm.ourSide();
        }
        else
        {
            kickoff_side = ( wm.ourSide() == LEFT
                             ? RIGHT
                             : LEFT );
        }
    }
    else // before_kick_off
    {
        // check half_time count
        if ( SP.halfTime() > 0 )
        {
            int half_time = SP.halfTime() * 10;
            int extra_half_time = SP.extraHalfTime() * 10;
            int normal_time = half_time * SP.nrNormalHalfs();
            int extra_time = extra_half_time * SP.nrExtraHalfs();
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION,
                          __FILE__": half_time=%d nr_normal_halfs=%d normal_time=%d",
                          half_time, SP.nrNormalHalfs(),
                          normal_time );
            dlog.addText( Logger::ACTION,
                          __FILE__": extra_half_time=%d nr_extra_halfs=%d extra_time=%d",
                          extra_half_time, SP.nrExtraHalfs(),
                          extra_time );
            dlog.addText( Logger::ACTION,
                          __FILE__": total_time=%d",
                          normal_time + extra_time );
#endif
            int time_flag = 0;

            if ( wm.time().cycle() <= normal_time )
            {
                time_flag = ( wm.time().cycle() / half_time ) % 2;
#ifdef DEBUG_PRINT
                dlog.addText( Logger::ACTION,
                              __FILE__": time=%d time/half_time=%d flag=%d",
                              wm.time().cycle(),
                              wm.time().cycle() / half_time,
                              time_flag );
#endif
            }
            else if ( wm.time().cycle() <= normal_time + extra_time )
            {
                int overtime = wm.time().cycle() - normal_time;
                time_flag = ( overtime / extra_half_time ) % 2;
#ifdef DEBUG_PRINT
                dlog.addText( Logger::ACTION,
                              __FILE__": overtime=%d overttime/extra_half_time=%d flag=%d",
                              overtime,
                              wm.time().cycle() / extra_half_time,
                              time_flag );
#endif
            }

            kickoff_side = ( time_flag == 0
                             ? LEFT
                             : RIGHT );
        }
        else
        {
            kickoff_side = LEFT;
        }
    }

    //
    // fix move target point
    //
#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION,
                  __FILE__": move_pos=(%.2f %.2f)",
                  M_move_point.x, M_move_point.y );
#endif
    if ( SP.kickoffOffside()
         && M_move_point.x >= 0.0 )
    {
        M_move_point.x = -0.001;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION,
                      __FILE__": avoid kick off offside" );
#endif
    }

    if ( kickoff_side != wm.ourSide()
         && M_move_point.r() < ServerParam::i().centerCircleR() + 0.1 )
    {
        M_move_point *= ( ServerParam::i().centerCircleR() + 0.5 ) / M_move_point.r();
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION,
                      __FILE__": avoid center circle. new pos=(%.2f %.2f)",
                      M_move_point.x, M_move_point.y );
#endif
    }

    // move
    double tmpr = ( M_move_point - wm.self().pos() ).r();
    if ( tmpr > 1.0 )
    {
        agent->doMove( M_move_point.x, M_move_point.y );
        agent->setNeckAction( new Neck_TurnToRelative( 0.0 ) );
        return true;
    }

    // field scan
    return Bhv_ScanField().execute( agent );
}
