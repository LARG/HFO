// -*-c++-*-

/*!
  \file sample_communication.cpp
  \brief sample communication planner Source File
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

#include "sample_communication.h"

#include "strategy.h"

#include <rcsc/formation/formation.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/freeform_parser.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/say_message_parser.h>

#include <cmath>

// #define DEBUG_PRINT
// #define DEBUG_PRINT_PLAYER_RECORD

using namespace rcsc;

namespace {

struct ObjectScore {
    const AbstractPlayerObject * player_;
    int number_;
    double score_;

    ObjectScore()
        : player_( static_cast< AbstractPlayerObject * >( 0 ) ),
          number_( -1 ),
          score_( -65535.0 )
      { }

    struct Compare {
        bool operator()( const ObjectScore & lhs,
                         const ObjectScore & rhs ) const
          {
              return lhs.score_ > rhs.score_;
          }
    };

    struct IllegalChecker {
        bool operator()( const ObjectScore & val ) const
          {
              return val.score_ <= 0.1;
          }
    };
};


struct AbstractPlayerSelfDistCmp {
    bool operator()( const AbstractPlayerObject * lhs,
                     const AbstractPlayerObject * rhs ) const
      {
          return lhs->distFromSelf() < rhs->distFromSelf();
      }
};

struct AbstractPlayerBallDistCmp {
    bool operator()( const AbstractPlayerObject * lhs,
                     const AbstractPlayerObject * rhs ) const
      {
          return lhs->distFromBall() < rhs->distFromBall();
      }
};


struct AbstractPlayerBallXDiffCmp {
private:
    AbstractPlayerBallXDiffCmp();
public:
    const double ball_x_;
    AbstractPlayerBallXDiffCmp( const double & ball_x )
        : ball_x_( ball_x )
      { }

    bool operator()( const AbstractPlayerObject * lhs,
                     const AbstractPlayerObject * rhs ) const
      {
          return std::fabs( lhs->pos().x - ball_x_ )
              < std::fabs( rhs->pos().x - ball_x_ );
      }
};

struct AbstractPlayerXCmp {
    bool operator()( const AbstractPlayerObject * lhs,
                     const AbstractPlayerObject * rhs ) const
      {
          return lhs->pos().x < rhs->pos().x;
      }
};

inline
double
distance_rate( const double & val,
               const double & variance )
{
    return std::exp( - std::pow( val, 2 ) / ( 2.0 * std::pow( variance, 2 ) ) );

}

inline
double
distance_from_ball( const AbstractPlayerObject * p,
                    const Vector2D & ball_pos,
                    const double & x_rate,
                    const double & y_rate )
{
    //return p->distFromBall();
    return std::sqrt( std::pow( ( p->pos().x - ball_pos.x ) * x_rate, 2 )
                      + std::pow( ( p->pos().y - ball_pos.y ) * y_rate, 2 ) ); // Magic Number
}

}

/*-------------------------------------------------------------------*/
/*!

 */
SampleCommunication::SampleCommunication()
    : M_current_sender_unum( Unum_Unknown ),
      M_next_sender_unum( Unum_Unknown ),
      M_ball_send_time( 0, 0 )
{
    for ( int i = 0; i < 12; ++i )
    {
        M_teammate_send_time[i].assign( 0, 0 );
        M_opponent_send_time[i].assign( 0, 0 );
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
SampleCommunication::~SampleCommunication()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::execute( PlayerAgent * agent )
{
    if ( ! agent->config().useCommunication() )
    {
        return false;
    }

    updateCurrentSender( agent );

    const WorldModel & wm = agent->world();
    const bool penalty_shootout = wm.gameMode().isPenaltyKickMode();

    bool say_recovery = false;
    if ( wm.gameMode().type() != GameMode::PlayOn
         && ! penalty_shootout
         && currentSenderUnum() == wm.self().unum()
         && wm.self().recovery() < ServerParam::i().recoverInit() - 0.002 )
    {
        say_recovery = true;
        sayRecovery( agent );
    }

    if ( wm.gameMode().type() == GameMode::BeforeKickOff
         || wm.gameMode().type() == GameMode::AfterGoal_
         || penalty_shootout )
    {
        return say_recovery;
    }

#ifdef DEBUG_PRINT_PLAYER_RECORD
    const AudioMemory::PlayerRecord::const_iterator end = agent->world().audioMemory().playerRecord().end();
    for ( AudioMemory::PlayerRecord::const_iterator p = agent->world().audioMemory().playerRecord().begin();
          p != end;
          ++p )
    {
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": PlayerRecord: time=[%d,%d] %s %d from %d",
                      p->first.cycle(), p->first.stopped(),
                      p->second.unum_ <= 11 ? "teammate" : "opponent",
                      p->second.unum_ <= 11 ? p->second.unum_ : p->second.unum_ - 11,
                      p->second.sender_ );
    }
#endif

#if 1
    sayBallAndPlayers( agent );
    sayStamina( agent );
#else
    sayBall( agent );
    sayGoalie( agent );
    saySelf( agent );
    sayPlayers( agent );
#endif
    attentiontoSomeone( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SampleCommunication::updateCurrentSender( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( agent->effector().getSayMessageLength() > 0 )
    {
        M_current_sender_unum = wm.self().unum();
        return;
    }

    // if ( wm.self().distFromBall() > ServerParam::i().audioCutDist() + 30.0 )
    // {
    //     M_current_sender_unum = wm.self().unum();
    //     return;
    // }

    M_current_sender_unum = Unum_Unknown;

    std::vector< int > candidate_unum;
    candidate_unum.reserve( 11 );

    if ( wm.ball().pos().x < -10.0
         || wm.gameMode().type() != GameMode::PlayOn )
    {
        //
        // all players
        //
        for ( int unum = 1; unum <= 11; ++unum )
        {
            candidate_unum.push_back( unum );
        }
    }
    else
    {
        //
        // exclude goalie
        //
        const int goalie_unum = ( wm.ourGoalieUnum() != Unum_Unknown
                                  ? wm.ourGoalieUnum()
                                  : Strategy::i().goalieUnum() );

        for ( int unum = 1; unum <= 11; ++unum )
        {
            if ( unum != goalie_unum )
            {
                candidate_unum.push_back( unum );
#if 0
                dlog.addText( Logger::COMMUNICATION,
                              __FILE__": (updateCurrentSender) field_player unum=%d",
                              candidate_unum.back() );
#endif
            }
        }
    }

    int val = ( wm.time().stopped() > 0
                ? wm.time().cycle() + wm.time().stopped()
                : wm.time().cycle() );

    int current = val % candidate_unum.size();
    int next = ( val + 1 ) % candidate_unum.size();

    M_current_sender_unum = candidate_unum[current];
    M_next_sender_unum = candidate_unum[next];

#ifdef DEBUG_PRINT
    dlog.addText( Logger::COMMUNICATION,
                  __FILE__": (updateCurrentSender) time=%d size=%d index=%d current=%d next=%d",
                  wm.time().cycle(),
                  candidate_unum.size(),
                  current,
                  M_current_sender_unum,
                  M_next_sender_unum );
#endif

}

/*-------------------------------------------------------------------*/
/*!

 */
void
SampleCommunication::updatePlayerSendTime( const WorldModel & wm,
                                           const SideID side,
                                           const int unum )
{
    if ( unum < 1 || 11 < unum )
    {
        std::cerr << __FILE__ << ':' << __LINE__
                  << ": Illegal player number. " << unum
                  << std::endl;
        return;
    }

    if ( side == wm.ourSide() )
    {
        M_teammate_send_time[unum] = wm.time();
    }
    else
    {
        M_opponent_send_time[unum] = wm.time();
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::shouldSayBall( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.ball().seenPosCount() > 0
         || wm.ball().seenVelCount() > 2 )
    {
        return false;
    }

    if ( wm.gameMode().type() != GameMode::PlayOn )
    {
        if ( agent->effector().queuedNextBallVel().r2() < std::pow( 0.5, 2 ) )
        {
            return false;
        }
    }

    if ( wm.existKickableTeammate() )
    {
        return false;
    }

    bool ball_vel_changed = false;

    const double cur_ball_speed = wm.ball().vel().r();

    const BallObject::State * prev_ball_state = wm.ball().getState( 1 );
    if ( prev_ball_state )
    {
        const double prev_ball_speed = prev_ball_state->vel_.r();

        double angle_diff = ( wm.ball().vel().th() - prev_ball_state->vel_.th() ).abs();

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (shouldSayBall) check ball vel" );
        dlog.addText( Logger::COMMUNICATION,
                      "prev_vel=(%.2f %.2f) r=%.3f",
                      prev_ball_state->vel_.x, prev_ball_state->vel_.y,
                      prev_ball_speed );
        dlog.addText( Logger::COMMUNICATION,
                      "cur_vel=(%.2f %.2f) r=%.3f",
                      wm.ball().vel().x, wm.ball().vel().y,
                      cur_ball_speed );

        if ( cur_ball_speed > prev_ball_speed + 0.1
             || ( prev_ball_speed > 0.5
                  && cur_ball_speed < prev_ball_speed * ServerParam::i().ballDecay() * 0.5 )
             || ( prev_ball_speed > 0.5         // Magic Number
                  && angle_diff > 20.0 ) ) // Magic Number
        {
            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (shouldSayBall) ball vel changed." );
            ball_vel_changed = true;
        }
    }

    if ( ball_vel_changed
         && wm.lastKickerSide() != wm.ourSide()
         && ! wm.existKickableOpponent() )
    {
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (shouldSayBall) ball vel changed. opponent kicked. no opponent kicker" );
        return true;
    }

    if ( wm.self().isKickable() )
    {
        if ( agent->effector().queuedNextBallKickable() )
        {
            if ( cur_ball_speed < 1.0 )
            {
                return false;
            }
        }

        if ( ball_vel_changed
             && agent->effector().queuedNextBallVel().r() > 1.0 )
        {
            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (shouldSayBall) kickable. ball vel changed" );
            return true;
        }

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (shouldSayBall) kickable, but no say" );
        return false;
    }


    const PlayerObject * ball_nearest_teammate = NULL;
    const PlayerObject * second_ball_nearest_teammate = NULL;

    const PlayerPtrCont::const_iterator t_end = wm.teammatesFromBall().end();
    for ( PlayerPtrCont::const_iterator t = wm.teammatesFromBall().begin();
          t != t_end;
          ++t )
    {
        if ( (*t)->isGhost() || (*t)->posCount() >= 10 ) continue;

        if ( ! ball_nearest_teammate )
        {
            ball_nearest_teammate = *t;
        }
        else if ( ! second_ball_nearest_teammate )
        {
            second_ball_nearest_teammate = *t;
            break;
        }
    }

    int our_min = std::min( wm.interceptTable()->selfReachCycle(),
                            wm.interceptTable()->teammateReachCycle() );
    int opp_min = wm.interceptTable()->opponentReachCycle();

    //
    // I am the nearest player to ball
    //
    if ( ! ball_nearest_teammate
         || ( ball_nearest_teammate->distFromBall() > wm.ball().distFromSelf() - 3.0 )  )
    {
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (shouldSayBall) maybe nearest to ball" );
        //return true;

        if ( ball_vel_changed
             || ( opp_min <= 1 && cur_ball_speed > 1.0 ) )
        {
            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (shouldSayBall) nearest to ball. ball vel changed?" );
            return true;
        }
    }

    if ( ball_nearest_teammate
         && wm.ball().distFromSelf() < 20.0
         && 1.0 < ball_nearest_teammate->distFromBall()
         && ball_nearest_teammate->distFromBall() < 6.0
         && ( opp_min <= our_min + 1
              || ball_vel_changed ) )
    {
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (shouldSayBall) support nearest player" );
        return true;
    }

#if 0
    if ( ball_nearest_teammate
         && second_ball_nearest_teammate
         && ( ball_nearest_teammate->distFromBall()  // the ball nearest player
              > ServerParam::i().visibleDistance() - 0.5 ) // over vis dist
         && ( second_ball_nearest_teammate->distFromBall() > wm.ball().distFromSelf() - 5.0 )  // I am second
         )
    {
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (shouldSayBall) second nearest to ball" );
        return true;
    }
#endif
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::shouldSayOpponentGoalie( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * goalie = wm.getOpponentGoalie();

    if ( ! goalie )
    {
        return false;
    }

    if ( goalie->seenPosCount() == 0
         && goalie->bodyCount() == 0
         && goalie->unum() != Unum_Unknown
         && goalie->unumCount() == 0
         && goalie->distFromSelf() < 25.0
         && 53.0 - 16.0 < goalie->pos().x
         && goalie->pos().x < 52.5
         && goalie->pos().absY() < 20.0 )
    {
        Vector2D goal_pos = ServerParam::i().theirTeamGoalPos();
        Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

        if ( ball_next.dist2( goal_pos ) < std::pow( 18.0, 2 ) )
        {
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::goalieSaySituation( const rcsc::PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! wm.self().goalie() )
    {
        return false;
    }

    int player_count = 0;

    const AbstractPlayerCont::const_iterator end = wm.allPlayers().end();
    for ( AbstractPlayerCont::const_iterator p = wm.allPlayers().begin();
          p != end;
          ++p )
    {
        if ( (*p)->unumCount() == 0 )
        {
            ++player_count;
        }
    }

    if ( player_count >= 5 )
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayBallAndPlayers( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int current_len = agent->effector().getSayMessageLength();

    const bool should_say_ball = shouldSayBall( agent );
    const bool should_say_goalie = shouldSayOpponentGoalie( agent );
    const bool goalie_say_situation = false; //goalieSaySituation( agent );

    if ( ! should_say_ball
         && ! should_say_goalie
         && ! goalie_say_situation
         && currentSenderUnum() != wm.self().unum()
         && current_len == 0 )
    {
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayBallAndPlayers) no send situation" );
        return false;
    }

    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;

    const AudioMemory & am = wm.audioMemory();

    //
    // initialize object priority score with fixed value 1000
    //

    std::vector< ObjectScore > objects( 23 ); // 0: ball, 1-11: teammate, 12-23: opponent

    for ( int i = 0; i < 23; ++i )
    {
        objects[i].number_ = i;
        objects[i].score_ = 1000.0;
    }

    //
    // as a base score, set time difference since last hear
    //
    objects[0].score_ = wm.time().cycle() - am.ballTime().cycle();

    {
        const AudioMemory::PlayerRecord::const_iterator end = am.playerRecord().end();
        for ( AudioMemory::PlayerRecord::const_iterator p = am.playerRecord().begin();
              p != end;
              ++p )
        {
            int num = p->second.unum_;
            if ( num < 1 || 22 < num )
            {
                continue;
            }

            objects[num].score_ = wm.time().cycle() -  p->first.cycle();
        }
    }

    //
    // opponent goalie
    //
    if ( 1 <= wm.theirGoalieUnum()
         && wm.theirGoalieUnum() <= 11 )
    {
        int num = wm.theirGoalieUnum() + 11;
        double diff = wm.time().cycle() - am.goalieTime().cycle();
        objects[num].score_ = std::min( objects[num].score_, diff );
    }

#ifdef DEBUG_PRINT
    for ( int i = 0; i < 23; ++i )
    {
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": base score: %s %d = %f",
                      ( i == 0 ? "ball" : i <= 11 ? "teammate" : "opponent" ),
                      ( i <= 11 ? i : i - 11 ),
                      objects[i].score_ );
    }
#endif

    //
    // ball
    //

    if ( wm.self().isKickable() )
    {
        if ( agent->effector().queuedNextBallKickable() )
        {
            objects[0].score_ = -65535.0;
        }
        else
        {
            objects[0].score_ = 1000.0;
        }
    }
    else if ( wm.ball().seenPosCount() > 0
              || wm.ball().seenVelCount() > 1
              || wm.existKickableTeammate() )
    {
        objects[0].score_ = -65535.0;
    }
    else if ( should_say_ball )
    {
        objects[0].score_ = 1000.0;
    }
    else if ( objects[0].score_ > 0.0 )
    {
        const BallObject::State & prev_state = wm.ball().stateRecord().front();

        double angle_diff = ( wm.ball().vel().th() - prev_state.vel_.th() ).abs();
        double prev_speed = prev_state.vel_.r();
        double cur_speed = wm.ball().vel().r();

        if ( cur_speed > prev_speed + 0.1
             || ( prev_speed > 0.5
                  && cur_speed < prev_speed * ServerParam::i().ballDecay() * 0.5 )
             || ( prev_speed > 0.5         // Magic Number
                  && angle_diff > 20.0 ) ) // Magic Number
        {
            objects[0].score_ = 1000.0;
        }
        else
        {
            objects[0].score_ *= 0.5;
        }
    }

    //
    // players:
    //   1. set illegal value if player's uniform number has not been seen directry.
    //   2. reduced the priority based on the distance from ball
    //
    {
        const double variance = 30.0; // Magic Number
        const double x_rate = 1.0; // Magic Number
        const double y_rate = 0.5; // Magic Number

        const int min_step = std::min( std::min( wm.interceptTable()->opponentReachCycle(),
                                                 wm.interceptTable()->teammateReachCycle() ),
                                       wm.interceptTable()->selfReachCycle() );

        const Vector2D ball_pos = wm.ball().inertiaPoint( min_step );

        for ( int unum = 1; unum <= 11; ++unum )
        {
            {
                const AbstractPlayerObject * t = wm.ourPlayer( unum );
                if ( ! t
                     || t->unumCount() >= 2 )
                {
                    objects[unum].score_ = -65535.0;
                }
                else
                {
                    double d = distance_from_ball( t, ball_pos, x_rate, y_rate );
                    objects[unum].score_ *= distance_rate( d, variance );
                    objects[unum].score_ *= std::pow( 0.3, t->unumCount() );
                    objects[unum].player_ = t;
                }
            }
            {
                const AbstractPlayerObject * o = wm.theirPlayer( unum );
                if ( ! o
                     || o->unumCount() >= 2 )
                {
                    objects[unum + 11].score_ = -65535.0;
                }
                else
                {
                    double d = distance_from_ball( o, ball_pos, x_rate, y_rate );
                    objects[unum + 11].score_ *= distance_rate( d, variance );
                    objects[unum].score_ *= std::pow( 0.3, o->unumCount() );
                    objects[unum + 11].player_ = o;
                }
            }
        }
    }

    //
    // erase illegal(unseen) objects
    //

    objects.erase( std::remove_if( objects.begin(),
                                   objects.end(),
                                   ObjectScore::IllegalChecker() ),
                   objects.end() );


    //
    // sorted by priority score
    //
    std::sort( objects.begin(), objects.end(),
               ObjectScore::Compare() );


#ifdef DEBUG_PRINT
    for ( std::vector< ObjectScore >::const_iterator it = objects.begin();
          it != objects.end();
          ++it )
    {
        // if ( it->score_ < 0.0 ) break;

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": rated score: %s %d = %f",
                      ( it->number_ == 0 ? "ball" : it->number_ <= 11 ? "teammate" : "opponent" ),
                      ( it->number_ <= 11 ? it->number_ : it->number_ - 11 ),
                      it->score_ );
    }
#endif

    bool can_send_ball = false;
    bool send_ball_and_player = false;
    std::vector< ObjectScore > send_players;

    for ( std::vector< ObjectScore >::iterator it = objects.begin();
          it != objects.end();
          ++it )
    {
        if ( it->number_ == 0 )
        {
            can_send_ball = true;
            if ( send_players.size() == 1 )
            {
                send_ball_and_player = true;
                break;
            }
        }
        else
        {
            send_players.push_back( *it );

            if ( can_send_ball )
            {
                send_ball_and_player = true;
                break;
            }

            if ( send_players.size() >= 3 )
            {
                break;
            }
        }
    }

    if ( should_say_ball )
    {
        can_send_ball = true;
    }

    Vector2D ball_vel = agent->effector().queuedNextBallVel();
    if ( wm.self().isKickable()
         && agent->effector().queuedNextBallKickable() )
    {
        // invalidate ball velocity
        ball_vel.assign( 0.0, 0.0 );
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayBallAndPlayers) next cycle kickable." );
    }

    if ( wm.existKickableOpponent()
         || wm.existKickableTeammate() )
    {
        ball_vel.assign( 0.0, 0.0 );
    }

    //
    // send ball only
    //
    if ( can_send_ball
         && ! send_ball_and_player
         && available_len >= BallMessage::slength() )
    {
        if ( available_len >= BallPlayerMessage::slength() )
        {
            agent->addSayMessage( new BallPlayerMessage( agent->effector().queuedNextBallPos(),
                                                         ball_vel,
                                                         wm.self().unum(),
                                                         agent->effector().queuedNextSelfPos(),
                                                         agent->effector().queuedNextSelfBody() ) );
            updatePlayerSendTime( wm, wm.ourSide(), wm.self().unum() );
        }
        else
        {
            agent->addSayMessage( new BallMessage( agent->effector().queuedNextBallPos(),
                                                   ball_vel ) );
        }
        M_ball_send_time = wm.time();
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayBallAndPlayers) only ball" );
        return true;
    }

    //
    // send ball and player
    //
    if ( send_ball_and_player )
    {
        if ( should_say_goalie
             && available_len >= BallGoalieMessage::slength() )
        {
            const PlayerObject * goalie = wm.getOpponentGoalie();
            agent->addSayMessage( new BallGoalieMessage( agent->effector().queuedNextBallPos(),
                                                         ball_vel,
                                                         goalie->pos() + goalie->vel(),
                                                         goalie->body() ) );
            M_ball_send_time = wm.time();
            updatePlayerSendTime( wm, goalie->side(), goalie->unum() );

            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (sayBallAndPlayers) ball and goalie" );
            return true;
        }

        if ( available_len >= BallPlayerMessage::slength() )
        {
            const AbstractPlayerObject * p0 = send_players[0].player_;
            if ( p0->unum() == wm.self().unum() )
            {
                agent->addSayMessage( new BallPlayerMessage( agent->effector().queuedNextBallPos(),
                                                             ball_vel,
                                                             wm.self().unum(),
                                                             agent->effector().queuedNextSelfPos(),
                                                             agent->effector().queuedNextSelfBody() ) );
            }
            else
            {
                agent->addSayMessage( new BallPlayerMessage( agent->effector().queuedNextBallPos(),
                                                             ball_vel,
                                                             send_players[0].number_,
                                                             p0->pos() + p0->vel(),
                                                             p0->body() ) );

            }
            M_ball_send_time = wm.time();
            updatePlayerSendTime( wm, p0->side(), p0->unum() );

            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (sayBallAndPlayers) ball and player %c%d",
                          p0->side() == wm.ourSide() ? 'T' : 'O', p0->unum() );
            return true;
        }
    }

    //
    // send players
    //

    if ( wm.ball().pos().x > 34.0
         && wm.ball().pos().absY() < 20.0 )
    {
        const PlayerObject * goalie = wm.getOpponentGoalie();
        if ( goalie
             && goalie->seenPosCount() == 0
             && goalie->bodyCount() == 0
             && goalie->pos().x > 53.0 - 16.0
             && goalie->pos().absY() < 20.0
             && goalie->unum() != Unum_Unknown
             && goalie->distFromSelf() < 25.0 )
        {
            if ( available_len >= GoalieAndPlayerMessage::slength() )
            {
                const AbstractPlayerObject * player = static_cast< AbstractPlayerObject * >( 0 );

                for ( std::vector< ObjectScore >::const_iterator it = send_players.begin();
                      it != send_players.end();
                      ++it )
                {
                    if ( it->player_->unum() != goalie->unum()
                         && it->player_->side() != goalie->side()  )
                    {
                        player = it->player_;
                        break;
                    }
                }

                if ( player )
                {
                    Vector2D goalie_pos = goalie->pos() + goalie->vel();
                    goalie_pos.x = bound( 53.0 - 16.0, goalie_pos.x, 52.9 );
                    goalie_pos.y = bound( -19.9, goalie_pos.y, +19.9 );
                    agent->addSayMessage( new GoalieAndPlayerMessage( goalie->unum(),
                                                                      goalie_pos,
                                                                      goalie->body(),
                                                                      ( player->side() == wm.ourSide()
                                                                        ? player->unum()
                                                                        : player->unum() + 11 ),
                                                                      player->pos() + player->vel() ) );

                    updatePlayerSendTime( wm, goalie->side(), goalie->unum() );
                    updatePlayerSendTime( wm, player->side(), player->unum() );

                    dlog.addText( Logger::COMMUNICATION,
                                  __FILE__": say goalie and player: goalie=%d (%.2f %.2f) body=%.1f,"
                                  " player=%s[%d] (%.2f %.2f)",
                                  goalie->unum(),
                                  goalie->pos().x,
                                  goalie->pos().y,
                                  goalie->body().degree(),
                                  ( player->side() == wm.ourSide() ? "teammate" : "opponent" ),
                                  player->unum() );
                    return true;
                }
            }

            if ( available_len >= GoalieMessage::slength() )
            {
                Vector2D goalie_pos = goalie->pos() + goalie->vel();
                goalie_pos.x = bound( 53.0 - 16.0, goalie_pos.x, 52.9 );
                goalie_pos.y = bound( -19.9, goalie_pos.y, +19.9 );
                agent->addSayMessage( new GoalieMessage( goalie->unum(),
                                                         goalie_pos,
                                                         goalie->body() ) );
                M_ball_send_time = wm.time();
                M_opponent_send_time[goalie->unum()] = wm.time();

                dlog.addText( Logger::COMMUNICATION,
                              __FILE__": say goalie info: %d pos=(%.1f %.1f) body=%.1f",
                              goalie->unum(),
                              goalie->pos().x,
                              goalie->pos().y,
                              goalie->body().degree() );
                return true;
            }
        }
    }

    if ( send_players.size() >= 3
         && available_len >= ThreePlayerMessage::slength() )
    {
        const AbstractPlayerObject * p0 = send_players[0].player_;
        const AbstractPlayerObject * p1 = send_players[1].player_;
        const AbstractPlayerObject * p2 = send_players[2].player_;
        agent->addSayMessage( new ThreePlayerMessage( send_players[0].number_,
                                                      p0->pos() + p0->vel(),
                                                      send_players[1].number_,
                                                      p1->pos() + p1->vel(),
                                                      send_players[2].number_,
                                                      p2->pos() + p2->vel() ) );

        updatePlayerSendTime( wm, p0->side(), p0->unum() );
        updatePlayerSendTime( wm, p1->side(), p1->unum() );
        updatePlayerSendTime( wm, p2->side(), p2->unum() );

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say three players %c%d %c%d %c%d",
                      p0->side() == wm.ourSide() ? 'T' : 'O', p0->unum(),
                      p1->side() == wm.ourSide() ? 'T' : 'O', p1->unum(),
                      p2->side() == wm.ourSide() ? 'T' : 'O', p2->unum() );
        return true;
    }

    if ( send_players.size() >= 2
         && available_len >= TwoPlayerMessage::slength() )
    {
        const AbstractPlayerObject * p0 = send_players[0].player_;
        const AbstractPlayerObject * p1 = send_players[1].player_;
        agent->addSayMessage( new TwoPlayerMessage( send_players[0].number_,
                                                    p0->pos() + p0->vel(),
                                                    send_players[1].number_,
                                                    p1->pos() + p1->vel() ) );

        updatePlayerSendTime( wm, p0->side(), p0->unum() );
        updatePlayerSendTime( wm, p1->side(), p1->unum() );

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say two players %c%d %c%d",
                      p0->side() == wm.ourSide() ? 'T' : 'O', p0->unum(),
                      p1->side() == wm.ourSide() ? 'T' : 'O', p1->unum() );
        return true;
    }

    if ( send_players.size() >= 1
         && available_len >= GoalieMessage::slength() )
    {
        const AbstractPlayerObject * p0 = send_players[0].player_;

        if ( p0->side() == wm.theirSide()
             && p0->goalie()
             && p0->pos().x > 53.0 - 16.0
             && p0->pos().absY() < 20.0
             && p0->distFromSelf() < 25.0 )
        {
            Vector2D goalie_pos = p0->pos() + p0->vel();
            goalie_pos.x = bound( 53.0 - 16.0, goalie_pos.x, 52.9 );
            goalie_pos.y = bound( -19.9, goalie_pos.y, +19.9 );
            agent->addSayMessage( new GoalieMessage( p0->unum(),
                                                     goalie_pos,
                                                     p0->body() ) );
            updatePlayerSendTime( wm, p0->side(), p0->unum() );

            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (sayBallAndPlayers) ball and goalie %c%d",
                          p0->side() == wm.ourSide() ? 'T' : 'O', p0->unum() );
            return true;
        }
    }

    if ( send_players.size() >= 1
         && available_len >= OnePlayerMessage::slength() )
    {
        const AbstractPlayerObject * p0 = send_players[0].player_;
        agent->addSayMessage( new OnePlayerMessage( send_players[0].number_,
                                                    p0->pos() + p0->vel() ) );

        updatePlayerSendTime( wm, p0->side(), p0->unum() );

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say one players %c%d",
                      p0->side() == wm.ourSide() ? 'T' : 'O', p0->unum() );
        return true;
    }


    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int current_len = agent->effector().getSayMessageLength();
    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;

    if ( available_len < BallMessage::slength() )
    {
        return false;
    }

    if ( wm.ball().seenPosCount() > 0
         || wm.ball().seenVelCount() > 1 )
    {
        // not seen at current cycle
        return false;
    }

    if ( wm.gameMode().type() != GameMode::PlayOn )
    {
        if ( agent->effector().queuedNextBallVel().r2() < 0.5*0.5 )
        {
            return false;
        }
    }

#if 1
    if ( wm.existKickableTeammate()
         //|| wm.existKickableOpponent()
         )
    {
        return false;
    }
#endif

    const PlayerObject * ball_nearest_teammate = NULL;
    const PlayerObject * second_ball_nearest_teammate = NULL;

    const PlayerPtrCont::const_iterator t_end = wm.teammatesFromBall().end();
    for ( PlayerPtrCont::const_iterator t = wm.teammatesFromBall().begin();
          t != t_end;
          ++t )
    {
        if ( (*t)->isGhost() || (*t)->posCount() >= 10 ) continue;

        if ( ! ball_nearest_teammate )
        {
            ball_nearest_teammate = *t;
        }
        else if ( ! second_ball_nearest_teammate )
        {
            second_ball_nearest_teammate = *t;
            break;
        }
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int our_min = std::min( self_min, mate_min );
    int opp_min = wm.interceptTable()->opponentReachCycle();


    bool send_ball = false;
    if ( wm.self().isKickable()  // I am kickable
         || ( ! ball_nearest_teammate
              || ( ball_nearest_teammate->distFromBall()
                   > wm.ball().distFromSelf() - 3.0 ) // I am nearest to ball
              || ( wm.ball().distFromSelf() < 20.0
                   && ball_nearest_teammate->distFromBall() < 6.0
                   && ball_nearest_teammate->distFromBall() > 1.0
                   && opp_min <= mate_min + 1 ) )
         || ( second_ball_nearest_teammate
              && ( ball_nearest_teammate->distFromBall()  // nearest to ball teammate is
                   > ServerParam::i().visibleDistance() - 0.5 ) // over vis dist
              && ( second_ball_nearest_teammate->distFromBall()
                   > wm.ball().distFromSelf() - 5.0 ) ) ) // I am second
    {
        send_ball = true;
    }

    //     const PlayerObject * nearest_opponent = wm.getOpponentNearestToBall( 10 );
    //     if ( nearest_opponent
    //          && ( nearest_opponent->distFromBall() >= 10.0
    //               || nearest_opponent->distFromBall() < nearest_opponent->playerTypePtr()->kickableArea() + 0.1 ) )
    //     {
    //         send_ball = false;
    //         return false;
    //     }

    Vector2D ball_vel = agent->effector().queuedNextBallVel();
    if ( wm.self().isKickable()
         && agent->effector().queuedNextBallKickable() )
    {
        // set ball velocity to 0.
        ball_vel.assign( 0.0, 0.0 );
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayBall) say ball status. next cycle kickable." );
    }

    Vector2D ball_trap_pos = wm.ball().inertiaPoint( our_min );

    //
    // ball & opponent goalie
    //
    if ( send_ball
         && ( wm.existKickableTeammate()
              || our_min <= opp_min + 1 )
         && ball_trap_pos.x > 34.0
         && ball_trap_pos.absY() < 20.0
         && available_len >= BallGoalieMessage::slength() )
    {
        if ( shouldSayOpponentGoalie( agent ) )
        {
            const PlayerObject * goalie = wm.getOpponentGoalie();
            agent->addSayMessage( new BallGoalieMessage( agent->effector().queuedNextBallPos(),
                                                         ball_vel,
                                                         goalie->pos() + goalie->vel(),
                                                         goalie->body() ) );
            M_ball_send_time = wm.time();
            updatePlayerSendTime( wm, goalie->side(), goalie->unum() );

            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (sayBall) say ball/goalie status" );
            return true;
        }
    }

    //
    // ball & opponent
    //
    if ( send_ball
         // && ball_nearest_teammate
         // && mate_min <= 3
         && available_len >= BallPlayerMessage::slength() )
    {
        // static GameTime s_last_opponent_send_time( 0, 0 );
        // static int s_last_sent_opponent_unum = Unum_Unknown;

        const PlayerObject * opponent = static_cast< const PlayerObject * >( 0 );

        Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

        const PlayerPtrCont::const_iterator o_end = wm.opponentsFromBall().end();
        for ( PlayerPtrCont::const_iterator o = wm.opponentsFromBall().begin();
              o != o_end;
              ++o )
        {
            if ( (*o)->seenPosCount() > 0 ) continue;
            if ( (*o)->unum() == Unum_Unknown ) continue;
            if ( (*o)->unumCount() > 0 ) continue;
            if ( (*o)->bodyCount() > 0 ) continue;
            if ( (*o)->pos().dist2( opp_trap_pos ) > 15.0*15.0 ) continue;
            // if ( (*o)->distFromBall() > 20.0 ) continue;
            // if ( (*o)->distFromSelf() > 20.0 ) continue;

            // if ( (*o)->unum() == s_last_sent_opponent_unum
            //      && s_last_opponent_send_time.cycle() >= wm.time().cycle() - 1 )
            // {
            //     continue;
            // }

            opponent = *o;
            break;
        }

        if ( opponent )
        {
            // s_last_opponent_send_time = wm.time();
            // s_last_sent_opponent_unum = opponent->unum();

            agent->addSayMessage( new BallPlayerMessage( agent->effector().queuedNextBallPos(),
                                                         ball_vel,
                                                         opponent->unum() + 11,
                                                         opponent->pos(),
                                                         opponent->body() ) );
            M_opponent_send_time[opponent->unum()] = wm.time();

            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (sayBall) say ball/opponent status. opponent=%d",
                          opponent->unum() );
            return true;
        }
    }

    if ( send_ball )
    {
        agent->addSayMessage( new BallMessage( agent->effector().queuedNextBallPos(),
                                               ball_vel ) );
        M_ball_send_time = wm.time();
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayBall) say ball only" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayGoalie( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    // goalie info: ball is in chance area
    const int current_len = agent->effector().getSayMessageLength();
    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;

    if ( available_len < GoalieMessage::slength() )
    {
        return false;
    }

    if ( shouldSayOpponentGoalie( agent ) )
    {
        const PlayerObject * goalie = wm.getOpponentGoalie();
        Vector2D goalie_pos = goalie->pos() + goalie->vel();
        goalie_pos.x = bound( 53.0 - 16.0, goalie_pos.x, 52.9 );
        goalie_pos.y = bound( -19.9, goalie_pos.y, +19.9 );
        agent->addSayMessage( new GoalieMessage( goalie->unum(),
                                                 goalie_pos,
                                                 goalie->body() ) );
        M_ball_send_time = wm.time();
        updatePlayerSendTime( wm, goalie->side(), goalie->unum() );

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say goalie info: %d pos=(%.1f %.1f) body=%.1f",
                      goalie->unum(),
                      goalie->pos().x,
                      goalie->pos().y,
                      goalie->body().degree() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int current_len = agent->effector().getSayMessageLength();
    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;

    if ( available_len < InterceptMessage::slength() )
    {
        return false;
    }

    if ( wm.gameMode().type() != GameMode::PlayOn )
    {
        return false;
    }

    if ( wm.ball().posCount() > 0
         || wm.ball().velCount() > 0 )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( wm.self().isKickable() )
    {
        double next_dist =  agent->effector().queuedNextMyPos().dist( agent->effector().queuedNextBallPos() );
        if ( next_dist > wm.self().playerType().kickableArea() )
        {
            self_min = 10000;
        }
        else
        {
            self_min = 0;
        }
    }

    if ( self_min <= mate_min
         && self_min <= opp_min
         && self_min <= 10 )
    {
        agent->addSayMessage( new InterceptMessage( true,
                                                    wm.self().unum(),
                                                    self_min ) );
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say self intercept info %d",
                      self_min );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayOffsideLine( PlayerAgent * agent )
{
    const int current_len = agent->effector().getSayMessageLength();

    if ( current_len == 0 )
    {
        return false;
    }

    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;

    if ( available_len < OffsideLineMessage::slength() )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    if ( wm.offsideLineX() < 10.0 )
    {
        return false;
    }

    if ( wm.seeTime() != wm.time()
         || wm.offsideLineCount() > 1
         || wm.opponentsFromSelf().size() < 10 )
    {
        return false;
    }

    int s_min = wm.interceptTable()->selfReachCycle();
    int t_min = wm.interceptTable()->teammateReachCycle();
    int o_min = wm.interceptTable()->opponentReachCycle();

    if ( o_min < t_min
         && o_min < s_min )
    {
        return false;
    }

    int min_step = std::min( s_min, t_min );

    Vector2D ball_pos = wm.ball().inertiaPoint( min_step );

    if ( 0.0 < ball_pos.x
         && ball_pos.x < 37.0
         && ball_pos.x > wm.offsideLineX() - 20.0
         && std::fabs( wm.self().pos().x - wm.offsideLineX() ) < 20.0
         )
    {
        agent->addSayMessage( new OffsideLineMessage( wm.offsideLineX() ) );
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say offside line %.1f",
                      wm.offsideLineX() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayDefenseLine( PlayerAgent * agent )
{
    const int current_len = agent->effector().getSayMessageLength();
    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;

    if ( available_len < DefenseLineMessage::slength() )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    if ( std::fabs( wm.self().pos().x - wm.ourDefenseLineX() ) > 40.0 )
    {
        return false;
    }

    if ( wm.seeTime() != wm.time() )
    {
        return false;
    }

    int opp_min = wm.interceptTable()->opponentReachCycle();

    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    if ( wm.self().goalie()
         && wm.gameMode().type() == GameMode::PlayOn
         && -40.0 < opp_trap_pos.x
         && opp_trap_pos.x < -20.0
         && wm.time().cycle() % 3 == 0 )
    {
        agent->addSayMessage( new DefenseLineMessage( wm.ourDefenseLineX() ) );
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say defense line %.1f",
                      wm.ourDefenseLineX() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayPlayers( PlayerAgent * agent )
{
#ifdef __APPLE__
    static GameTime s_last_time( -1, 0 );
#else
    static thread_local GameTime s_last_time( -1, 0 );
#endif

    const int len = agent->effector().getSayMessageLength();
    if ( len + OnePlayerMessage::slength() > ServerParam::i().playerSayMsgSize() )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    //     if ( s_last_time.cycle() >= wm.time().cycle() - 3 )
    //     {
    //         return false;
    //     }

    if ( len == 0
         && wm.time().cycle() % 11 != wm.self().unum() - 1 )
    {
        return false;
    }

    bool opponent_attack = false;

    int opp_min = wm.interceptTable()->opponentReachCycle();
    int mate_min = wm.interceptTable()->opponentReachCycle();
    int self_min = wm.interceptTable()->opponentReachCycle();

    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    if ( opp_min <= mate_min
         && opp_min <= self_min )
    {
        if ( opp_trap_pos.x < 10.0 )
        {
            opponent_attack = true;
        }
    }

    AbstractPlayerCont candidates;
    bool include_self = false;

    // set self as candidate
    if ( M_teammate_send_time[wm.self().unum()] != wm.time()
         && ( ! opponent_attack
              || wm.self().pos().x < opp_trap_pos.x + 10.0 )
         && wm.time().cycle() % 11 == ( wm.self().unum() - 1 ) )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayPlayers) candidate self(1)" );
#endif
        candidates.push_back( &(wm.self()) );
        include_self = true;
    }

    if ( ! opponent_attack )
    {
        // set teammate candidates
        const PlayerPtrCont::const_iterator t_end = wm.teammatesFromSelf().end();
        for ( PlayerPtrCont::const_iterator t = wm.teammatesFromSelf().begin();
              t != t_end;
              ++t )
        {
            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (sayPlayers) __ check(1) teammate %d(%.1f %.1f)",
                          (*t)->unum(),
                          (*t)->pos().x, (*t)->pos().y );

            if ( (*t)->seenPosCount() > 0 ) continue;
            //if ( (*t)->seenVelCount() > 0 ) continue;
            if ( (*t)->unumCount() > 0 ) continue;
            if ( (*t)->unum() == Unum_Unknown ) continue;

            if ( M_teammate_send_time[(*t)->unum()].cycle() >= wm.time().cycle() - 1 )
            {
                continue;
            }
#ifdef DEBUG_PRINT
            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (sayPlayers) candidate teammate(1) %d",
                          (*t)->unum() );
#endif
            candidates.push_back( *t );
        }
    }

    // set opponent candidates
    const PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
          o != o_end;
          ++o )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayPlayers) __ check opponent %d(%.1f %.1f)",
                      (*o)->unum(),
                      (*o)->pos().x, (*o)->pos().y );
#endif
        if ( (*o)->seenPosCount() > 0 ) continue;
        //if ( (*o)->seenVelCount() > 0 ) continue;
        if ( (*o)->unumCount() > 0 ) continue;
        if ( (*o)->unum() == Unum_Unknown ) continue;

        if ( ! opponent_attack )
        {
            if ( M_opponent_send_time[(*o)->unum()].cycle() >= wm.time().cycle() - 1 )
            {
                dlog.addText( Logger::COMMUNICATION,
                              __FILE__": (sayPlayers) __ check opponent %d. recently sent",
                              (*o)->unum() );
                continue;
            }
        }
        else
        {
            if ( (*o)->pos().x > 0.0
                 && (*o)->pos().x > wm.ourDefenseLineX() + 15.0 )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::COMMUNICATION,
                              __FILE__": (sayPlayers) __ check opponent %d. x>0 && x>defense_line+15",
                              (*o)->unum() );
#endif
                continue;
            }
        }
#ifdef DEBUG_PRINT
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayPlayers) candidate opponent %d",
                      (*o)->unum() );
#endif
        candidates.push_back( *o );
    }

    if ( //opponent_attack &&
        ! candidates.empty()
        && candidates.size() < 3 )
    {
        // set self
        if ( ! include_self
             && M_teammate_send_time[wm.self().unum()] != wm.time()
             && wm.time().cycle() % 11 == ( wm.self().unum() - 1 ) )
        {
            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (sayPlayers) candidate self(2)" );
            candidates.push_back( &(wm.self()) );
        }

        // set teammate candidates
        const PlayerPtrCont::const_iterator t_end = wm.teammatesFromSelf().end();
        for ( PlayerPtrCont::const_iterator t = wm.teammatesFromSelf().begin();
              t != t_end;
              ++t )
        {
            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (sayPlayers) __ check(2) teammate %d(%.1f %.1f)",
                          (*t)->unum(),
                          (*t)->pos().x, (*t)->pos().y );

            if ( (*t)->seenPosCount() > 0 ) continue;
            //if ( (*t)->seenVelCount() > 0 ) continue;
            if ( (*t)->unumCount() > 0 ) continue;
            if ( (*t)->unum() == Unum_Unknown ) continue;

            if ( M_teammate_send_time[(*t)->unum()].cycle() >= wm.time().cycle() - 1 )
            {
                continue;
            }

            dlog.addText( Logger::COMMUNICATION,
                          __FILE__": (sayPlayers) candidate teammate(2) %d",
                          (*t)->unum() );
            candidates.push_back( *t );
        }
    }


    if ( candidates.empty() )
    {
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayPlayers) no candidate" );
        return false;
    }


    if ( opponent_attack )
    {
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayPlayers) opponent attack. sorted by X." );
        std::sort( candidates.begin(), candidates.end(), AbstractPlayerXCmp() );
    }
    else
    {
        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": (sayPlayers) NO opponent attack. sorted by distance from self." );
        std::sort( candidates.begin(), candidates.end(), AbstractPlayerSelfDistCmp() );
    }

    AbstractPlayerCont::const_iterator first = candidates.begin();
    int first_unum = ( (*first)->side() == wm.ourSide()
                       ? (*first)->unum()
                       : (*first)->unum() + 11 );

    if ( candidates.size() >= 3
         && len + ThreePlayerMessage::slength() <= ServerParam::i().playerSayMsgSize() )
    {
        AbstractPlayerCont::const_iterator second = first; ++second;
        int second_unum = ( (*second)->side() == wm.ourSide()
                            ? (*second)->unum()
                            : (*second)->unum() + 11 );
        AbstractPlayerCont::const_iterator third = second; ++third;
        int third_unum = ( (*third)->side() == wm.ourSide()
                           ? (*third)->unum()
                           : (*third)->unum() + 11 );

        agent->addSayMessage( new ThreePlayerMessage( first_unum,
                                                      (*first)->pos() + (*first)->vel(),
                                                      second_unum,
                                                      (*second)->pos() + (*second)->vel(),
                                                      third_unum,
                                                      (*third)->pos() + (*third)->vel() ) );
        updatePlayerSendTime( wm, (*first)->side(), (*first)->unum() );
        updatePlayerSendTime( wm, (*second)->side(), (*second)->unum() );
        updatePlayerSendTime( wm, (*third)->side(), (*third)->unum() );

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say three players %c%d %c%d %c%d",
                      (*first)->side() == wm.ourSide() ? 'T' : 'O', (*first)->unum(),
                      (*second)->side() == wm.ourSide() ? 'T' : 'O', (*second)->unum(),
                      (*third)->side() == wm.ourSide() ? 'T' : 'O', (*third)->unum() );
    }
    else if ( candidates.size() >= 2
              && len + TwoPlayerMessage::slength() <= ServerParam::i().playerSayMsgSize() )
    {
        AbstractPlayerCont::const_iterator second = first; ++second;
        int second_unum = ( (*second)->side() == wm.ourSide()
                            ? (*second)->unum()
                            : (*second)->unum() + 11 );
        agent->addSayMessage( new TwoPlayerMessage( first_unum,
                                                    (*first)->pos() + (*first)->vel(),
                                                    second_unum,
                                                    (*second)->pos() + (*second)->vel() ) );
        updatePlayerSendTime( wm, (*first)->side(), (*first)->unum() );
        updatePlayerSendTime( wm, (*second)->side(), (*second)->unum() );

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say two players %c%d %c%d",
                      (*first)->side() == wm.ourSide() ? 'T' : 'O', (*first)->unum(),
                      (*second)->side() == wm.ourSide() ? 'T' : 'O', (*second)->unum() );
    }
    else if ( len + OnePlayerMessage::slength() <= ServerParam::i().playerSayMsgSize() )
    {
        agent->addSayMessage( new OnePlayerMessage( first_unum,
                                                    (*first)->pos() + (*first)->vel() ) );
        updatePlayerSendTime( wm, (*first)->side(), (*first)->unum() );

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say one players %c%d",
                      (*first)->side() == wm.ourSide() ? 'T' : 'O', (*first)->unum() );
    }

    s_last_time = wm.time();
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayOpponents( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( sayThreeOpponents( agent ) )
    {
        return true;
    }

    if ( sayTwoOpponents( agent ) )
    {
        return true;
    }

    const int current_len = agent->effector().getSayMessageLength();
    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;
    if ( available_len < OpponentMessage::slength() )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( opp_min > self_min + 10
         && opp_min > mate_min + 10 )
    {
        return false;
    }

    const PlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();
    if ( fastest_opponent
         && fastest_opponent->unum() != Unum_Unknown
         && fastest_opponent->unumCount() == 0
         && fastest_opponent->seenPosCount() == 0
         && fastest_opponent->bodyCount() == 0
         && 10.0 < fastest_opponent->distFromSelf()
         && fastest_opponent->distFromSelf() < 30.0 )
    {
        agent->addSayMessage( new OpponentMessage( fastest_opponent->unum(),
                                                   fastest_opponent->pos(),
                                                   fastest_opponent->body() ) );
        M_opponent_send_time[fastest_opponent->unum()] = wm.time();

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say opponent status" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayTwoOpponents( PlayerAgent * agent )
{
    const int current_len = agent->effector().getSayMessageLength();
    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;
    if ( available_len < TwoPlayerMessage::slength() )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    std::vector< const PlayerObject * > candidates;

    const PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
          o != o_end;
          ++o )
    {
        if ( (*o)->seenPosCount() > 0 ) continue;
        if ( (*o)->unumCount() > 0 ) continue;

        if ( M_opponent_send_time[(*o)->unum()].cycle() >= wm.time().cycle() - 1 )
        {
            continue;
        }

        candidates.push_back( *o );
    }

    if ( candidates.size() < 2 )
    {
        return false;
    }

    std::vector< const PlayerObject * >::const_iterator first = candidates.begin();
    std::vector< const PlayerObject * >::const_iterator second = first; ++second;

    agent->addSayMessage( new TwoPlayerMessage( (*first)->unum() + 11,
                                                (*first)->pos() + (*first)->vel(),
                                                (*second)->unum() + 11,
                                                (*second)->pos() + (*second)->vel() ) );
    M_opponent_send_time[(*first)->unum()] = wm.time();
    M_opponent_send_time[(*second)->unum()] = wm.time();


    dlog.addText( Logger::COMMUNICATION,
                  __FILE__": say two oppoinents %d %d",
                  (*first)->unum(),
                  (*second)->unum() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayThreeOpponents( PlayerAgent * agent )
{
    const int current_len = agent->effector().getSayMessageLength();
    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;
    if ( available_len < ThreePlayerMessage::slength() )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    std::vector< const PlayerObject * > candidates;

    const PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
          o != o_end;
          ++o )
    {
        if ( (*o)->seenPosCount() > 0 ) continue;
        if ( (*o)->unumCount() > 0 ) continue;

        if ( M_opponent_send_time[(*o)->unum()].cycle() >= wm.time().cycle() - 1 )
        {
            continue;
        }

        candidates.push_back( *o );
    }

    if ( candidates.size() < 3 )
    {
        return false;
    }

    std::vector< const PlayerObject * >::const_iterator first = candidates.begin();
    std::vector< const PlayerObject * >::const_iterator second = first; ++second;
    std::vector< const PlayerObject * >::const_iterator third = second; ++third;

    agent->addSayMessage( new ThreePlayerMessage( (*first)->unum() + 11,
                                                  (*first)->pos() + (*first)->vel(),
                                                  (*second)->unum() + 11,
                                                  (*second)->pos() + (*second)->vel(),
                                                  (*third)->unum() + 11,
                                                  (*third)->pos() + (*third)->vel() ) );
    M_opponent_send_time[(*first)->unum()] = wm.time();
    M_opponent_send_time[(*second)->unum()] = wm.time();
    M_opponent_send_time[(*third)->unum()] = wm.time();

    dlog.addText( Logger::COMMUNICATION,
                  __FILE__": say three oppoinents %d %d %d",
                  (*first)->unum(),
                  (*second)->unum(),
                  (*third)->unum() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::saySelf( PlayerAgent * agent )
{
    const int current_len = agent->effector().getSayMessageLength();
    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;
    if ( available_len < SelfMessage::slength() )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    // if ( std::fabs( wm.self().pos().x - wm.defenseLineX() ) > 40.0 )
    // {
    //     return false;
    // }

    if ( std::fabs( wm.self().distFromBall() ) > 35.0 )
    {
        return false;
    }

    if ( M_teammate_send_time[wm.self().unum()].cycle() >= wm.time().cycle() )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( opp_min < self_min
         && opp_min < mate_min )
    {
        return false;
    }

    bool send_self = false;

    if ( current_len > 0 )
    {
        // if another type message is already registered, self info is appended to the message.
        send_self = true;
    }

    if ( ! send_self
         && wm.time().cycle() % 11 == ( wm.self().unum() - 1 ) )
    {
        const PlayerObject * ball_nearest_teammate = NULL;
        const PlayerObject * second_ball_nearest_teammate = NULL;

        const PlayerPtrCont::const_iterator t_end = wm.teammatesFromBall().end();
        for ( PlayerPtrCont::const_iterator t = wm.teammatesFromBall().begin();
              t != t_end;
              ++t )
        {
            if ( (*t)->isGhost() || (*t)->posCount() >= 10 ) continue;

            if ( ! ball_nearest_teammate )
            {
                ball_nearest_teammate = *t;
#ifdef DEBUG_PRINT
                dlog.addText( Logger::COMMUNICATION,
                              __FILE__": (saySelf) ball_nearest_teammate %d (%.1f %.1f)",
                              (*t)->unum(),
                              (*t)->pos().x,  (*t)->pos().y );
#endif
            }
            else if ( ! second_ball_nearest_teammate )
            {
                second_ball_nearest_teammate = *t;
#ifdef DEBUG_PRINT
                dlog.addText( Logger::COMMUNICATION,
                              __FILE__": (saySelf) second_ball_nearest_teammate %d (%.1f %.1f)",
                              (*t)->unum(),
                              (*t)->pos().x,  (*t)->pos().y );
#endif
                break;
            }
        }

        if ( ball_nearest_teammate
             && ball_nearest_teammate->distFromBall() < wm.ball().distFromSelf()
             && ( ! second_ball_nearest_teammate
                  || second_ball_nearest_teammate->distFromBall() > wm.ball().distFromSelf() )
             )
        {
            send_self = true;
        }
    }

    if ( send_self )
    {
        agent->addSayMessage( new SelfMessage( agent->effector().queuedNextMyPos(),
                                               agent->effector().queuedNextMyBody(),
                                               wm.self().stamina() ) );
        M_teammate_send_time[wm.self().unum()] = wm.time();

        dlog.addText( Logger::COMMUNICATION,
                      __FILE__": say self status" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayStamina( PlayerAgent * agent )
{
    const int current_len = agent->effector().getSayMessageLength();
    if ( current_len == 0 )
    {
        return false;
    }

    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;
    if ( available_len < StaminaMessage::slength() )
    {
        return false;
    }

    agent->addSayMessage( new StaminaMessage( agent->world().self().stamina() ) );

    dlog.addText( Logger::COMMUNICATION,
                  __FILE__": say self stamina" );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCommunication::sayRecovery( PlayerAgent * agent )
{
    const int current_len = agent->effector().getSayMessageLength();
    const int available_len = ServerParam::i().playerSayMsgSize() - current_len;
    if ( available_len < RecoveryMessage::slength() )
    {
        return false;
    }

    agent->addSayMessage( new RecoveryMessage( agent->world().self().recovery() ) );

    dlog.addText( Logger::COMMUNICATION,
                  __FILE__": say self recovery" );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SampleCommunication::attentiontoSomeone( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().pos().x > wm.offsideLineX() - 15.0
         && wm.interceptTable()->selfReachCycle() <= 3 )
    {
        if ( currentSenderUnum() != wm.self().unum()
             && currentSenderUnum() != Unum_Unknown )
        {
            agent->doAttentionto( wm.ourSide(), currentSenderUnum() );
            agent->debugClient().addMessage( "AttCurSender%d", currentSenderUnum() );
        }
        else
        {
            std::vector< const PlayerObject * > candidates;
            const PlayerPtrCont::const_iterator end = wm.teammatesFromSelf().end();
            for ( PlayerPtrCont::const_iterator p = wm.teammatesFromSelf().begin();
                  p != end;
                  ++p )
            {
                if ( (*p)->goalie() ) continue;
                if ( (*p)->unum() == Unum_Unknown ) continue;
                if ( (*p)->pos().x > wm.offsideLineX() + 1.0 ) continue;

                if ( (*p)->distFromSelf() > 20.0 ) break;

                candidates.push_back( *p );
            }

            const Vector2D self_next = agent->effector().queuedNextSelfPos();

            const PlayerObject * target_teammate = NULL;
            double max_x = -10000000.0;
            for ( std::vector< const PlayerObject * >::const_iterator p = candidates.begin();
                  p != candidates.end();
                  ++p )
            {
                Vector2D diff = (*p)->pos() + (*p)->vel() - self_next;

                double x = diff.x * ( 1.0 - diff.absY() / 40.0 ) ;

                if ( x > max_x )
                {
                    max_x = x;
                    target_teammate = *p;
                }
            }

            if ( target_teammate )
            {
                dlog.addText( Logger::COMMUNICATION,
                              __FILE__": attentionto most front teammate",
                              wm.offsideLineX() );
                agent->debugClient().addMessage( "AttFrontMate%d", target_teammate->unum() );
                agent->doAttentionto( wm.ourSide(), target_teammate->unum() );
                return;
            }

            // maybe ball owner
            if ( wm.self().attentiontoUnum() > 0 )
            {
                dlog.addText( Logger::COMMUNICATION,
                              __FILE__": attentionto off. maybe ball owner",
                              wm.offsideLineX() );
                agent->debugClient().addMessage( "AttOffBOwner" );
                agent->doAttentiontoOff();
            }
        }

        return;
    }

    const PlayerObject * fastest_teammate = wm.interceptTable()->fastestTeammate();
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( fastest_teammate
         && fastest_teammate->unum() != Unum_Unknown
         && mate_min <= 1
         && mate_min < self_min
         && mate_min <= opp_min + 1
         && mate_min <= 5 + std::min( 4, fastest_teammate->posCount() )
         && wm.ball().inertiaPoint( mate_min ).dist2( agent->effector().queuedNextSelfPos() )
         < std::pow( 35.0, 2 ) )
    {
        // set attention to ball nearest teammate
        agent->debugClient().addMessage( "AttBallOwner%d", fastest_teammate->unum() );
        agent->doAttentionto( wm.ourSide(), fastest_teammate->unum() );
        return;
    }

    const PlayerObject * nearest_teammate = wm.getTeammateNearestToBall( 5 );

    if ( nearest_teammate
         && nearest_teammate->unum() != Unum_Unknown
         && opp_min <= 3
         && opp_min <= mate_min
         && opp_min <= self_min
         && nearest_teammate->distFromSelf() < 45.0
         && nearest_teammate->distFromBall() < 20.0 )
    {
        agent->debugClient().addMessage( "AttBallNearest(1)%d", nearest_teammate->unum() );
        agent->doAttentionto( wm.ourSide(), nearest_teammate->unum() );
        return;
    }

    if ( nearest_teammate
         && nearest_teammate->unum() != Unum_Unknown
         && wm.ball().posCount() >= 3
         && nearest_teammate->distFromBall() < 20.0 )
    {
        agent->debugClient().addMessage( "AttBallNearest(2)%d", nearest_teammate->unum() );
        agent->doAttentionto( wm.ourSide(), nearest_teammate->unum() );
        return;
    }

    if ( nearest_teammate
         && nearest_teammate->unum() != Unum_Unknown
         && nearest_teammate->distFromSelf() < 45.0
         && nearest_teammate->distFromBall() < 3.5 )
    {
        agent->debugClient().addMessage( "AttBallNearest(3)%d", nearest_teammate->unum() );
        agent->doAttentionto( wm.ourSide(), nearest_teammate->unum() );
        return;
    }


    // if ( ( ! wm.self().goalie()
    //        || wm.self().pos().x > ServerParam::i().ourPenaltyAreaLineX() - 1.0 )
    //      && currentSenderUnum() != wm.self().unum()
    //      && currentSenderUnum() != Unum_Unknown )
    // {
    //     agent->doAttentionto( wm.ourSide(), currentSenderUnum() );
    //     agent->debugClient().addMessage( "AttCurrentSender%d", currentSenderUnum() );
    //     return;
    // }

    if ( currentSenderUnum() != wm.self().unum()
         && currentSenderUnum() != Unum_Unknown )
    {
        agent->doAttentionto( wm.ourSide(), currentSenderUnum() );
        agent->debugClient().addMessage( "AttCurSender%d", currentSenderUnum() );
    }
    else
    {
        agent->debugClient().addMessage( "AttOff" );
        agent->doAttentiontoOff();
    }
}
