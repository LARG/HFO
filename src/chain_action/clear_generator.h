// -*-c++-*-

/*!
  \file clear_generator.h
  \brief clear kick generator Header File
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

#ifndef CROSS_GENERATOR_H
#define CROSS_GENERATOR_H

#include "cooperative_action.h"

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class AbstractPlayerObject;
class PlayerObject;
class WorldModel;
}


class ClearGenerator {
private:

    rcsc::GameTime M_update_time;
    int M_total_count;

    std::vector< CooperativeAction::Ptr > M_courses;


    // private for singleton
    ClearGenerator();

    // not used
    ClearGenerator( const ClearGenerator & );
    ClearGenerator & operator=( const ClearGenerator & );

public:

    static
    ClearGenerator & instance();

    void generate( const rcsc::WorldModel & wm );

    const std::vector< CooperativeAction::Ptr > & courses( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_courses;
      }

private:

    void clear();

    void createCourses( const rcsc::WorldModel & wm );

    int predictOpponentsReachStep( const rcsc::WorldModel & wm,
                                   const rcsc::Vector2D & first_ball_pos,
                                   const double & first_ball_speed,
                                   const rcsc::AngleDeg & ball_move_angle );
    int predictOpponentReachStep( const rcsc::AbstractPlayerObject * opponent,
                                  const rcsc::Vector2D & first_ball_pos,
                                  const rcsc::Vector2D & first_ball_vel,
                                  const rcsc::AngleDeg & ball_move_angle,
                                  const int max_cycle );
};

#endif
