// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

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

#ifndef SAMPLE_PLAYER_H
#define SAMPLE_PLAYER_H

#include "HFO.hpp"
#include "action_generator.h"
#include "field_evaluator.h"
#include "communication.h"
#include "feature_extractor.h"

#include <rcsc/player/player_agent.h>
#include <vector>

class SamplePlayer
    : public rcsc::PlayerAgent {
private:

    Communication::Ptr M_communication;

    FieldEvaluator::ConstPtr M_field_evaluator;
    ActionGenerator::ConstPtr M_action_generator;

public:

    SamplePlayer();

    virtual
    ~SamplePlayer();

protected:

    /*!
      You can override this method.
      But you must call PlayerAgent::initImpl() in this method.
    */
    virtual
    bool initImpl( rcsc::CmdLineParser & cmd_parser );

    //! main decision
    virtual
    void actionImpl();

    //! communication decision
    virtual
    void communicationImpl();

    virtual
    void handleActionStart();
    virtual
    void handleActionEnd();

    virtual
    void handleServerParam();
    virtual
    void handlePlayerParam();
    virtual
    void handlePlayerType();

    virtual
    FieldEvaluator::ConstPtr createFieldEvaluator() const;

    virtual
    ActionGenerator::ConstPtr createActionGenerator() const;

private:

    bool doPreprocess();
    bool doShoot();
    bool doForceKick();
    bool doHeardPassReceive();

public:
    virtual
    FieldEvaluator::ConstPtr getFieldEvaluator() const;

protected:
  FeatureExtractor* feature_extractor;
  long lastTrainerMessageTime;
  int num_teammates, num_opponents;
  bool playing_offense;
};

#endif
