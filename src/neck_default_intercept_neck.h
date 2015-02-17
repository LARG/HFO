// -*-c++-*-

/*!
  \file neck_default_intercept_neck.h
  \brief default neck action to use with intercept action
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

#ifndef NECK_DEFAULT_INTERCEPT_NECK_H
#define NECK_DEFAULT_INTERCEPT_NECK_H

#include <rcsc/player/soccer_action.h>
#include "action_chain_holder.h"

namespace rcsc {
class PlayerAgent;
class ViewAction;
}

class ActionChainGraph;

/*!
  \class NeckDefaultInterceptNeck
  \brief default neck action to use with intercept action
*/
class Neck_DefaultInterceptNeck
    : public rcsc::NeckAction {
private:
    const ActionChainGraph & M_chain_graph;

    rcsc::ViewAction * M_default_view_act;
    rcsc::NeckAction * M_default_neck_act;


public:
    /*!
      \brief constructor
     */
    explicit
    Neck_DefaultInterceptNeck( const ActionChainGraph & chain_graph,
                               rcsc::ViewAction * view = static_cast< rcsc::ViewAction * >( 0 ),
                               rcsc::NeckAction * neck = static_cast< rcsc::NeckAction * >( 0 ) )
        : M_chain_graph( chain_graph ),
          M_default_view_act( view ),
          M_default_neck_act( neck )
      { }

    explicit
    Neck_DefaultInterceptNeck( rcsc::ViewAction * view = static_cast< rcsc::ViewAction * >( 0 ),
                               rcsc::NeckAction * neck = static_cast< rcsc::NeckAction * >( 0 ) )
        : M_chain_graph( ActionChainHolder::i().graph() ),
          M_default_view_act( view ),
          M_default_neck_act( neck )
      { }

    explicit
    Neck_DefaultInterceptNeck( rcsc::NeckAction * neck = static_cast< rcsc::NeckAction * >( 0 ) )
        : M_chain_graph( ActionChainHolder::i().graph() ),
          M_default_view_act( static_cast< rcsc::ViewAction * >( 0 ) ),
          M_default_neck_act( neck )
      { }

    /*!
      \brief destruct sub actions if exists
     */
    ~Neck_DefaultInterceptNeck();

    /*!
      \brief default neck action to use with intercept action
      \param agent pointer to the agent itself
      \return true with action, false if not performed
     */
    bool execute( rcsc::PlayerAgent * agent );

    /*!
      \brief create copy of this object
      \return copy of this object
    */
    rcsc::NeckAction * clone() const;

private:

    /*!
      \brief implementation of this action
      \param agent pointer to the agent itself
      \return true with action, false if not performed
     */
    bool doTurnToReceiver( rcsc::PlayerAgent * agent );
};

#endif
