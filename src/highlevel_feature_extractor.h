#ifndef HIGHLEVEL_FEATURE_EXTRACTOR_H
#define HIGHLEVEL_FEATURE_EXTRACTOR_H

#include <rcsc/player/player_agent.h>
#include "feature_extractor.h"
#include <vector>

class HighLevelFeatureExtractor : public FeatureExtractor {
public:
  HighLevelFeatureExtractor(int num_teammates, int num_opponents,
                            bool playing_offense);
  virtual ~HighLevelFeatureExtractor();

  // Updated the state features stored in feature_vec
  virtual const std::vector<float>& ExtractFeatures(const rcsc::WorldModel& wm);

protected:
  // Number of features for non-player objects.
  const static int num_basic_features = 58;
  // Number of features for each player or opponent in game.
  const static int features_per_player = 8;
  int numTeammates; // Number of teammates in HFO
  int numOpponents; // Number of opponents in HFO
  bool playingOffense; // Are we playing offense or defense?
};

#endif // HIGHLEVEL_FEATURE_EXTRACTOR_H
