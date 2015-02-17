// -*-c++-*-

/*!
  \file self_pass_generator.h
  \brief self pass generator Header File
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

#ifndef SELF_PASS_GENERATOR_H
#define SELF_PASS_GENERATOR_H

#include "cooperative_action.h"

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class PlayerObject;
class WorldModel;
}

class SelfPassGenerator {
private:

    rcsc::GameTime M_update_time;
    int M_total_count;

    std::vector< CooperativeAction::Ptr > M_courses;

    // private for singleton
    SelfPassGenerator();

    // not used
    SelfPassGenerator( const SelfPassGenerator & );
    SelfPassGenerator & operator=( const SelfPassGenerator & );
public:

    static
    SelfPassGenerator & instance();

    void generate( const rcsc::WorldModel & wm );

    const std::vector< CooperativeAction::Ptr > & courses( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_courses;
      }

private:

    void clear();

    void createCourses( const rcsc::WorldModel & wm );

    void createSelfCache( const rcsc::WorldModel & wm,
                          const rcsc::AngleDeg & dash_angle,
                          const int n_turn,
                          const int n_dash,
                          std::vector< rcsc::Vector2D > & self_cache );

    bool canKick( const rcsc::WorldModel & wm,
                  const int n_turn,
                  const int n_dash,
                  const rcsc::Vector2D & receive_pos );

    bool checkOpponent( const rcsc::WorldModel & wm,
                        const int n_turn,
                        const int n_dash,
                        const rcsc::Vector2D & ball_pos,
                        const rcsc::Vector2D & receive_pos );

};

#endif
