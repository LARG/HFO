#ifndef HIGHLEVEL_FEATURE_EXTRACTOR_H
#define HIGHLEVEL_FEATURE_EXTRACTOR_H

#include <rcsc/player/player_agent.h>
#include "feature_extractor.h"
#include <vector>

/**
 * This feature extractor creates the high level feature set used by
 * Barrett et al.
 * (http://www.cs.utexas.edu/~sbarrett/publications/details-THESIS14-Barrett.html)
 * pages 159-160.
 */
class HighLevelFeatureExtractor : public FeatureExtractor {
public:
  HighLevelFeatureExtractor(int num_teammates, int num_opponents,
                            bool playing_offense);
  virtual ~HighLevelFeatureExtractor();

  // Updated the state features stored in feature_vec
  virtual const std::vector<float>& ExtractFeatures(const rcsc::WorldModel& wm);

protected:
  // Number of features for non-player objects.
  const static int num_basic_features = 10;
  // Number of features for each teammate and opponent in game.
  const static int features_per_teammate = 6;
  const static int features_per_opponent = 3;
};

#endif // HIGHLEVEL_FEATURE_EXTRACTOR_H
