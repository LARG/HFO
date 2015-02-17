// -*-c++-*-

/*!
  \file action_chain_graph.h
  \brief cooperative action sequence searcher
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

#ifndef RCSC_ACTION_CHAIN_GRAPH_H
#define RCSC_ACTION_CHAIN_GRAPH_H

#include "action_generator.h"
#include "field_evaluator.h"

#include <rcsc/geom/vector_2d.h>

#include <boost/shared_ptr.hpp>

#include <vector>
#include <iostream>
#include <utility>

namespace rcsc {
class PlayerAgent;
class WorldModel;
}

class CooperativeAction;
class PredictState;

class ActionChainGraph {
public:

    typedef boost::shared_ptr< ActionChainGraph > Ptr; //!< pointer type alias
    typedef boost::shared_ptr< const ActionChainGraph > ConstPtr; //!< const pointer type alias

public:
    static const size_t DEFAULT_MAX_CHAIN_LENGTH;
    static const size_t DEFAULT_MAX_EVALUATE_LIMIT;

private:
    FieldEvaluator::ConstPtr M_evaluator;
    ActionGenerator::ConstPtr M_action_generator;

    int M_chain_count;
    int M_best_chain_count;

    unsigned long M_max_chain_length;
    long M_max_evaluate_limit;

    static std::vector< std::pair< rcsc::Vector2D, double > > S_evaluated_points;

private:
    std::vector< ActionStatePair > M_result;
    double M_best_evaluation;

    void calculateResult( const rcsc::WorldModel & wm );

    void calculateResultChain( const rcsc::WorldModel & wm,
                               unsigned long * n_evaluated );
    bool doSearch( const rcsc::WorldModel & wm,
                   const PredictState & state,
                   const std::vector< ActionStatePair > & path,
                   std::vector< ActionStatePair > * result,
                   double * result_evaluation,
                   unsigned long * n_evaluated,
                   unsigned long max_chain_length,
                   long max_evaluate_limit );

    void calculateResultBestFirstSearch( const rcsc::WorldModel & wm,
                                         unsigned long * n_evaluated );

    void debugPrintCurrentState( const rcsc::WorldModel & wm );


public:
    /*!
      \brief constructor
      \param evaluator evaluator of each state
      \param generator action and state generator
      \param agent pointer to player agent
     */
    ActionChainGraph( const FieldEvaluator::ConstPtr & evaluator,
                      const ActionGenerator::ConstPtr & generator,
                      unsigned long max_chain_length = DEFAULT_MAX_CHAIN_LENGTH,
                      long max_evaluate_limit = DEFAULT_MAX_EVALUATE_LIMIT );

    void calculate( const rcsc::WorldModel & wm )
      {
          calculateResult( wm );
      }

    const std::vector< ActionStatePair > & getAllChain() const
      {
          return M_result;
      };

    const CooperativeAction & getFirstAction() const
      {
          return (*(M_result.begin())).action();
      };

    const PredictState & getFirstState() const
      {
          return (*(M_result.begin())).state();
      };

    const PredictState & getFinalResult() const
      {
          return (*(M_result.rbegin())).state();
      };

public:
    static
    void debug_send_chain( rcsc::PlayerAgent * agent,
                           const std::vector< ActionStatePair > & path );

    static
    void write_chain_log( const std::string & pre_log_message,
                          const rcsc::WorldModel & world,
                          const int count,
                          const std::vector< ActionStatePair > & path,
                          const double & eval );

    static
    void write_chain_log( const rcsc::WorldModel & world,
                          const int count,
                          const std::vector< ActionStatePair > & path,
                          const double & eval );
};

#endif
