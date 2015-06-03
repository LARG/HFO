#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "highlevel_feature_extractor.h"
#include <rcsc/common/server_param.h>

using namespace rcsc;

HighLevelFeatureExtractor::HighLevelFeatureExtractor(int num_teammates,
                                                     int num_opponents,
                                                     bool playing_offense) :
    FeatureExtractor(),
    numTeammates(num_teammates),
    numOpponents(num_opponents),
    playingOffense(playing_offense)
{
  assert(numTeammates >= 0);
  assert(numOpponents >= 0);
  numFeatures = num_basic_features + features_per_teammate * numTeammates;
  if (numOpponents > 0) {
    // One extra basic feature and one feature per teammate
    numFeatures += 1 + numTeammates;
  }
  feature_vec.resize(numFeatures);
}

HighLevelFeatureExtractor::~HighLevelFeatureExtractor() {}

const std::vector<float>& HighLevelFeatureExtractor::ExtractFeatures(
    const WorldModel& wm) {
  featIndx = 0;
  const ServerParam& SP = ServerParam::i();
  const SelfObject& self = wm.self();
  const Vector2D& self_pos = self.pos();
  const float self_ang = self.body().radian();
  const PlayerCont& teammates = wm.teammates();
  float maxR = sqrtf(SP.pitchHalfLength() * SP.pitchHalfLength()
                     + SP.pitchHalfWidth() * SP.pitchHalfWidth());
  Vector2D goalCenter(SP.pitchHalfLength(), 0);
  // features about self pos
  float r;
  float th;
  angleDistToPoint(self_pos, goalCenter, th, r);
  addNormFeature(r, 0, maxR);
  addNormFeature(th, -2*M_PI, 2*M_PI);
  addNormFeature(self_ang, -2*M_PI, 2*M_PI);
  // features about our open angle to goal
  addNormFeature(calcLargestGoalAngle(wm, self_pos), 0, M_PI);
  //std::cout << "goal angle: " << RAD_T_DEG * features[ind-1] << std::endl;
  // teammate's open angle to goal
  int detected_teammates = 0;
  for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
    const PlayerObject& teammate = *it;
    if (valid(teammate) && detected_teammates < numTeammates) {
      addNormFeature(calcLargestGoalAngle(wm, teammate.pos()), 0, M_PI);
      detected_teammates++;
    }
  }
  // Add zero features for any missing teammates
  for (int i=detected_teammates; i<numTeammates; ++i) {
      addFeature(FEAT_INVALID);
  }

  // dist to our closest opp
  if (numOpponents > 0) {
    calcClosestOpp(wm, self_pos, th, r);
    addNormFeature(r, 0, maxR);
    //addNormFeature(th,-M_PI,M_PI);

    // teammates dists to closest opps
    detected_teammates = 0;
    for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
      const PlayerObject& teammate = *it;
      if (valid(teammate) && detected_teammates < numTeammates) {
        //addNormFeature(calcClosestOpp(wm,teammate.pos),0,maxR);
        calcClosestOpp(wm, teammate.pos(), th, r);
        addNormFeature(r, 0, maxR);
        //addNormFeature(th,-M_PI,M_PI);
        detected_teammates++;
      }
    }
    // Add zero features for any missing teammates
    for (int i=detected_teammates; i<numTeammates; ++i) {
      addFeature(FEAT_INVALID);
    }
  }

  // open angle to teammates
  detected_teammates = 0;
  for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
    const PlayerObject& teammate = *it;
    if (valid(teammate) && detected_teammates < numTeammates) {
      addNormFeature(calcLargestTeammateAngle(wm, self_pos, teammate.pos()),0,M_PI);
      detected_teammates++;
    }
  }
  // Add zero features for any missing teammates
  for (int i=detected_teammates; i<numTeammates; ++i) {
      addFeature(FEAT_INVALID);
  }

  // dist and angle to teammates
  detected_teammates = 0;
  for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
    const PlayerObject& teammate = *it;
    if (valid(teammate) && detected_teammates < numTeammates) {
      angleDistToPoint(self_pos, teammate.pos(), th, r);
      addNormFeature(r,0,maxR);
      addNormFeature(th,-M_PI,M_PI);
      detected_teammates++;
    }
  }
  // Add zero features for any missing teammates
  for (int i=detected_teammates; i<numTeammates; ++i) {
      addFeature(FEAT_INVALID);
      addFeature(FEAT_INVALID);
  }

  assert(featIndx == numFeatures);
  checkFeatures();
  //std::cout << "features: " << features.rows(0,ind-1).t();
  return feature_vec;
}
