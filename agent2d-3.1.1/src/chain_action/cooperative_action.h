// -*-c++-*-

/*!
  \file cooperative_action.h
  \brief cooperative action type Header File.
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

#ifndef COOPERATIVE_ACTION_H
#define COOPERATIVE_ACTION_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>
#include <rcsc/types.h>

#include <boost/shared_ptr.hpp>

/*!
  \class CooperativeAction
  \brief abstract cooperative action class
 */
class CooperativeAction {
public:

    typedef boost::shared_ptr< CooperativeAction > Ptr; //!< pointer type
    typedef boost::shared_ptr< const CooperativeAction > ConstPtr; //!< const pointer type

    /*!
      \enum ActionCategory
      \brief action category enumeration.
     */
    enum ActionCategory {
        Hold,
        Dribble,
        Pass,
        Shoot,
        Clear,
        Move,

        NoAction,
    };

private:

    ActionCategory M_category; //!< action category type
    int M_index;

    int M_player_unum; //!< acting player's uniform number
    int M_target_player_unum; //!< action target player's uniform number

    rcsc::Vector2D M_target_point; //!< action end point

    double M_first_ball_speed; //!< first ball speed (if necessary)
    double M_first_turn_moment; //!< first turn moment (if necessary)
    double M_first_dash_power; //!< first dash speed (if necessary)
    rcsc::AngleDeg M_first_dash_angle; //!< first dash angle (relative to body) (if necessary)

    int M_duration_step; //!< action duration period

    int M_kick_count; //!< kick count (if necessary)
    int M_turn_count; //!< dash count (if necessary)
    int M_dash_count; //!< dash count (if necessary)

    bool M_final_action; //!< if this value is true, this action is the final one of action chain.

    const char * M_description; //!< description message for debugging purpose

    // not used
    CooperativeAction();
    CooperativeAction( const CooperativeAction & );
    CooperativeAction & operator=( const CooperativeAction & );
protected:

    /*!
      \brief construct with necessary variables
      \param category action category type
      \param player_unum action executor player's unum
      \param target_point action target point
      \param duration_step this action's duration step
      \param description description message (must be a literal character string)
     */
    CooperativeAction( const ActionCategory & category,
                       const int player_unum,
                       const rcsc::Vector2D & target_point,
                       const int duration_step,
                       const char * description );

    void setCategory( const ActionCategory & category );
    void setPlayerUnum( const int unum );
    void setTargetPlayerUnum( const int unum );
    void setTargetPoint( const rcsc::Vector2D & point );

public:

    /*!
      \brief virtual destructor.
     */
    virtual
    ~CooperativeAction()
      { }

    void setIndex( const int i ) { M_index = i; }

    void setFirstBallSpeed( const double & speed );
    void setFirstTurnMoment( const double & moment );
    void setFirstDashPower( const double & power );
    void setFirstDashAngle( const rcsc::AngleDeg & angle );

    void setDurationStep( const int duration_step );

    void setKickCount( const int count );
    void setTurnCount( const int count );
    void setDashCount( const int count );

    void setFinalAction( const bool on );

    void setDescription( const char * description );

    /*!
     */
    const ActionCategory & category() const { return M_category; }

    /*!
     */
    int index() const { return M_index; }

    /*!
     */
    const char * description() const { return M_description; }

    /*!
     */
    int playerUnum() const { return M_player_unum; }

    /*!
     */
    int targetPlayerUnum() const { return M_target_player_unum; }

    /*!
     */
    const rcsc::Vector2D & targetPoint() const { return M_target_point; }

    /*!
     */
    const double & firstBallSpeed() const { return M_first_ball_speed; }

    /*!
     */
    const double & firstTurnMoment() const { return M_first_turn_moment; }

    /*!
     */
    const double & firstDashPower() const { return M_first_dash_power; }

    /*!
     */
    const rcsc::AngleDeg & firstDashAngle() const { return M_first_dash_angle; }

    /*!
     */
    int durationStep() const { return M_duration_step; }

    /*!
     */
    int kickCount() const { return M_kick_count; }

    /*!
     */
    int turnCount() const { return M_turn_count; }

    /*!
     */
    int dashCount() const { return M_dash_count; }

    /*!
     */
    bool isFinalAction() const { return M_final_action; }



    //
    //
    //

    struct DistCompare {
        const rcsc::Vector2D pos_;

    private:
        DistCompare();
    public:

        DistCompare( const rcsc::Vector2D & pos )
            : pos_( pos )
          { }

        bool operator()( const Ptr & lhs,
                         const Ptr & rhs ) const
          {
              return lhs->targetPoint().dist2( pos_ ) < rhs->targetPoint().dist2( pos_ );
          }

    };


};

#endif
