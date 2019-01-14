#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>
#include <tensorflow/core/protobuf/meta_graph.pb.h>
#include <tensorflow/core/public/session.h>
#include <tensorflow/core/public/session_options.h>

//J530113064
using namespace std;
using namespace hfo;

float pitchHalfLength = 52.5;
float pitchHalfWidth = 34;
float tolerance_x = pitchHalfLength * 0.1;
float tolerance_y = pitchHalfWidth * 0.1;
float FEAT_MIN = -1;
float FEAT_MAX = 1;
float pi = 3.14159265358979323846;
float max_R = sqrtf(pitchHalfLength * pitchHalfLength + pitchHalfWidth * pitchHalfWidth);
float stamina_max = 8000;

typedef std::vector<std::pair<std::string, tensorflow::Tensor>> tensor_dict;
typedef std::vector<float> State;
/**
 * @brief load a previous store model
 * @details [long description]
 *
 * in Python run:
 *
 *    saver = tf.train.Saver(tf.global_variables())
 *    saver.save(sess, './exported/my_model')
 *    tf.train.write_graph(sess.graph, '.', './exported/graph.pb, as_text=False)
 *
 * this relies on a graph which has an operation called `init` responsible to
 * initialize all variables, eg.
 *
 *    sess.run(tf.global_variables_initializer())  # somewhere in the python
 * file
 *
 * @param sess active tensorflow session
 * @param graph_fn path to graph file (eg. "./exported/graph.pb")
 * @param checkpoint_fn path to checkpoint file (eg. "./exported/my_model",
 * optional)
 * @return status of reloading
 */
tensorflow::Status LoadModel(tensorflow::Session *sess, std::string graph_fn,
                             std::string checkpoint_fn = "")
{
  tensorflow::Status status;

  // Read in the protobuf graph we exported
  tensorflow::MetaGraphDef graph_def;
  status = ReadBinaryProto(tensorflow::Env::Default(), graph_fn, &graph_def);
  if (status != tensorflow::Status::OK())
    return status;

  // create the graph in the current session
  status = sess->Create(graph_def.graph_def());
  if (status != tensorflow::Status::OK())
    return status;

  // restore model from checkpoint, iff checkpoint is given
  if (checkpoint_fn != "")
  {
    const std::string restore_op_name = graph_def.saver_def().restore_op_name();
    const std::string filename_tensor_name =
        graph_def.saver_def().filename_tensor_name();

    tensorflow::Tensor filename_tensor(tensorflow::DT_STRING,
                                       tensorflow::TensorShape());
    filename_tensor.scalar<std::string>()() = checkpoint_fn;

    tensor_dict feed_dict = {{filename_tensor_name, filename_tensor}};
    status = sess->Run(feed_dict, {}, {restore_op_name}, nullptr);
    if (status != tensorflow::Status::OK())
      return status;
  }
  else
  {
    // virtual Status Run(const std::vector<std::pair<string, Tensor> >& inputs,
    //                  const std::vector<string>& output_tensor_names,
    //                  const std::vector<string>& target_node_names,
    //                  std::vector<Tensor>* outputs) = 0;
    status = sess->Run({}, {}, {"init"}, nullptr);
    if (status != tensorflow::Status::OK())
      return status;
  }

  return tensorflow::Status::OK();
}

std::vector<State> stack_state(std::vector<State> &stacked_state, State &state, bool is_new_episode)
{
  if (is_new_episode)
  {
    stacked_state = std::vector<State>();
    stacked_state.push_back(state);
    stacked_state.push_back(state);
    stacked_state.push_back(state);
    stacked_state.push_back(state);
  }
  else
  {
    stacked_state.erase(stacked_state.begin());
    stacked_state.push_back(state);
  }
  return stacked_state;
}

float unnormalize(float val, float min_val, float max_val)
{
  if (val < FEAT_MIN || val > FEAT_MAX)
  {
    std::cout << "Unnormalized value Violated Feature Bounds: " << val
              << " Expected min/max: [" << FEAT_MIN << ", "
              << FEAT_MAX << "]" << std::endl;
    float ft_max = FEAT_MAX; // Linker error on OSX otherwise...?
    float ft_min = FEAT_MIN;
    val = std::min(std::max(val, ft_min), ft_max);
  }
  return ((val - FEAT_MIN) / (FEAT_MAX - FEAT_MIN)) * (max_val - min_val) + min_val;
}

float abs_x(float normalized_x_pos, bool playingOffense)
{
  float tolerance_x = .1 * pitchHalfLength;
  if (playingOffense)
  {
    return unnormalize(normalized_x_pos, -tolerance_x, pitchHalfLength + tolerance_x);
  }
  else
  {
    return unnormalize(normalized_x_pos, -pitchHalfLength - tolerance_x, tolerance_x);
  }
}

float abs_y(float normalized_y_pos)
{
  float tolerance_y = .1 * pitchHalfWidth;
  return unnormalize(normalized_y_pos, -pitchHalfWidth - tolerance_y,
                     pitchHalfWidth + tolerance_y);
}

void remake_state(std::vector<float> &state, int num_mates, int num_ops, bool is_offensive)
{
  state[0] = abs_x(state[0], is_offensive);
  state[1] = abs_y(state[1]);
  state[2] = unnormalize(state[2], -pi, pi);
  state[3] = abs_x(state[3], is_offensive);
  state[4] = abs_y(state[4]);
  state[5] = unnormalize(state[5], 0, 1);
  state[6] = unnormalize(state[6], 0, max_R);
  state[7] = unnormalize(state[7], -pi, pi);
  state[8] = unnormalize(state[8], 0, pi);
  if (num_ops > 0)
    state[9] = unnormalize(state[9], 0, max_R);
  else
    state[9] = -1000;
  for (int i = 10; i < 10 + num_mates; i++)
  {
    if (state[i] != -2)
      state[i] = unnormalize(state[i], 0, pi);
    else
      state[i] = -1000;
  }
  for (int i = 10 + num_mates; i < 10 + 2 * num_mates; i++)
  {
    if (state[i] != -2)
      state[i] = unnormalize(state[i], 0, max_R);
    else
      state[i] = -1000;
  }
  for (int i = 10 + 2 * num_mates; i < 10 + 3 * num_mates; i++)
  {
    if (state[i] != -2)
      state[i] = unnormalize(state[i], 0, pi);
    else
      state[i] = -1000;
  }
  int index = 10 + 3 * num_mates;
  for (int i = 0; i < num_mates; i++)
  {
    if (state[index] != -2)
      state[index] = abs_x(state[index], is_offensive);
    else
      state[index] = -1000;
    index += 1;
    if (state[index] != -2)
      state[index] = abs_y(state[index]);
    else
      state[index] = -1000;
    index += 2;
  }
  index = 10 + 6 * num_mates;
  for (int i = 0; i < num_ops; i++)
  {
    if (state[index] != -2)
      state[index] = abs_x(state[index], is_offensive);
    else
      state[index] = -1000;
    index += 1;
    if (state[index] != -2)
      state[index] = abs_y(state[index]);
    else
      state[index] = -1000;
    index += 2;
  }
  state[state.size() - 1] = unnormalize(state[state.size() - 1], 0, stamina_max);
}

// Before running this program, first Start HFO server:
// $./bin/HFO --offense-agents 1

// Server Connection Options. See printouts from bin/HFO.
feature_set_t features = HIGH_LEVEL_FEATURE_SET;
string config_dir = "example/formations-dt";
int port = 6000;
string server_addr = "localhost";
string team_name = "base_right";
bool goalie = false;

// We omit PASS & CATCH & MOVE actions here
action_t HIGH_LEVEL_ACTIONS[2] = {MOVE, GO_TO_BALL};

int main(int argc, char **argv)
{
  // Create the HFO environment
  HFOEnvironment hfo;
  // Connect to the server and request high-level feature set. See
  // manual for more information on feature sets.
  hfo.connectToServer(features, config_dir, port, server_addr,
                      team_name, goalie);

  std::vector<hfo::action_t> actions = {MOVE, GO_TO_BALL};

  const std::string graph_fn = "./exported/my_model.meta";
  const std::string checkpoint_fn = "./exported/my_model";

  // prepare session
  tensorflow::Session *sess;
  tensorflow::SessionOptions options;
  TF_CHECK_OK(tensorflow::NewSession(options, &sess));
  TF_CHECK_OK(LoadModel(sess, graph_fn, checkpoint_fn));

  status_t status = IN_GAME;
  for (int episode = 0; status != SERVER_DOWN; episode++)
  {
    status = IN_GAME;
    while (status == IN_GAME)
    {
      auto state = hfo.getState();
      tensorflow::TensorShape data_shape({1, state.size(), 4});
      tensorflow::Tensor data(tensorflow::DT_FLOAT, data_shape);  
      remake_state(state, hfo.getNumTeammates(), hfo.getNumOpponents(), false);
      std::vector<State> stacked_state;
      stack_state(stacked_state, state, true);
      auto input_map = data.tensor<float, 3>();
      for (int b = 0; b < 4; b++)
      {
        for (int c = 0; c < state.size(); c++)
        {
          input_map(0, b, c) = stacked_state[b][c];
        }
      }

      tensor_dict feed_dict = {
          {"DQNetwork/inputs", data},
      };
      std::vector<tensorflow::Tensor> outputs;
      TF_CHECK_OK(sess->Run(feed_dict, {"DQNetwork/add"}, {}, &outputs));  
      auto output_map = outputs[0].tensor<float,2>();
      int action = 0;
      if (output_map(0,0) < output_map(0,1)) action = 1;
      hfo.act(actions[action]);

      // Advance the environment and get the game status
      status = hfo.step();
    }
    // Check what the outcome of the episode was
    cout << "Episode " << episode << " ended with status: "
         << StatusToString(status) << endl;
  }
  hfo.act(QUIT);
};
