// -*-c++-*-

/*!
  \file short_dribble_generator.h
  \brief short step dribble course generator Header File
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

#ifndef SHORT_DRIBBLE_GENERATOR_H
#define SHORT_DRIBBLE_GENERATOR_H

#include "cooperative_action.h"

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class PlayerObject;
class WorldModel;
}

class ShortDribbleGenerator {
private:

    rcsc::GameTime M_update_time;
    int M_total_count;

    rcsc::Vector2D M_first_ball_pos;
    rcsc::Vector2D M_first_ball_vel;

    rcsc::GameTime M_queued_action_time;
    CooperativeAction::Ptr M_queued_action;

    std::vector< CooperativeAction::Ptr > M_courses;

    // private for singleton
    ShortDribbleGenerator();

    // not used
    ShortDribbleGenerator( const ShortDribbleGenerator & );
    ShortDribbleGenerator & operator=( const ShortDribbleGenerator & );
public:

    static
    ShortDribbleGenerator & instance();

    void generate( const rcsc::WorldModel & wm );

    void setQueuedAction( const rcsc::WorldModel & wm,
                          CooperativeAction::Ptr action );

    const std::vector< CooperativeAction::Ptr > & courses( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_courses;
      }

private:

    void clear();

    void createCourses( const rcsc::WorldModel & wm );

    void simulateDashes( const rcsc::WorldModel & wm );

    void simulateKickTurnsDashes( const rcsc::WorldModel & wm,
                                  const rcsc::AngleDeg & dash_angle,
                                  const int n_turn );

    void createSelfCache( const rcsc::WorldModel & wm,
                          const rcsc::AngleDeg & dash_angle,
                          const int n_turn,
                          const int n_dash,
                          std::vector< rcsc::Vector2D > & self_cache );

    bool checkOpponent( const rcsc::WorldModel & wm,
                        const rcsc::Vector2D & ball_trap_pos,
                        const int dribble_step );
};

#endif
