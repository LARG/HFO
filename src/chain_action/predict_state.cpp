// -*-c++-*-

/*!
  \file predict_state.cpp
  \brief predicted field state class Source File
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa AKIYAMA

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

#include "predict_state.h"

#include <rcsc/common/server_param.h>

#include <algorithm>

using namespace rcsc;

//#define STRICT_LINE_UPDATE

const int PredictState::VALID_PLAYER_THRESHOLD = 8;

/*-------------------------------------------------------------------*/
/*!

 */
PredictState::PredictState( const WorldModel & wm )
    : M_world( &wm ),
      M_spend_time( 0 ),
      M_ball_holder_unum( Unum_Unknown ),
      M_ball(),
      M_self_unum( Unum_Unknown ),
      M_our_players(),
      M_our_defense_line_x( 0.0 ),
      M_our_offense_player_line_x( 0.0 )
{
    init( wm );
}

/*-------------------------------------------------------------------*/
/*!

 */
PredictState::PredictState( const PredictState & rhs,
                            unsigned long append_spend_time )
    : M_world( rhs.M_world ),
      M_spend_time( rhs.spendTime() + append_spend_time ),
      M_ball_holder_unum( rhs.ballHolderUnum() ),
      M_ball( rhs.M_ball ),
      M_self_unum( rhs.M_self_unum ),
      M_our_players( rhs.M_our_players ),
      M_our_defense_line_x( rhs.M_our_defense_line_x ),
      M_our_offense_player_line_x( rhs.M_our_offense_player_line_x )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
PredictState::PredictState( const PredictState & rhs,
                            unsigned long append_spend_time,
                            int ball_holder_unum,
                            const Vector2D & ball_and_holder_pos )
    : M_world( rhs.M_world ),
      M_spend_time( rhs.M_spend_time + append_spend_time ),
      M_ball_holder_unum( ball_holder_unum ),
      M_ball( ball_and_holder_pos ),
      M_self_unum( rhs.M_self_unum ),
      M_our_players( rhs.M_our_players ),
      M_our_defense_line_x( rhs.M_our_defense_line_x ),
      M_our_offense_player_line_x( std::max( rhs.M_our_offense_player_line_x,
                                             ball_and_holder_pos.x ) )
{
    M_our_players[ ball_holder_unum - 1 ]
        = PredictPlayerObject::Ptr( new PredictPlayerObject( *(M_our_players[ ball_holder_unum - 1 ] ),
                                                             ball_and_holder_pos ) );

    updateLines();
}

/*-------------------------------------------------------------------*/
/*!

 */
PredictState::PredictState( const PredictState & rhs,
                            unsigned long append_spend_time,
                            const Vector2D & ball_pos )
    : M_world( rhs.M_world ),
      M_spend_time( rhs.M_spend_time + append_spend_time ),
      M_ball_holder_unum( rhs.M_ball_holder_unum ),
      M_ball( ball_pos ),
      M_self_unum( rhs.M_self_unum ),
      M_our_players( rhs.M_our_players ),
      M_our_defense_line_x( rhs.M_our_defense_line_x ),
      M_our_offense_player_line_x( rhs.M_our_offense_player_line_x )
{
    updateLines();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::init( const WorldModel & wm )
{
    //
    // initialize world
    //
    M_world = &wm;

    //
    // initialize spend_time
    //
    M_spend_time = 0;

    //
    // initialize ball pos & vel
    //
    M_ball.assign( wm.ball().pos(), wm.ball().vel() );

    //
    // initialize self_unum
    //
    M_self_unum = wm.self().unum();

    //
    // initialize ball holder
    //
    const AbstractPlayerObject * h = wm.getTeammateNearestToBall( VALID_PLAYER_THRESHOLD );

    if ( h
         && wm.ball().pos().dist2( h->pos() ) < wm.ball().pos().dist2( wm.self().pos() ) )
    {
        M_ball_holder_unum = h->unum();
    }
    else
    {
        M_ball_holder_unum = wm.self().unum();
    }

    //
    // initialize all teammates
    //
    M_our_players.reserve( 11 );

    for ( int n = 1; n <= 11; ++n )
    {
        PredictPlayerObject::Ptr ptr;

        if ( n == M_self_unum )
        {
            ptr = PredictPlayerObject::Ptr( new PredictPlayerObject( wm.self() ) );
        }
        else
        {
            const AbstractPlayerObject * t = wm.ourPlayer( n );

            if ( t )
            {
                ptr = PredictPlayerObject::Ptr( new PredictPlayerObject( *t ) );

            }
            else
            {
                ptr = PredictPlayerObject::Ptr( new PredictPlayerObject() );
            }
        }

        M_our_players.push_back( ptr );

#ifndef STRICT_LINE_UPDATE
        if ( ptr->isValid()
             && M_our_offense_player_line_x < ptr->pos().x )
        {
            M_our_offense_player_line_x = ptr->pos().x;
        }
#endif
    }

    updateLines();
}

void
PredictState::updateLines()
{
    // XXX: tentative implementation, should consider M_our_players
    M_our_defense_line_x = std::min( M_world->ourDefenseLineX(), M_ball.pos().x );

#ifdef STRICT_LINE_UPDATE
    M_our_offense_player_line_x = ServerParam::i().ourTeamGoalLineX();

    const PredictPlayerPtrCont::const_iterator t_end = M_all_teammates.end();
    for ( PredictPlayerPtrCont::const_iterator it = M_all_teammates.begin();
          it != t_end;
          ++it )
    {
        if ( (*it)->isValid()
             && M_our_offense_player_line_x < (*it)->pos().x )
        {
            M_our_offense_player_line_x = (*it)->pos().x;
        }
    }
#endif
}


/*-------------------------------------------------------------------*/
/*!

 */
AbstractPlayerCont
PredictState::getPlayerCont( const PlayerPredicate * predicate ) const
{
    AbstractPlayerCont ret;

    if ( ! predicate )
    {
        return ret;
    }

    for ( PredictPlayerPtrCont::const_iterator
              it = M_our_players.begin(),
              end = M_our_players.end();
          it != end;
          ++it )
    {
        if ( (*predicate)( **it ) )
        {
            ret.push_back( &(**it) );
        }
    }

    for ( PlayerPtrCont::const_iterator
              it = M_world->opponentsFromSelf().begin(),
              end = M_world->opponentsFromSelf().end();
          it != end;
          ++it )
    {
        if ( (*predicate)( **it ) )
        {
            ret.push_back( *it );
        }
    }

    delete predicate;

    return ret;
}
