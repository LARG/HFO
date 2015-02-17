// -*-c++-*-

/*!
  \file tackle_generator.h
  \brief tackle/foul generator class Header File
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

#ifndef TACKLE_GENERATOR_H
#define TACKLE_GENERATOR_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class AbstractPlayerObject;
class WorldModel;
}

/*!
  \class TackleGenerator
  \brief tackle/foul generator
 */
class TackleGenerator {
public:

    struct TackleResult {
        rcsc::AngleDeg tackle_angle_; //!< global angle
        rcsc::Vector2D ball_vel_; //!< result ball velocity
        double ball_speed_;
        rcsc::AngleDeg ball_move_angle_;
        double score_;

        TackleResult();
        TackleResult( const rcsc::AngleDeg & angle,
                      const rcsc::Vector2D & vel );
        void clear();
    };

    typedef std::vector< TackleResult > Container;


private:

    //! candidate container
    Container M_candidates;

    //! best tackle result
    TackleResult M_best_result;

    // private for singleton
    TackleGenerator();

    // not used
    TackleGenerator( const TackleGenerator & );
    const TackleGenerator & operator=( const TackleGenerator & );
public:

    static
    TackleGenerator & instance();

    void generate( const rcsc::WorldModel & wm );


    const Container & candidates( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_candidates;
      }

    const TackleResult & bestResult( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_best_result;
      }

private:

    void clear();

    void calculate( const rcsc::WorldModel & wm );
    double evaluate( const rcsc::WorldModel & wm,
                     const TackleResult & result );

    int predictOpponentsReachStep( const rcsc::WorldModel & wm,
                                   const rcsc::Vector2D & first_ball_pos,
                                   const rcsc::Vector2D & first_ball_vel,
                                   const rcsc::AngleDeg & ball_move_angle );
    int predictOpponentReachStep( const rcsc::AbstractPlayerObject * opponent,
                                  const rcsc::Vector2D & first_ball_pos,
                                  const rcsc::Vector2D & first_ball_vel,
                                  const rcsc::AngleDeg & ball_move_angle,
                                  const int max_cycle );

};

#endif
