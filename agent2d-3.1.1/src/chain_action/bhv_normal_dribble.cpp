// -*-c++-*-

/*!
  \file bhv_normal_dribble.cpp
  \brief normal dribble action that uses DribbleTable
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

#include "bhv_normal_dribble.h"

#include "action_chain_holder.h"
#include "action_chain_graph.h"
#include "cooperative_action.h"

#include "dribble.h"
#include "short_dribble_generator.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/server_param.h>
#include <rcsc/soccer_math.h>
#include <rcsc/math_util.h>

//#define DEBUG_PRINT

using namespace rcsc;

class IntentionNormalDribble
    : public SoccerIntention {
private:
    const Vector2D M_target_point; //!< trapped ball position

    int M_turn_step; //!< remained turn step
    int M_dash_step; //!< remained dash step

    GameTime M_last_execute_time; //!< last executed time

    NeckAction::Ptr M_neck_action;
    ViewAction::Ptr M_view_action;

public:

    IntentionNormalDribble( const Vector2D & target_point,
                            const int n_turn,
                            const int n_dash,
                            const GameTime & start_time ,
                            NeckAction::Ptr neck = NeckAction::Ptr(),
                            ViewAction::Ptr view = ViewAction::Ptr() )
        : M_target_point( target_point ),
          M_turn_step( n_turn ),
          M_dash_step( n_dash ),
          M_last_execute_time( start_time ),
          M_neck_action( neck ),
          M_view_action( view )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );

private:
    /*!
      \brief clear the action queue
     */
    void clear()
      {
          M_turn_step = M_dash_step = 0;
      }

    bool checkOpponent( const WorldModel & wm );
    bool doTurn( PlayerAgent * agent );
    bool doDash( PlayerAgent * agent );
};

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::finished( const PlayerAgent * agent )
{
    if ( M_turn_step + M_dash_step == 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished) check finished. empty queue" );
        return true;
    }

    const WorldModel & wm = agent->world();

    if ( M_last_execute_time.cycle() + 1 != wm.time().cycle() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": finished(). last execute time is not match" );
        return true;
    }

    if ( wm.existKickableTeammate()
         || wm.existKickableOpponent() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished). exist other kickable player" );
        return true;
    }

    if ( wm.ball().pos().dist2( M_target_point ) < std::pow( 1.0, 2 )
         && wm.self().pos().dist2( M_target_point ) < std::pow( 1.0, 2 ) )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished). reached target point" );
        return true;
    }

    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    if ( ball_next.absX() > ServerParam::i().pitchHalfLength() - 0.5
         || ball_next.absY() > ServerParam::i().pitchHalfWidth() - 0.5 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished) ball will be out of pitch. stop intention." );
        return true;
    }

    if ( wm.audioMemory().passRequestTime() == agent->world().time() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished). heard pass request." );
        return true;
    }

    // playmode is checked in PlayerAgent::parse()
    // and intention queue is totally managed at there.

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (finished). not finished yet." );

    return false;
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::execute( PlayerAgent * agent )
{
    if ( M_turn_step + M_dash_step == 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) empty queue." );
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:execute) turn=%d dash=%d",
                  M_turn_step, M_dash_step );

    const WorldModel & wm = agent->world();

    //
    // compare the current queue with other chain action candidates
    //

    if ( wm.self().isKickable()
         && M_turn_step <= 0 )
    {
        CooperativeAction::Ptr current_action( new Dribble( wm.self().unum(),
                                                            M_target_point,
                                                            wm.ball().vel().r(),
                                                            0,
                                                            M_turn_step,
                                                            M_dash_step,
                                                            "queuedDribble" ) );
        current_action->setIndex( 0 );
        current_action->setFirstDashPower( ServerParam::i().maxDashPower() );

        ShortDribbleGenerator::instance().setQueuedAction( wm, current_action );

        ActionChainHolder::instance().update( wm );
        const ActionChainGraph & search_result = ActionChainHolder::i().graph();
        const CooperativeAction & first_action = search_result.getFirstAction();

        if ( first_action.category() != CooperativeAction::Dribble
             || ! first_action.targetPoint().equals( current_action->targetPoint() ) )
        {
            agent->debugClient().addMessage( "CancelDribbleQ" );
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:execute) cancel. select other action." );
            return false;
        }
    }

    //
    //
    //

    if ( checkOpponent( wm ) )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute). but exist opponent. cancel intention." );
        return false;
    }

    //
    // execute action
    //

    if ( M_turn_step > 0 )
    {
        if ( ! doTurn( agent ) )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:exuecute) failed to turn. clear intention" );
            this->clear();
            return false;
        }
    }
    else if ( M_dash_step > 0 )
    {
        if ( ! doDash( agent ) )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:execute) failed to dash.  clear intention" );
            this->clear();
            return false;
        }
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute). No command queue" );
        this->clear();
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:execute). done" );
    agent->debugClient().addMessage( "NormalDribbleQ%d:%d", M_turn_step, M_dash_step );
    agent->debugClient().setTarget( M_target_point );


    if ( ! M_view_action )
    {
        if ( wm.gameMode().type() != GameMode::PlayOn
             || M_turn_step + M_dash_step <= 1 )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:execute) default view synch" );
            agent->debugClient().addMessage( "ViewSynch" );
            agent->setViewAction( new View_Synch() );
        }
        else
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:execute) default view normal" );
            agent->debugClient().addMessage( "ViewNormal" );
            agent->setViewAction( new View_Normal() );
        }
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) registered view" );
        agent->debugClient().addMessage( "ViewRegisterd" );
        agent->setViewAction( M_view_action->clone() );
    }

    if ( ! M_neck_action )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) default turn_neck scan field" );
        agent->debugClient().addMessage( "NeckScan" );
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) registered turn_neck" );
        agent->debugClient().addMessage( "NeckRegistered" );
        agent->setNeckAction( M_neck_action->clone() );
    }

    M_last_execute_time = wm.time();

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::checkOpponent( const WorldModel & wm )
{
    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    /*--------------------------------------------------------*/
    // exist near opponent goalie in NEXT cycle
    if ( ball_next.x > ServerParam::i().theirPenaltyAreaLineX()
         && ball_next.absY() < ServerParam::i().penaltyAreaHalfWidth() )
    {
        const PlayerObject * opp_goalie = wm.getOpponentGoalie();
        if ( opp_goalie
             && opp_goalie->distFromBall() < ( ServerParam::i().catchableArea()
                                               + ServerParam::i().defaultPlayerSpeedMax() )
             )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": existOpponent(). but exist near opponent goalie" );
            this->clear();
            return true;
        }
    }

    const PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 5 );

    if ( ! nearest_opp )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": existOppnent(). No opponent" );
        return false;
    }

    /*--------------------------------------------------------*/
    // exist very close opponent at CURRENT cycle
    if (  nearest_opp->distFromBall() < ServerParam::i().defaultKickableArea() + 0.2 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": existOpponent(). but exist kickable opp" );
        this->clear();
        return true;
    }

    /*--------------------------------------------------------*/
    // exist near opponent at NEXT cycle
    if ( nearest_opp->pos().dist( ball_next )
         < ServerParam::i().defaultPlayerSpeedMax() + ServerParam::i().defaultKickableArea() + 0.3 )
    {
        const Vector2D opp_next = nearest_opp->pos() + nearest_opp->vel();
        // oppopnent angle is known
        if ( nearest_opp->bodyCount() == 0
             || nearest_opp->vel().r() > 0.2 )
        {
            Line2D opp_line( opp_next,
                             ( nearest_opp->bodyCount() == 0
                               ? nearest_opp->body()
                               : nearest_opp->vel().th() ) );
            if ( opp_line.dist( ball_next ) > 1.2 )
            {
                // never reach
                dlog.addText( Logger::DRIBBLE,
                              __FILE__": existOpponent(). opp never reach." );
            }
            else if ( opp_next.dist( ball_next ) < 0.6 + 1.2 )
            {
                dlog.addText( Logger::DRIBBLE,
                              __FILE__": existOpponent(). but opp may reachable(1)." );
                this->clear();
                return true;
            }

            dlog.addText( Logger::DRIBBLE,
                          __FILE__": existOpponent(). opp angle is known. opp may not be reached." );
        }
        // opponent angle is not known
        else
        {
            if ( opp_next.dist( ball_next ) < 1.2 + 1.2 ) //0.6 + 1.2 )
            {
                dlog.addText( Logger::DRIBBLE,
                              __FILE__": existOpponent(). but opp may reachable(2)." );
                this->clear();
                return true;
            }
        }

        dlog.addText( Logger::DRIBBLE,
                      __FILE__": existOpponent(). exist near opp. but avoidable?" );
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::doTurn( PlayerAgent * agent )
{
    if ( M_turn_step <= 0 )
    {
        return false;
    }

    const double default_dist_thr = 0.5;

    const WorldModel & wm = agent->world();

    --M_turn_step;

    Vector2D my_inertia = wm.self().inertiaPoint( M_turn_step + M_dash_step );
    AngleDeg target_angle = ( M_target_point - my_inertia ).th();
    AngleDeg angle_diff = target_angle - wm.self().body();

    double target_dist = ( M_target_point - my_inertia ).r();
    double angle_margin
        = std::max( 15.0,
                    std::fabs( AngleDeg::atan2_deg( default_dist_thr,
                                                    target_dist ) ) );

    if ( angle_diff.abs() < angle_margin )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (doTurn)  but already facing. diff = %.1f  margin=%.1f",
                      angle_diff.degree(), angle_margin );
        this->clear();
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (doTurn) turn to (%.2f, %.2f) moment=%f",
                  M_target_point.x, M_target_point.y, angle_diff.degree() );

    agent->doTurn( angle_diff );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::doDash( PlayerAgent * agent )
{
    if ( M_dash_step <= 0 )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    --M_dash_step;

    double dash_power
        = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
    double accel_mag = dash_power * wm.self().dashRate();

    Vector2D dash_accel = Vector2D::polar2vector( accel_mag, wm.self().body() );

    Vector2D my_next = wm.self().pos() + wm.self().vel() + dash_accel;
    Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    Vector2D ball_next_rel = ( ball_next - my_next ).rotatedVector( - wm.self().body() );
    double ball_next_dist = ball_next_rel.r();

    if ( ball_next_dist < ( wm.self().playerType().playerSize()
                            + ServerParam::i().ballSize() )
         )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (doDash) collision may occur. ball_dist = %f",
                      ball_next_dist );
        this->clear();
        return false;
    }

    if ( ball_next_rel.absY() > wm.self().playerType().kickableArea() - 0.1 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (doDash) next Y difference is over. y_diff = %f",
                      ball_next_rel.absY() );
        this->clear();
        return false;
    }

    // this dash is the last of queue
    // but at next cycle, ball is NOT kickable
    if ( M_dash_step <= 0 )
    {
        if ( ball_next_dist > wm.self().playerType().kickableArea() - 0.15 )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (doDash) last dash. but not kickable at next. ball_dist=%f",
                          ball_next_dist );
            this->clear();
            return false;
        }
    }

    if ( M_dash_step > 0 )
    {
        // remain dash step. but next dash cause over run.
        AngleDeg ball_next_angle = ( ball_next - my_next ).th();
        if ( ( wm.self().body() - ball_next_angle ).abs() > 90.0
             && ball_next_dist > wm.self().playerType().kickableArea() - 0.2 )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (doDash) dash. but run over. ball_dist=%f",
                          ball_next_dist );
            this->clear();
            return false;
        }
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (doDash) power=%.1f  accel_mag=%.2f",
                  dash_power, accel_mag );
    agent->doDash( dash_power );

    return true;
}





/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

*/
Bhv_NormalDribble::Bhv_NormalDribble( const CooperativeAction & action,
                                      NeckAction::Ptr neck,
                                      ViewAction::Ptr view )
    : M_target_point( action.targetPoint() ),
      M_first_ball_speed( action.firstBallSpeed() ),
      M_first_turn_moment( action.firstTurnMoment() ),
      M_first_dash_power( action.firstDashPower() ),
      M_first_dash_angle( action.firstDashAngle() ),
      M_total_step( action.durationStep() ),
      M_kick_step( action.kickCount() ),
      M_turn_step( action.turnCount() ),
      M_dash_step( action.dashCount() ),
      M_neck_action( neck ),
      M_view_action( view )
{
    if ( action.category() != CooperativeAction::Dribble )
    {
        M_target_point = Vector2D::INVALIDATED;
        M_total_step = 0;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_NormalDribble::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). no kickable..." );
        return false;
    }

    if ( ! M_target_point.isValid()
         || M_total_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). illegal target point or illegal total step" );
#ifdef DEBUG_PRINT
        std::cerr << wm.self().unum() << ' ' << wm.time()
                  << " Bhv_NormalDribble::execute() illegal target point or illegal total step"
                  << std::endl;
#endif
        return false;
    }

    const ServerParam & SP = ServerParam::i();

    if ( M_kick_step > 0 )
    {
        Vector2D first_vel = M_target_point - wm.ball().pos();
        first_vel.setLength( M_first_ball_speed );

        const Vector2D kick_accel = first_vel - wm.ball().vel();
        const double kick_power = kick_accel.r() / wm.self().kickRate();
        const AngleDeg kick_angle = kick_accel.th() - wm.self().body();

        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). first kick: power=%f angle=%f, n_turn=%d n_dash=%d",
                      kick_power, kick_angle.degree(),
                      M_turn_step, M_dash_step );

        if ( kick_power > SP.maxPower() + 1.0e-5 )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (execute) over the max power" );
#ifdef DEBUG_PRINT
            std::cerr << __FILE__ << ':' << __LINE__ << ':'
                      << wm.self().unum() << ' ' << wm.time()
                      << " over the max power " << kick_power << std::endl;
#endif
        }

        agent->doKick( kick_power, kick_angle );

        dlog.addCircle( Logger::DRIBBLE,
                        wm.ball().pos() + first_vel,
                        0.1,
                        "#0000ff" );
    }
    else if ( M_turn_step > 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). first turn: moment=%.2f, n_turn=%d n_dash=%d",
                      M_first_turn_moment,
                      M_turn_step, M_dash_step );

        agent->doTurn( M_first_turn_moment );

    }
    else if ( M_dash_step > 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). first dash: dash power=%.1f dir=%.1f",
                      M_first_dash_power, M_first_dash_angle.degree() );
        agent->doDash( M_first_dash_power, M_first_dash_angle );
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). no action steps" );
        std::cerr << __FILE__ << ':' << __LINE__ << ':'
                  << wm.self().unum() << ' ' << wm.time()
                  << " no action step." << std::endl;
        return false;
    }


   agent->setIntention( new IntentionNormalDribble( M_target_point,
                                                    M_turn_step,
                                                    M_total_step - M_turn_step - 1, // n_dash
                                                    wm.time() ) );

   if ( ! M_view_action )
   {
       if ( M_turn_step + M_dash_step >= 3 )
       {
           agent->setViewAction( new View_Normal() );
       }
   }
   else
   {
       agent->setViewAction( M_view_action->clone() );
   }

   if ( ! M_neck_action )
   {
       agent->setNeckAction( new Neck_ScanField() );
   }
   else
   {
       agent->setNeckAction( M_neck_action->clone() );
   }

    return true;
}
