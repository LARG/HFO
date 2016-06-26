// -*-c++-*-

/*!
  \file action_chain_holder.h
  \brief holder of calculated chain action Source File
*/

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "action_chain_holder.h"
#include <rcsc/player/world_model.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
ActionChainHolder::ActionChainHolder()
    : M_graph(),
      M_evaluator(),
      M_generator()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
ActionChainHolder &
ActionChainHolder::instance()
{
#ifdef __APPLE__
    static ActionChainHolder s_instance;
#else
    static thread_local ActionChainHolder s_instance;
#endif
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
const
ActionChainHolder &
ActionChainHolder::i()
{
    return instance();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainHolder::setFieldEvaluator( const FieldEvaluator::ConstPtr & evaluator )
{
    M_evaluator = evaluator;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainHolder::setActionGenerator( const ActionGenerator::ConstPtr & generator )
{
    M_generator = generator;
}

/*-------------------------------------------------------------------*/
/*!

 */
FieldEvaluator::ConstPtr
ActionChainHolder::fieldEvaluator() const
{
    return M_evaluator;
}

/*-------------------------------------------------------------------*/
/*!

 */
ActionGenerator::ConstPtr
ActionChainHolder::actionGenerator() const
{
    return M_generator;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainHolder::update( const WorldModel & wm )
{
#ifdef __APPLE__
    static GameTime s_update_time( 0, 0 );
    static FieldEvaluator::ConstPtr s_update_evaluator;
    static ActionGenerator::ConstPtr s_update_generator;
#else
    static thread_local GameTime s_update_time( 0, 0 );
    static thread_local FieldEvaluator::ConstPtr s_update_evaluator;
    static thread_local ActionGenerator::ConstPtr s_update_generator;
#endif

    if ( s_update_time == wm.time()
         && s_update_evaluator == M_evaluator
         && s_update_generator == M_generator )
    {
        return;
    }
    s_update_time = wm.time();
    s_update_evaluator = M_evaluator;
    s_update_generator = M_generator;

    M_graph = ActionChainGraph::Ptr( new ActionChainGraph( M_evaluator, M_generator ) );
    M_graph->calculate( wm );
}

/*-------------------------------------------------------------------*/
/*!

 */
const ActionChainGraph &
ActionChainHolder::graph() const
{
    return *M_graph;
}
