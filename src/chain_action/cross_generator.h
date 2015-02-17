// -*-c++-*-

/*!
  \file cross_generator.h
  \brief cross pass generator Header File
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

#include "pass.h"

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class PlayerObject;
class WorldModel;
}


class CrossGenerator {
private:
    int M_total_count;

    const rcsc::AbstractPlayerObject * M_passer; //!< estimated passer
    rcsc::Vector2D M_first_point;

    rcsc::AbstractPlayerCont M_receiver_candidates;
    rcsc::AbstractPlayerCont M_opponents;

    std::vector< CooperativeAction::Ptr > M_courses;


    // private for singleton
    CrossGenerator();

    // not used
    CrossGenerator( const CrossGenerator & );
    CrossGenerator & operator=( const CrossGenerator & );

public:

    static
    CrossGenerator & instance();

    void generate( const rcsc::WorldModel & wm );

    const std::vector< CooperativeAction::Ptr > & courses( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_courses;
      }

private:

    void clear();

    void updatePasser( const rcsc::WorldModel & wm );
    void updateReceivers( const rcsc::WorldModel & wm );
    void updateOpponents( const rcsc::WorldModel & wm );

    void createCourses( const rcsc::WorldModel & wm );
    void createCross( const rcsc::WorldModel & wm,
                      const rcsc::AbstractPlayerObject * receiver );

    bool checkOpponent( const rcsc::Vector2D & first_ball_pos,
                        const rcsc::AbstractPlayerObject * receiver,
                        const rcsc::Vector2D & receive_pos,
                        const double & first_ball_speed,
                        const rcsc::AngleDeg & ball_move_angle,
                        const int max_cycle );

    double getMinimumAngleWidth( const double & target_dist,
                                 const rcsc::AngleDeg & target_angle );

};

#endif
