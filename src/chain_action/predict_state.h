// -*-c++-*-

/*!
  \file predict_state.h
  \brief predicted field state class Header File
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

#ifndef RCSC_PLAYER_PREDICT_STATE_H
#define RCSC_PLAYER_PREDICT_STATE_H

#include "predict_player_object.h"
#include "predict_ball_object.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/player/player_predicate.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/vector_2d.h>

#include <boost/shared_ptr.hpp>

#include <algorithm>

class PredictState {
public:
    static const int VALID_PLAYER_THRESHOLD;

    typedef boost::shared_ptr< PredictState > Ptr; //!< pointer type alias
    typedef boost::shared_ptr< const PredictState > ConstPtr; //!< const pointer type alias

private:
    const rcsc::WorldModel * M_world;
    unsigned long M_spend_time;

    int M_ball_holder_unum;

    PredictBallObject M_ball;

    int M_self_unum;

    PredictPlayerPtrCont M_our_players;


    double M_our_defense_line_x;
    double M_our_offense_player_line_x;


public:

    PredictState( const rcsc::WorldModel & wm );

    PredictState( const PredictState & rhs,
                  unsigned long append_spend_time = 0 );

    PredictState( const PredictState & rhs,
                  unsigned long append_spend_time,
                  int ball_holder_unum,
                  const rcsc::Vector2D & ball_and_holder_pos );

    PredictState( const PredictState & rhs,
                  unsigned long append_spend_time,
                  const rcsc::Vector2D & ball_pos );

private:

    void init( const rcsc::WorldModel & wm );
    void updateLines();

public:

    unsigned long spendTime() const
      {
          return M_spend_time;
      }

    int ballHolderUnum() const
      {
          return M_ball_holder_unum;
      }

    const rcsc::AbstractPlayerObject * ballHolder() const
      {
          if ( ballHolderUnum() == rcsc::Unum_Unknown )
          {
              return static_cast< rcsc::AbstractPlayerObject * >( 0 );
          }
          else
          {
              return ourPlayer( ballHolderUnum() );
          }
      }

    const PredictBallObject & ball() const
      {
          return M_ball;
      }

    const rcsc::AbstractPlayerObject & self() const
      {
          if ( M_self_unum < 1 || 11 < M_self_unum )
          {
              std::cerr << "internal error: "
                        << __FILE__ << ":" << __LINE__
                        << "invalid self unum " << M_self_unum << std::endl;
              return *(M_our_players[0]);
          }

          return *(M_our_players[ M_self_unum - 1 ]);
      }

    const rcsc::AbstractPlayerObject * ourPlayer( const int unum ) const
      {
          if ( unum < 1 || 11 < unum  )
          {
              std::cerr << "internal error: "
                        << __FILE__ << ":" << __LINE__ << ": "
                        << "invalid unum " << unum << std::endl;
              return static_cast< const rcsc::AbstractPlayerObject * >( 0 );
          }

          return &( *(M_our_players[ unum - 1 ]) );
      }

    const rcsc::AbstractPlayerObject * theirPlayer( const int unum ) const
      {
          return M_world->theirPlayer( unum );
      }

    const PredictPlayerPtrCont & ourPlayers() const
      {
          return M_our_players;
      }

    const rcsc::PlayerCont & opponents() const
      {
          return M_world->opponents();
      }

    const rcsc::AbstractPlayerCont & theirPlayers() const
      {
          return M_world->theirPlayers();
      }

    const rcsc::PlayerPtrCont & opponentsFromSelf() const
      {
          return M_world->opponentsFromSelf();
      }

    const rcsc::PlayerObject * getOpponentNearestTo( const rcsc::Vector2D & point,
                                                     const int count_thr,
                                                     double * dist_to_point ) const
      {
          return M_world->getOpponentNearestTo( point, count_thr, dist_to_point );
      }

    const rcsc::PlayerType * ourPlayerType( const int unum ) const
      {
          return M_world->ourPlayerType( unum );
      }

    const rcsc::AbstractPlayerObject * getOurGoalie() const
      {
          if ( M_world->ourGoalieUnum() == rcsc::Unum_Unknown )
          {
              return static_cast< rcsc::AbstractPlayerObject * >( 0 );
          }

          return ourPlayer( M_world->ourGoalieUnum() );
      }

    const rcsc::PlayerObject * getOpponentGoalie() const
      {
          return M_world->getOpponentGoalie();
      }

    const rcsc::GameMode & gameMode() const
      {
          return M_world->gameMode();
      }

    const rcsc::GameTime & currentTime() const
      {
          return M_world->time();
      }

    rcsc::SideID ourSide() const
      {
          return M_world->ourSide();
      }

    double offsideLineX() const
      {
          return std::max( M_world->offsideLineX(), M_ball.pos().x );
      }

    double ourDefenseLineX() const
      {
          return M_our_defense_line_x;
      }

    double ourOffensePlayerLineX() const
      {
          return M_our_offense_player_line_x;
      }

    double theirDefensePlayerLineX() const
      {
          return M_world->theirDefensePlayerLineX();
      }

    int dirCount( const rcsc::AngleDeg & angle )
      {
          return M_world->dirCount( angle );
      }

    int dirRangeCount( const rcsc::AngleDeg & angle,
                       const double & width,
                       int * max_count,
                       int * sun_count,
                       int * ave_count ) const
      {
          return M_world->dirRangeCount( angle, width,
                                         max_count, sun_count, ave_count );
      }

    const rcsc::AudioMemory & audioMemory() const
      {
          return M_world->audioMemory();
      }

    rcsc::AbstractPlayerCont getPlayerCont( const rcsc::PlayerPredicate * predicate ) const;
};

#endif
