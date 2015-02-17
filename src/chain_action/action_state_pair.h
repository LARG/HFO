// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#ifndef RCSC_PLAYER_ACTION_STATE_PAIR_H
#define RCSC_PLAYER_ACTION_STATE_PAIR_H

#include "predict_state.h"

#include <boost/shared_ptr.hpp>

#include <vector>

class CooperativeAction;

/*!
  \class ActionStatePair
  \brief a pair of a cooperative action and predicted state when this action performed
*/
class ActionStatePair {
public:
    boost::shared_ptr< const CooperativeAction > M_action;
    boost::shared_ptr< const PredictState > M_state;

    // not used
    ActionStatePair();

public:

    /*!
      \brief copy constructor
      \param rhs copied object
     */
    ActionStatePair( const ActionStatePair & rhs )
        : M_action( rhs.M_action ),
          M_state( rhs.M_state )
      { }

    /*!
      \brief substitution operator
      \param rhs right hand side value
      \return reference to this object
     */
    ActionStatePair & operator=( const ActionStatePair & rhs )
      {
          if ( this != &rhs )
          {
              this->M_action = rhs.M_action;
              this->M_state = rhs.M_state;
          }
          return *this;
      }

    ActionStatePair( const CooperativeAction * action,
                     const PredictState * state )
        : M_action( action ),
          M_state( state )
      { }

    ActionStatePair( const boost::shared_ptr< const CooperativeAction > & action,
                     const PredictState * state )
        : M_action( action ),
          M_state( state )
      { }

    ActionStatePair( const CooperativeAction * action,
                     const boost::shared_ptr< const PredictState > & state )
        : M_action( action ),
          M_state( state )
      { }

    ActionStatePair( const boost::shared_ptr< const CooperativeAction > & action,
                     const boost::shared_ptr< const PredictState > & state )
        : M_action( action ),
          M_state( state )
      { }

    const CooperativeAction & action() const
      {
          return *M_action;
      }

    const PredictState & state() const
      {
          return *M_state;
      }

    const boost::shared_ptr< const CooperativeAction > & actionPtr() const
      {
          return M_action;
      }

    const boost::shared_ptr< const PredictState > & statePtr() const
      {
          return M_state;
      }
};

#endif
