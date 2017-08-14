// -*-c++-*-

#ifndef LOWLEVEL_FEATURE_EXTRACTOR_H
#define LOWLEVEL_FEATURE_EXTRACTOR_H

#include <rcsc/player/player_agent.h>
#include "feature_extractor.h"
#include <vector>

class LowLevelFeatureExtractor : public FeatureExtractor {
public:
  LowLevelFeatureExtractor(int num_teammates, int num_opponents,
                           bool playing_offense);
  virtual ~LowLevelFeatureExtractor();

  // Updated the state features stored in feature_vec
  virtual const std::vector<float>& ExtractFeatures(const rcsc::WorldModel& wm,
						    bool last_action_status);

protected:
  // Number of features for non-player objects.
  const static int num_basic_features = 58;
  // Number of features for each player or opponent in game, not including uniform numbers.
  const static int features_per_player = 8;
};

#endif // LOWLEVEL_FEATURE_EXTRACTOR_H
