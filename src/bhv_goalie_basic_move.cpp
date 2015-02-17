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

#include "bhv_goalie_basic_move.h"

#include "bhv_basic_tackle.h"
#include "neck_goalie_turn_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_stop_dash.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/line_2d.h>
#include <rcsc/soccer_math.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_GoalieBasicMove::execute( PlayerAgent * agent )
{
    const Vector2D move_point = getTargetPoint( agent );

    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_GoalieBasicMove. move_point(%.2f %.2f)",
                  move_point.x, move_point.y );

    //////////////////////////////////////////////////////////////////
    // tackle
    if ( Bhv_BasicTackle( 0.8, 90.0 ).execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": tackle" );
        return true;
    }

    /////////////////////////////////////////////////////////////
    //----------------------------------------------------------
    if ( doPrepareDeepCross( agent, move_point ) )
    {
        // face to opponent side to wait the opponent last cross pass
        return true;
    }
    //----------------------------------------------------------
    // check distance to the move target point
    // if already there, try to stop
    if ( doStopAtMovePoint( agent, move_point ) )
    {
        // execute stop action
        return true;
    }
    //----------------------------------------------------------
    // check whether ball is in very dangerous state
    if ( doMoveForDangerousState( agent, move_point ) )
    {
        // execute emergency action
        return true;
    }
    //----------------------------------------------------------
    // check & correct X difference
    if ( doCorrectX( agent, move_point ) )
    {
        // execute x-pos adjustment action
        return true;
    }

    //----------------------------------------------------------
    if ( doCorrectBodyDir( agent, move_point, true ) ) // consider opp
    {
        // exeucte turn
        return true;
    }

    //----------------------------------------------------------
    if ( doGoToMovePoint( agent, move_point ) )
    {
        // mainly execute Y-adjustment if body direction is OK. -> only dash
        // if body direction is not good, nomal go to action is done.
        return true;
    }

    //----------------------------------------------------------
    // change my body angle to desired angle
    if ( doCorrectBodyDir( agent, move_point, false ) ) // not consider opp
    {
        return true;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": only look ball" );
    agent->debugClient().addMessage( "OnlyTurnNeck" );

    agent->doTurn( 0.0 );
    agent->setNeckAction( new Neck_GoalieTurnNeck() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_GoalieBasicMove::getTargetPoint( PlayerAgent * agent )
{
    const double base_move_x = -49.8;
    const double danger_move_x = -51.5;
    const WorldModel & wm = agent->world();

    int ball_reach_step = 0;
    if ( ! wm.existKickableTeammate()
         && ! wm.existKickableOpponent() )
    {
        ball_reach_step
            = std::min( wm.interceptTable()->teammateReachCycle(),
                        wm.interceptTable()->opponentReachCycle() );
    }
    const Vector2D base_pos = wm.ball().inertiaPoint( ball_reach_step );


    //---------------------------------------------------------//
    // angle is very dangerous
    if ( base_pos.y > ServerParam::i().goalHalfWidth() + 3.0 )
    {
        Vector2D right_pole( - ServerParam::i().pitchHalfLength(),
                             ServerParam::i().goalHalfWidth() );
        AngleDeg angle_to_pole = ( right_pole - base_pos ).th();

        if ( -140.0 < angle_to_pole.degree()
             && angle_to_pole.degree() < -90.0 )
        {
            agent->debugClient().addMessage( "RPole" );
            return Vector2D( danger_move_x, ServerParam::i().goalHalfWidth() + 0.001 );
        }
    }
    else if ( base_pos.y < -ServerParam::i().goalHalfWidth() - 3.0 )
    {
        Vector2D left_pole( - ServerParam::i().pitchHalfLength(),
                            - ServerParam::i().goalHalfWidth() );
        AngleDeg angle_to_pole = ( left_pole - base_pos ).th();

        if ( 90.0 < angle_to_pole.degree()
             && angle_to_pole.degree() < 140.0 )
        {
            agent->debugClient().addMessage( "LPole" );
            return Vector2D( danger_move_x, - ServerParam::i().goalHalfWidth() - 0.001 );
        }
    }

    //---------------------------------------------------------//
    // ball is close to goal line
    if ( base_pos.x < -ServerParam::i().pitchHalfLength() + 8.0
         && base_pos.absY() > ServerParam::i().goalHalfWidth() + 2.0 )
    {
        Vector2D target_point( base_move_x, ServerParam::i().goalHalfWidth() - 0.1 );
        if ( base_pos.y < 0.0 )
        {
            target_point.y *= -1.0;
        }

        dlog.addText( Logger::TEAM,
                      __FILE__": getTarget. target is goal pole" );
        agent->debugClient().addMessage( "Pos(1)" );

        return target_point;
    }

//---------------------------------------------------------//
    {
        const double x_back = 7.0; // tune this!!
        int ball_pred_cycle = 5; // tune this!!
        const double y_buf = 0.5; // tune this!!
        const Vector2D base_point( - ServerParam::i().pitchHalfLength() - x_back,
                                   0.0 );
        Vector2D ball_point;
        if ( wm.existKickableOpponent() )
        {
            ball_point = base_pos;
            agent->debugClient().addMessage( "Pos(2)" );
        }
        else
        {
            int opp_min = wm.interceptTable()->opponentReachCycle();
            if ( opp_min < ball_pred_cycle )
            {
                ball_pred_cycle = opp_min;
                dlog.addText( Logger::TEAM,
                              __FILE__": opp may reach near future. cycle = %d",
                              opp_min );
            }

            ball_point
                = inertia_n_step_point( base_pos,
                                        wm.ball().vel(),
                                        ball_pred_cycle,
                                        ServerParam::i().ballDecay() );
            agent->debugClient().addMessage( "Pos(3)" );
        }

        if ( ball_point.x < base_point.x + 0.1 )
        {
            ball_point.x = base_point.x + 0.1;
        }

        Line2D ball_line( ball_point, base_point );
        double move_y = ball_line.getY( base_move_x );

        if ( move_y > ServerParam::i().goalHalfWidth() - y_buf )
        {
            move_y = ServerParam::i().goalHalfWidth() - y_buf;
        }
        if ( move_y < - ServerParam::i().goalHalfWidth() + y_buf )
        {
            move_y = - ServerParam::i().goalHalfWidth() + y_buf;
        }

        return Vector2D( base_move_x, move_y );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_GoalieBasicMove::getBasicDashPower( PlayerAgent * agent,
                                        const Vector2D & move_point )
{
    const WorldModel & wm = agent->world();
    const PlayerType & mytype = wm.self().playerType();

    const double my_inc = mytype.staminaIncMax() * wm.self().recovery();

    if ( std::fabs( wm.self().pos().x - move_point.x ) > 3.0 )
    {
        return ServerParam::i().maxDashPower();
    }

    if ( wm.ball().pos().x > -30.0 )
    {
        if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.9 )
        {
            return my_inc * 0.5;
        }
        agent->debugClient().addMessage( "P1" );
        return my_inc;
    }
    else if ( wm.ball().pos().x > ServerParam::i().ourPenaltyAreaLineX() )
    {
        if ( wm.ball().pos().absY() > 20.0 )
        {
            // penalty area
            agent->debugClient().addMessage( "P2" );
            return my_inc;
        }
        if ( wm.ball().vel().x > 1.0 )
        {
            // ball is moving to opponent side
            agent->debugClient().addMessage( "P2.5" );
            return my_inc * 0.5;
        }

        int opp_min = wm.interceptTable()->opponentReachCycle();
        if ( opp_min <= 3 )
        {
            agent->debugClient().addMessage( "P2.3" );
            return ServerParam::i().maxDashPower();
        }

        if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.7 )
        {
            agent->debugClient().addMessage( "P2.6" );
            return my_inc * 0.7;
        }
        agent->debugClient().addMessage( "P3" );
        return ServerParam::i().maxDashPower() * 0.6;
    }
    else
    {
        if ( wm.ball().pos().absY() < 15.0
             || wm.ball().pos().y * wm.self().pos().y < 0.0 ) // opposite side
        {
            agent->debugClient().addMessage( "P4" );
            return ServerParam::i().maxDashPower();
        }
        else
        {
            agent->debugClient().addMessage( "P5" );
            return my_inc;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_GoalieBasicMove::doPrepareDeepCross( PlayerAgent * agent,
                                         const Vector2D & move_point )
{
    if ( move_point.absY() < ServerParam::i().goalHalfWidth() - 0.8 )
    {
        // consider only very deep cross
        dlog.addText( Logger::TEAM,
                      __FILE__": doPrepareDeepCross no deep cross" );
        return false;
    }

    const WorldModel & wm = agent->world();

    const Vector2D goal_c( - ServerParam::i().pitchHalfLength(), 0.0 );

    Vector2D goal_to_ball = wm.ball().pos() - goal_c;

    if ( goal_to_ball.th().abs() < 60.0 )
    {
        // ball is not in side cross area
        dlog.addText( Logger::TEAM,
                      __FILE__": doPrepareDeepCross.ball is not in side cross area" );
        return false;
    }

    Vector2D my_inertia = wm.self().inertiaFinalPoint();
    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;
    //double dist_thr = 0.5;

    if ( my_inertia.dist( move_point ) > dist_thr )
    {
        // needed to go to move target point
        double dash_power = getBasicDashPower( agent, move_point );
        dlog.addText( Logger::TEAM,
                      __FILE__": doPrepareDeepCross. need to move. power=%.1f",
                      dash_power );
        agent->debugClient().addMessage( "DeepCrossMove%.0f", dash_power );
        agent->debugClient().setTarget( move_point );
        agent->debugClient().addCircle( move_point, dist_thr );

        doGoToPointLookBall( agent,
                             move_point,
                             wm.ball().angleFromSelf(),
                             dist_thr,
                             dash_power );
        return true;
    }

    AngleDeg body_angle = ( wm.ball().pos().y < 0.0
                            ? 10.0
                            : -10.0 );
    agent->debugClient().addMessage( "PrepareCross" );
    dlog.addText( Logger::TEAM,
                  __FILE__": doPrepareDeepCross  body angle = %.1f  move_point(%.1f %.1f)",
                  body_angle.degree(),
                  move_point.x, move_point.y );
    agent->debugClient().setTarget( move_point );

    Body_TurnToAngle( body_angle ).execute( agent );
    agent->setNeckAction( new Neck_GoalieTurnNeck() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_GoalieBasicMove::doStopAtMovePoint( PlayerAgent * agent,
                                        const Vector2D & move_point )
{
    //----------------------------------------------------------
    // already exist at target point
    // but inertia movement is big
    // stop dash

    const WorldModel & wm = agent->world();
    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    // now, in the target area
    if ( wm.self().pos().dist( move_point ) < dist_thr )
    {
        const Vector2D my_final
            = inertia_final_point( wm.self().pos(),
                                   wm.self().vel(),
                                   wm.self().playerType().playerDecay() );
        // after inertia move, can stay in the target area
        if ( my_final.dist( move_point ) < dist_thr )
        {
            agent->debugClient().addMessage( "InertiaStay" );
            dlog.addText( Logger::TEAM,
                          __FILE__": doStopAtMovePoint. inertia stay" );
            return false;
        }

        // try to stop at the current point
        dlog.addText( Logger::TEAM,
                      __FILE__": doStopAtMovePoint. stop dash" );
        agent->debugClient().addMessage( "Stop" );
        agent->debugClient().setTarget( move_point );

        Body_StopDash( true ).execute( agent ); // save recovery
        agent->setNeckAction( new Neck_GoalieTurnNeck() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_GoalieBasicMove::doMoveForDangerousState( PlayerAgent * agent,
                                              const Vector2D & move_point )
{
    const WorldModel& wm = agent->world();

    const double x_buf = 0.5;

    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    dlog.addText( Logger::TEAM,
                  __FILE__": doMoveForDangerousState" );

    if ( std::fabs( move_point.x - wm.self().pos().x ) > x_buf
         && ball_next.x < -ServerParam::i().pitchHalfLength() + 11.0
         && ball_next.absY() < ServerParam::i().goalHalfWidth() + 1.0 )
    {
        // x difference to the move point is over threshold
        // but ball is in very dangerous area (just front of our goal)

        // and, exist opponent close to ball
        if ( ! wm.opponentsFromBall().empty()
             && wm.opponentsFromBall().front()->distFromBall() < 2.0 )
        {
            Vector2D block_point
                = wm.opponentsFromBall().front()->pos();
            block_point.x -= 2.5;
            block_point.y = move_point.y;

            if ( wm.self().pos().x < block_point.x )
            {
                block_point.x = wm.self().pos().x;
            }

            dlog.addText( Logger::TEAM,
                          __FILE__": block opponent kickaer" );
            agent->debugClient().addMessage( "BlockOpp" );

            if ( doGoToMovePoint( agent, block_point ) )
            {
                return true;
            }

            double dist_thr = wm.ball().distFromSelf() * 0.1;
            if ( dist_thr < 0.5 ) dist_thr = 0.5;

            agent->debugClient().setTarget( block_point );
            agent->debugClient().addCircle( block_point, dist_thr );

            doGoToPointLookBall( agent,
                                 move_point,
                                 wm.ball().angleFromSelf(),
                                 dist_thr,
                                 ServerParam::i().maxDashPower() );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_GoalieBasicMove::doCorrectX( PlayerAgent * agent,
                                 const Vector2D & move_point )
{
    const WorldModel & wm = agent->world();

    const double x_buf = 0.5;

    dlog.addText( Logger::TEAM,
                  __FILE__": doCorrectX" );
    if ( std::fabs( move_point.x - wm.self().pos().x ) < x_buf )
    {
        // x difference is already small.
        dlog.addText( Logger::TEAM,
                      __FILE__": doCorrectX. x diff is small" );
        return false;
    }

    int opp_min_cyc = wm.interceptTable()->opponentReachCycle();
    if ( ( ! wm.existKickableOpponent() && opp_min_cyc >= 4 )
         || wm.ball().distFromSelf() > 18.0 )
    {
        double dash_power = getBasicDashPower( agent, move_point );

        dlog.addText( Logger::TEAM,
                      __FILE__": doCorrectX. power=%.1f",
                      dash_power );
        agent->debugClient().addMessage( "CorrectX%.0f", dash_power );
        agent->debugClient().setTarget( move_point );
        agent->debugClient().addCircle( move_point, x_buf );

        if ( ! wm.existKickableOpponent()
             && wm.ball().distFromSelf() > 30.0 )
        {
            if ( ! Body_GoToPoint( move_point, x_buf, dash_power
                                   ).execute( agent ) )
            {
                AngleDeg body_angle = ( wm.self().body().degree() > 0.0
                                        ? 90.0
                                        : -90.0 );
                Body_TurnToAngle( body_angle ).execute( agent );

            }
            agent->setNeckAction( new Neck_TurnToBall() );
            return true;
        }

        doGoToPointLookBall( agent,
                             move_point,
                             wm.ball().angleFromSelf(),
                             x_buf,
                             dash_power );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_GoalieBasicMove::doCorrectBodyDir( PlayerAgent * agent,
                                       const Vector2D & move_point,
                                       const bool consider_opp )
{
    // adjust only body direction

    const WorldModel & wm = agent->world();

    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    const AngleDeg target_angle = ( ball_next.y < 0.0 ? -90.0 : 90.0 );
    const double angle_diff = ( wm.self().body() - target_angle ).abs();

    dlog.addText( Logger::TEAM,
                  __FILE__": doCorrectBodyDir" );

    if ( angle_diff < 5.0 )
    {
        return false;
    }

#if 1
    {
        const Vector2D goal_c( - ServerParam::i().pitchHalfLength(), 0.0 );
        Vector2D goal_to_ball = wm.ball().pos() - goal_c;
        if ( goal_to_ball.th().abs() >= 60.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": doCorrectBodyDir. danger area" );
            return false;
        }
    }
#else
    if ( wm.ball().pos().x < -36.0
         && wm.ball().pos().absY() < 15.0
         && wm.self().pos().dist( move_point ) > 1.5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": doCorrectBodyDir. danger area" );
        return false;
    }
#endif

    double opp_ball_dist
        = ( wm.opponentsFromBall().empty()
            ? 100.0
            : wm.opponentsFromBall().front()->distFromBall() );
    if ( ! consider_opp
         || opp_ball_dist > 7.0
         || wm.ball().distFromSelf() > 20.0
         || ( std::fabs( move_point.y - wm.self().pos().y ) < 1.0 // y diff
              && ! wm.existKickableOpponent() ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": body face to %.1f.  angle_diff=%.1f %s",
                      target_angle.degree(), angle_diff,
                      consider_opp ? "consider_opp" : "" );
        agent->debugClient().addMessage( "CorrectBody%s",
                                         consider_opp ? "WithOpp" : "" );
        Body_TurnToAngle( target_angle ).execute( agent );
        agent->setNeckAction( new Neck_GoalieTurnNeck() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_GoalieBasicMove::doGoToMovePoint( PlayerAgent * agent,
                                      const Vector2D & move_point )
{
    // move to target point
    // check Y coordinate difference

    const WorldModel & wm = agent->world();

    double dist_thr = wm.ball().distFromSelf() * 0.08;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    const double y_diff = std::fabs( move_point.y - wm.self().pos().y );
    if ( y_diff < dist_thr )
    {
        // already there
        dlog.addText( Logger::TEAM,
                      __FILE__": doGoToMovePoint. y_diff=%.2f < thr=%.2f",
                      y_diff, dist_thr );
        return false;
    }

    //----------------------------------------------------------//
    // dash to body direction

    double dash_power = getBasicDashPower( agent, move_point );

    // body direction is OK
    if ( std::fabs( wm.self().body().abs() - 90.0 ) < 7.0 )
    {
        // calc dash power only to reach the target point
        double required_power = y_diff / wm.self().dashRate();
        if ( dash_power > required_power )
        {
            dash_power = required_power;
        }

        if ( move_point.y > wm.self().pos().y )
        {
            if ( wm.self().body().degree() < 0.0 )
            {
                dash_power *= -1.0;
            }
        }
        else
        {
            if ( wm.self().body().degree() > 0.0 )
            {
                dash_power *= -1.0;
            }
        }

        dash_power = ServerParam::i().normalizeDashPower( dash_power );

        dlog.addText( Logger::TEAM,
                      __FILE__": doGoToMovePoint. CorrectY(1) power= %.1f",
                      dash_power );
        agent->debugClient().addMessage( "CorrectY(1)%.0f", dash_power );
        agent->debugClient().setTarget( move_point );

        agent->doDash( dash_power );
        agent->setNeckAction( new Neck_GoalieTurnNeck() );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": doGoToMovePoint. CorrectPos power= %.1f",
                      dash_power );
        agent->debugClient().addMessage( "CorrectPos%.0f", dash_power );
        agent->debugClient().setTarget( move_point );
        agent->debugClient().addCircle( move_point, dist_thr );

        doGoToPointLookBall( agent,
                             move_point,
                             wm.ball().angleFromSelf(),
                             dist_thr,
                             dash_power );
    }
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_GoalieBasicMove::doGoToPointLookBall( PlayerAgent * agent,
                                          const Vector2D & target_point,
                                          const AngleDeg & body_angle,
                                          const double & dist_thr,
                                          const double & dash_power,
                                          const double & back_power_rate )
{
    const WorldModel & wm = agent->world();

    if ( wm.gameMode().type() == GameMode::PlayOn
         || wm.gameMode().type() == GameMode::PenaltyTaken_ )
    {
        agent->debugClient().addMessage( "Goalie:GoToLook" );
        dlog.addText( Logger::TEAM,
                      __FILE__": doGoToPointLookBall. use GoToPointLookBall" );
        Bhv_GoToPointLookBall( target_point,
                               dist_thr,
                               dash_power,
                               back_power_rate
                               ).execute( agent );
    }
    else
    {
        agent->debugClient().addMessage( "Goalie:GoTo" );
        dlog.addText( Logger::TEAM,
                      __FILE__": doGoToPointLookBall. use GoToPoint" );
        if ( Body_GoToPoint( target_point, dist_thr, dash_power
                             ).execute( agent ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": doGoToPointLookBall. go" );
        }
        else
        {
            Body_TurnToAngle( body_angle ).execute( agent );
            dlog.addText( Logger::TEAM,
                          __FILE__": doGoToPointLookBall. turn to %.1f",
                          body_angle.degree() );
        }

        agent->setNeckAction( new Neck_TurnToBall() );
    }
}
