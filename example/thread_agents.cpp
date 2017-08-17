#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>

using namespace std;
using namespace hfo;

// This example controls two agents using a single process.
// To run this example you don't need to start the HFO server first. Simply run:
// $> ./example/threaded_agent

int offense_agents = 2;
std::vector<std::thread> threads_;
std::vector<std::thread> player_threads;
std::vector<hfo::HFOEnvironment> envs_;
bool FINISHED = false;
std::mutex mutexs_[2]; // one for each agent
std::condition_variable cvs_[2]; // one for each agent
std::vector<int> need_to_step_;
std::vector<hfo::status_t> status_;
int server_port_ = 6000;
bool episode_over_ = false;

void ExecuteCommand(string cmd) {
  int ret = system(cmd.c_str());
  assert(ret == 0);
}

void startServer(int offense_agents, int offense_npcs,
                 int defense_agents, int defense_npcs, bool fullstate,
                 bool gui, bool log_game, bool verbose) {
  std::string cmd = "./bin/HFO";
  cmd += " --port " + std::to_string(server_port_)
      + " --offense-agents " + std::to_string(offense_agents)
      + " --offense-npcs " + std::to_string(offense_npcs)
      + " --defense-agents " + std::to_string(defense_agents)
      + " --defense-npcs " + std::to_string(defense_npcs);
  if (fullstate) { cmd += " --fullstate"; }
  if (!gui)      { cmd += " --headless"; }
  if (!log_game) { cmd += " --no-logging"; }
  if (verbose)   { cmd += " --verbose"; }
  std::cout << "Starting server with command: " << cmd << std::endl;
  threads_.emplace_back(std::thread(ExecuteCommand, cmd));
  sleep(10);
}

void EnvConnect(HFOEnvironment* env, int server_port, string team_name,
                bool play_goalie, status_t* status_ptr,
                mutex* mtx, condition_variable* cv, int* need_to_step) {
  string config_dir = "bin/teams/base/config/formations-dt";
  env->connectToServer(LOW_LEVEL_FEATURE_SET, config_dir, server_port,
                       "localhost", team_name, play_goalie);
  while (!FINISHED) {
    std::unique_lock<std::mutex> lck(*mtx);
    while (!(*need_to_step)) {
      cv->wait(lck);
    }
    *status_ptr = env->step();
    *need_to_step = false;
  }
}

void act(int tid, action_t action, float arg1, float arg2) {
  assert(envs_.size() >= tid);
  envs_[tid].act(action, arg1, arg2);
}

void stepThread(int tid) {
  mutexs_[tid].lock();
  need_to_step_[tid] = true;
  cvs_[tid].notify_one();
  mutexs_[tid].unlock();
  while (need_to_step_[tid]) {
    std::this_thread::yield();
  }
}

status_t stepUntilEpisodeEnd(int tid) {
  HFOEnvironment& env = envs_[tid];
  while (status_[tid] == IN_GAME) {
    env.act(NOOP);
    stepThread(tid);
    if (status_[tid] == SERVER_DOWN) {
      cerr << "Server Down! Exiting." << endl;
      exit(1);
    }
  }
  return status_[tid];
}

status_t step(int tid) {
  status_t old_status = status_[tid];
  if (episode_over_ && old_status == IN_GAME) {
    stepUntilEpisodeEnd(tid);
    old_status = status_[tid];
  }
  stepThread(tid);
  status_t current_status = status_[tid];
  if (current_status == SERVER_DOWN) {
    cerr << "Server Down! Exiting." << endl;
    exit(1);
  }
  if (episode_over_ && old_status != IN_GAME && current_status == IN_GAME) {
    episode_over_ = false;
  }
  if (current_status != IN_GAME) {
    episode_over_ = true;
  }
  return current_status;
}

void cleanup() {
  FINISHED = true;
  for (int i=0; i<envs_.size(); ++i) {
    envs_[i].act(QUIT);
    stepThread(i);
  }
  for (std::thread& t : threads_) {
    t.join();
  }
}

void PlayEpisode(int tid) {
  while (status_[tid] == IN_GAME) {
    const vector<float>& feature_vec = envs_[tid].getState();
    act(tid, DASH, 100., 0);
    status_[tid] = step(tid);
  }
}

void PlayEpisodes(int tid, int num_episodes) {
  for (int i=0; i<num_episodes; ++i) {
    PlayEpisode(tid);
    // Make sure status goes back to IN_GAME before next episode start
    while (status_[tid] != IN_GAME) {
      act(tid, NOOP, 0., 0.);
      status_[tid] = step(tid);
    }
  }
}

int main(int argc, char**argv) {
  int num_episodes = 50;
  envs_.resize(offense_agents);
  need_to_step_.resize(offense_agents, false);
  status_.resize(offense_agents, IN_GAME);

  startServer(offense_agents, 0, 0, 0, true, false, false, false);

  for (int tid = 0; tid < offense_agents; tid++) {
    threads_.emplace_back(EnvConnect, &envs_[tid], server_port_, "base_left",
                          false, &status_[tid],
                          &mutexs_[tid], &cvs_[tid], &need_to_step_[tid]);
    sleep(5);
  }

  for (int i=0; i<offense_agents; ++i) {
    player_threads.emplace_back(PlayEpisodes, i, num_episodes);
    sleep(5);
  }

  for (std::thread& t : player_threads) {
    t.join();
  }

  cleanup();
};
