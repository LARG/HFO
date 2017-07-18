#include <vector>
#include <HFO.hpp>
#include <cstdlib>
#include <math.h> 
#include <fstream>

using namespace std;
using namespace hfo;

/* Before running this program, first Start HFO server:
 ../bin/HFO --offense-npcs 2 --defense-agents 1 --defense-npcs 1 
This is a hand coded defense agent, which can play a 2v2 game againt 2 offense npcs when paired up with a goal keeper
Server Connection Options. See printouts from bin/HFO.*/

feature_set_t features = HIGH_LEVEL_FEATURE_SET;
string config_dir = "../bin/teams/base/config/formations-dt";
int port = 7000;
string server_addr = "localhost";
string team_name = "base_right";
bool goalie = false;

double kickable_dist = 1.504052352;
double open_area_up_limit_x = 0.747311440447;
double open_area_up_limit_y = 0.229619544504;
double open_area_low_limit_x = -0.352161264597;
double open_area_low_limit_y = 0.140736680776;
double tackle_limit = 1.613456553;
 

double HALF_FIELD_WIDTH = 68 ; // y coordinate -34 to 34 (-34 = bottom 34 = top)
double HALF_FIELD_LENGTH = 52.5; // x coordinate 0 to 52.5 (0 = goalline 52.5 = center)

struct action_with_params {
    action_t action;
    double param;
};

// Returns a random high-level action
action_t get_random_high_lv_action() {
  action_t action_indx = (action_t) ((rand() % 4) + REDUCE_ANGLE_TO_GOAL);
  return action_indx;
}

double get_actual_angle( double normalized_angle) {
        return normalized_angle * M_PI;
}
	


double get_dist_normalized (double ref_x, double ref_y, double src_x, double src_y) {
        return sqrt(pow(ref_x - src_x,2) + pow((HALF_FIELD_WIDTH/HALF_FIELD_LENGTH)*(ref_y - src_y),2));              
}




bool is_kickable(double ball_pos_x, double ball_pos_y, double opp_pos_x, double opp_pos_y) {
        return get_dist_normalized(ball_pos_x, ball_pos_y, opp_pos_x, opp_pos_y) < kickable_dist; //#param
}

bool is_in_open_area(double pos_x, double pos_y) {
        if (pos_x < open_area_up_limit_x ) {//&& pos_x > open_area_low_limit_x && pos_y < open_area_up_limit_y && pos_y > open_area_low_limit_y ) { //#param
                return false;
        } else {
                return true;
        }
}

action_with_params get_defense_action(const std::vector<float>& state_vec, double no_of_opponents, int numTMates) {
        int size_of_vec = 10 + 6*numTMates + 3*no_of_opponents;
        if (size_of_vec != state_vec.size()) {
                std :: cout <<"Invalid Feature Vector / Check the number of teammates/opponents provided";
                return {NOOP,0};
        }
        
        double agent_posx = state_vec[0];
        double agent_posy = state_vec[1];
        double agent_orientation = get_actual_angle(state_vec[2]);
        
        double opp1_unum = state_vec[9+6*numTMates+3];
        double opp2_unum = state_vec[9+(1*3)+6*numTMates+3];
        
        double ball_pos_x = state_vec[3];
        double ball_pos_y = state_vec[4];
        double opp1_pos_x = state_vec[9+6*numTMates+1];
        double opp1_pos_y = state_vec[9+6*numTMates+2];
        double opp2_pos_x = state_vec[9+(1*3)+6*numTMates+3];
        double opp2_pos_y = state_vec[9+(1*3)+6*numTMates+3];


        double opp1_dist_to_ball = get_dist_normalized(ball_pos_x, ball_pos_y, opp1_pos_x, opp1_pos_y);
        double opp1_dist_to_agent = get_dist_normalized(agent_posx, agent_posy, opp1_pos_x, opp1_pos_y);
        bool is_kickable_opp1 = is_kickable(ball_pos_x, ball_pos_y,opp1_pos_x, opp1_pos_y);
        bool is_in_open_area_opp1 = is_in_open_area(opp1_pos_x, opp1_pos_y);
    
        double opp2_dist_to_ball = get_dist_normalized(ball_pos_x, ball_pos_y, opp2_pos_x, opp2_pos_y);
        double opp2_dist_to_agent = get_dist_normalized(agent_posx, agent_posy, opp2_pos_x, opp2_pos_y);
        bool is_kickable_opp2 = is_kickable(ball_pos_x, ball_pos_y,opp2_pos_x, opp2_pos_y);
        bool is_in_open_area_opp2 = is_in_open_area(opp2_pos_x, opp2_pos_y);
    
        double tackle_limit_nn = tackle_limit;
        
        if (is_in_open_area(opp1_pos_x, opp1_pos_y) && is_in_open_area(opp2_pos_x, opp2_pos_y)) {
                //std:: cout << "In open Area" << "\n";
                if (is_kickable(ball_pos_x, ball_pos_y, opp1_pos_x, opp1_pos_y) && 
                        get_dist_normalized(ball_pos_x, ball_pos_y, opp1_pos_x, opp1_pos_y) < 
                        get_dist_normalized(ball_pos_x, ball_pos_y, opp2_pos_x, opp2_pos_y)) {
                        return {MARK_PLAYER, opp2_unum};
                        //                      return {REDUCE_ANGLE_TO_GOAL, 1};

                } else if (is_kickable(ball_pos_x, ball_pos_y, opp2_pos_x, opp2_pos_y)) {
                        return {MARK_PLAYER, opp1_unum};
                        //                      return {REDUCE_ANGLE_TO_GOAL, 1};

                } else if (get_dist_normalized(ball_pos_x, ball_pos_y, opp1_pos_x, opp1_pos_y) > 
                                   get_dist_normalized(ball_pos_x, ball_pos_y, agent_posx, agent_posy) && 
                               get_dist_normalized(ball_pos_x, ball_pos_y, opp2_pos_x, opp2_pos_y) > 
                               get_dist_normalized(ball_pos_x, ball_pos_y, agent_posx, agent_posy)) {
                        return {GO_TO_BALL,1};
                } else {
                        return {REDUCE_ANGLE_TO_GOAL, 1};
                }
        } else {
                //std:: cout << "In Penalty Area" << "\n";
                if (! is_kickable(ball_pos_x,ball_pos_y,opp1_pos_x, opp1_pos_y) && ! is_kickable(ball_pos_x,ball_pos_y,opp2_pos_x,opp2_pos_y)) 
                {
                        //std :: cout <<"IN AREA BUT GOTO\n";
                         if (get_dist_normalized(ball_pos_x, ball_pos_y, opp1_pos_x, opp1_pos_y) > get_dist_normalized(ball_pos_x, ball_pos_y, agent_posx, agent_posy) && 
                            get_dist_normalized(ball_pos_x, ball_pos_y, opp1_pos_x, opp1_pos_y) > get_dist_normalized(ball_pos_x, ball_pos_y, agent_posx, agent_posy)) {
                                        return {GO_TO_BALL,2};
                        } else {
                                return {REDUCE_ANGLE_TO_GOAL, 0};
                        }

                } else if ( get_dist_normalized (agent_posx, agent_posy, opp1_pos_x, opp1_pos_y) < tackle_limit
                           || get_dist_normalized (agent_posx, agent_posy, opp2_pos_x, opp2_pos_y) < tackle_limit ) { //#param
                        /*double turn_angle;
                        //REVISIT TURN ANGLE.. CONSIDER ACTUAL DIRECTION ALSO..
                        if (get_dist_normalized(ball_pos_x, ball_pos_y, opp1_pos_x, opp1_pos_y) < get_dist_normalized(ball_pos_x, ball_pos_y, opp1_pos_x, opp1_pos_y)) {
                                turn_angle = atan2((HALF_FIELD_WIDTH/HALF_FIELD_LENGTH)*(opp1_pos_y), opp1_pos_x) - (agent_orientation*M_PI);
                        } else {
                                turn_angle = atan2((HALF_FIELD_WIDTH/HALF_FIELD_LENGTH)*(opp2_pos_y), opp2_pos_x) - (agent_orientation*M_PI);
                        }
                        turn_angle = atan2(tan(turn_angle),1);
                        if (turn_angle > M_PI ) {
                                turn_angle -= 2*M_PI;
                        } else if (turn_angle < -M_PI) {
                                turn_angle += 2*M_PI;
                        }*/
                        /*string s = "TACKLE " + std ::to_string (turn_angle) + "\n";
                        cout << s; */
                        //return {DASH, turn_angle*180/M_PI}; 
                        //return {TACKLE, turn_angle*180/M_PI}; //TACKLE needs power od dir
                        return {MOVE,0};
                } else if ((!is_in_open_area(opp1_pos_x, opp1_pos_y) && is_in_open_area(opp2_pos_x, opp2_pos_y)) || 
                                  (!is_in_open_area(opp2_pos_x, opp2_pos_y) && is_in_open_area(opp1_pos_x, opp1_pos_y))) {
                                return {REDUCE_ANGLE_TO_GOAL,0};
                } else if (!is_in_open_area(opp1_pos_x, opp1_pos_y) && !is_in_open_area(opp2_pos_x, opp2_pos_y)) {
                        if (get_dist_normalized(ball_pos_x, ball_pos_y, opp1_pos_x, opp1_pos_y) < get_dist_normalized(ball_pos_x, ball_pos_y, opp1_pos_x, opp1_pos_y)) {
                                return {MARK_PLAYER,opp2_unum};
                        } else {
                                return {MARK_PLAYER,opp1_unum};
                        }
                } else {
                        std :: cout <<"Unknown Condition";
                        return {NOOP,0};
                }
        }
        
}
void read_params() {
    std::ifstream fin("params.txt");
    double d[6];
    int i = 0;
    while (true) {
        fin >> d[i];
        if( fin.eof() ) break;
        std::cout << d[i] << std::endl;
        i++;
                //if (i >=6 ) {
                //      std::cout << "invalid params" << d[5];
                //      exit(0);
                //}
    }
    fin.close();
    kickable_dist = (d[0]+1)* 0.818175061;
    open_area_up_limit_x = d[1];
    open_area_up_limit_y = d[2];
    open_area_low_limit_x = d[3];
    open_area_low_limit_y = d[4];
    tackle_limit = (d[5]+1)* 0.818175061;
    std :: cout << "kickable dist " << kickable_dist << "tackle_limit" <<tackle_limit;
    return;
}

void write_cost(double cost) {
    std::ofstream myfile ("cost.txt");
    if (myfile.is_open()) {
        myfile  << cost;
        myfile.close();
    }
    return;
}

int main(int argc, char** argv) {
  // Create the HFO environment
  //read_params();
  HFOEnvironment hfo;
  int random = 0;
  double numGoals = 0;
  int numEpisodes = 5000;
  double actualNumEpisodes = 0;
  // Connect to the server and request high-level feature set. See
  // manual for more information on feature sets.
  hfo.connectToServer(features, config_dir, port, server_addr,
                           team_name, goalie);
  for (int episode=0; episode<numEpisodes; episode++) {
    status_t status = IN_GAME;
    while (status == IN_GAME) {

    // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      if (random == 0) {
              action_with_params a = get_defense_action(feature_vec, 2, hfo.getNumTeammates());
         // std::cout << a.action << a.param;
         if (a.action == hfo :: MARK_PLAYER || a.action == hfo::TACKLE) {
                  hfo.act(a.action, a.param);
              } else if (a.action == hfo :: DASH) {
                          double power = 100;
                          hfo.act(a.action, power, a.param);
                  } else {
                      hfo.act(a.action);
              }
              string s = hfo::ActionToString(a.action) + " " +to_string(a.param) + "\n";
             // std::cout << s;
      } else {
	std::cout <<"Random";
	action_t a = get_random_high_lv_action();
	if (a == hfo :: MARK_PLAYER) {
	  hfo.act(NOOP); // why not MOVE?
	} else {
	  hfo.act(a);
	}
      }
      //hfo.act(hfo::INTERCEPT);
      status = hfo.step();
    }
    if (status==GOAL)
            numGoals = numGoals+1;
    // Check what the outcome of the episode was
    cout << "Episode " << episode << " ended with status: "
         << StatusToString(status) << std::endl;
    
    if (status==SERVER_DOWN) {
      break;
    } else {
      actualNumEpisodes++;
    }
  }
  double cost = numGoals/actualNumEpisodes;
  hfo.act(QUIT);
  //write_cost(cost);
};

