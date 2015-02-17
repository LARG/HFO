// -*-c++-*-

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

#ifndef ACTGEN_ACTION_CHAIN_LENGTH_FILTER_H
#define ACTGEN_ACTION_CHAIN_LENGTH_FILTER_H

#include "action_generator.h"

#include <boost/scoped_ptr.hpp>

#include <vector>

class ActGen_MaxActionChainLengthFilter
    : public ActionGenerator {
private:
    const ActionGenerator::ConstPtr M_generator;
    const unsigned int M_max_threshold_length;

public:
    ActGen_MaxActionChainLengthFilter( const ActionGenerator * generator,
                                       unsigned int max_threshold_length )
        : M_generator( generator )
        , M_max_threshold_length( max_threshold_length )
      { }

    void generate( std::vector< ActionStatePair > * result,
                   const PredictState & state,
                   const rcsc::WorldModel & current_wm,
                   const std::vector< ActionStatePair > & path ) const
      {
          if ( path.size() < M_max_threshold_length )
          {
              M_generator->generate( result, state, current_wm, path );
          }
      }
};

class ActGen_MinActionChainLengthFilter
    : public ActionGenerator {
private:
    const ActionGenerator::ConstPtr M_generator;
    const unsigned int M_min_threshold_length;

public:
    ActGen_MinActionChainLengthFilter( const ActionGenerator * generator,
                                       unsigned int min_threshold_length )
        : M_generator( generator )
        , M_min_threshold_length( min_threshold_length )
      { }

    void generate( std::vector< ActionStatePair > * result,
                   const PredictState & state,
                   const rcsc::WorldModel & current_wm,
                   const std::vector< ActionStatePair > & path ) const
      {
          if ( path.size() + 1 >= M_min_threshold_length )
          {
              M_generator->generate( result, state, current_wm, path );
          }
      }
};


class ActGen_RangeActionChainLengthFilter
    : public ActionGenerator {
public:
    static const int MIN = 1;
    static const int MAX = -1;

private:
    const ActionGenerator::ConstPtr M_generator;

    const int M_min_threshold_length;
    const int M_max_threshold_length;

public:
    ActGen_RangeActionChainLengthFilter( const ActionGenerator * generator,
                                         unsigned int min_threshold_length,
                                         unsigned int max_threshold_length )
        : M_generator( generator )
        , M_min_threshold_length( min_threshold_length )
        , M_max_threshold_length( max_threshold_length )
      { }

    void generate( std::vector< ActionStatePair > * result,
                   const PredictState & state,
                   const rcsc::WorldModel & current_wm,
                   const std::vector< ActionStatePair > & path ) const
      {
          if ( ( M_max_threshold_length == MAX
                 || static_cast< int >( path.size() ) < M_max_threshold_length )
               && ( static_cast< int >( path.size() + 1 ) >= M_min_threshold_length ) )
          {
              M_generator->generate( result, state, current_wm, path );
          }
      }
};

#endif
