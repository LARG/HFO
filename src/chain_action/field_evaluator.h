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

#ifndef RCSC_PLAYER_FIELD_EVALUATOR_H
#define RCSC_PLAYER_FIELD_EVALUATOR_H

/////////////////////////////////////////////////////////////////////

#include "cooperative_action.h"
#include "predict_state.h"
#include "action_state_pair.h"

#include <rcsc/player/world_model.h>

#include <boost/shared_ptr.hpp>

#include <vector>

/*!
  \class FieldEvaluator
  \brief abstract field evaluator function object class
*/
class FieldEvaluator {
public:

    typedef boost::shared_ptr< FieldEvaluator > Ptr; //!< pointer type alias
    typedef boost::shared_ptr< const FieldEvaluator > ConstPtr; //!< const pointer type alias

protected:
    /*!
      \brief protected constructor to inhibit instantiation of this class
     */
    FieldEvaluator()
      { }

public:
    /*!
      \brief virtual destructor
     */
    virtual
    ~FieldEvaluator()
      { }

    /*!
      \brief evaluation function
      \return evaluation value of world model
     */
    virtual
    double operator() ( const PredictState & state,
                        const std::vector< ActionStatePair > & path ) const = 0;
};

#endif
