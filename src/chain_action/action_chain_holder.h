// -*-c++-*-

/*!
  \file action_chain_holder.h
  \brief holder of calculated chain action Header File
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

#ifndef ACTION_CHAIN_HOLDER_H
#define ACTION_CHAIN_HOLDER_H

namespace rcsc {
class WorldModel;
}

#include "action_chain_graph.h"
#include "field_evaluator.h"
#include "action_generator.h"

class ActionChainHolder {

private:
    ActionChainGraph::Ptr M_graph;
    FieldEvaluator::ConstPtr M_evaluator;
    ActionGenerator::ConstPtr M_generator;

private:
    /*!
      \brief private constructor to inhibit instantiation expect singleton
     */
    ActionChainHolder();

    /*!
      \brief private constructor to inhibit instantiation expect singleton
     */
    ActionChainHolder( const ActionChainHolder & );

    /*!
      \brief private operator=
     */
    const ActionChainHolder & operator=( const ActionChainHolder & );

public:
    static
    ActionChainHolder & instance();

    static
    const
    ActionChainHolder & i();

    bool initialized() { return M_graph != NULL; }

public:
    void setFieldEvaluator( const FieldEvaluator::ConstPtr & evaluator );
    void setActionGenerator( const ActionGenerator::ConstPtr & generator );

    FieldEvaluator::ConstPtr fieldEvaluator() const;
    ActionGenerator::ConstPtr actionGenerator() const;

    void update( const rcsc::WorldModel & wm );

    const ActionChainGraph & graph() const;
};

#endif
