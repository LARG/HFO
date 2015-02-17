// -*-c++-*-

/*!
  \file shoot_generator.h
  \brief shoot course generator class Header File
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

#ifndef SHOOT_GENERATOR_H
#define SHOOT_GENERATOR_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <functional>
#include <vector>

namespace rcsc {
class PlayerAgent;
class PlayerObject;
class WorldModel;
}

/*!
  \class ShootGenerator
  \brief shoot course generator
 */
class ShootGenerator {
public:

    /*!
      \struct Course
      \brief shoot course
     */
    struct Course {
        int index_;
        rcsc::Vector2D target_point_; //!< target point on the goal line
        rcsc::Vector2D first_ball_vel_; //!< first ball velocity
        double first_ball_speed_; //!< first ball speed
        rcsc::AngleDeg ball_move_angle_; //!< shoot angle
        double ball_move_dist_; //!< ball move distance
        int ball_reach_step_; //!< ball reach step
        bool goalie_never_reach_; //!< true if goalie never reach the ball
        bool opponent_never_reach_; //!< true if goalie never reach the ball
        int kick_step_; //!< estimated kick step
        double score_; //!< evaluated value of this shoot

        /*!
          \brief constructor
          \param point shoot target point on the goal line
          \param speed first ball speed
          \param angle shoot angle
         */
        Course( const int index,
                const rcsc::Vector2D & target_point,
                const double & first_ball_speed,
                const rcsc::AngleDeg & ball_move_angle,
                const double & ball_move_dist,
                const int ball_reach_step )
            : index_( index ),
              target_point_( target_point ),
              first_ball_vel_( rcsc::Vector2D::polar2vector( first_ball_speed,
                                                             ball_move_angle ) ),
              first_ball_speed_( first_ball_speed ),
              ball_move_angle_( ball_move_angle ),
              ball_move_dist_( ball_move_dist ),
              ball_reach_step_( ball_reach_step ),
              goalie_never_reach_( true ),
              opponent_never_reach_( true ),
              kick_step_( 3 ),
              score_( 0 )
          { }
    };

    //! type of the Shot container
    typedef std::vector< Course > Container;

    /*!
      \struct ScoreCmp
      \brief function object to evaluate the shoot object
     */
    struct ScoreCmp
        : public std::binary_function< Course,
                                       Course,
                                       bool > {
        /*!
          \brief compare operator
          \param lhs left hand side argument
          \param rhs right hand side argument
         */
        result_type operator()( const first_argument_type & lhs,
                                const second_argument_type & rhs ) const
          {
              return lhs.score_ > rhs.score_;
          }
    };

private:

    //! search count
    int M_total_count;

    //! first ball position
    rcsc::Vector2D M_first_ball_pos;

    //! cached calculated shoot pathes
    Container M_courses;

    // private for singleton
    ShootGenerator();

    // not used
    ShootGenerator( const ShootGenerator & );
    const ShootGenerator & operator=( const ShootGenerator & );
public:

    static
    ShootGenerator & instance();

    void generate( const rcsc::WorldModel & wm );

    /*!
      \brief calculate the shoot and return the container
      \param agent const pointer to the agent
      \return const reference to the shoot container
     */
    const Container & courses( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_courses;
      }

private:

    void clear();

    void createShoot( const rcsc::WorldModel & wm,
                      const rcsc::Vector2D & target_point );

    bool createShoot( const rcsc::WorldModel & wm,
                      const rcsc::Vector2D & target_point,
                      const double & first_ball_speed,
                      const rcsc::AngleDeg & ball_move_angle,
                      const double & ball_move_dist );

    bool maybeGoalieCatch( const rcsc::PlayerObject * goalie,
                           Course & course );

    bool opponentCanReach( const rcsc::PlayerObject * opponent,
                           Course & course );

    void evaluateCourses( const rcsc::WorldModel & wm );
};

#endif
