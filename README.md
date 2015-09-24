RoboCup 2D Half Field Offense
===============

![3 on 3 HFO](https://github.com/mhauskn/HFO/blob/master/img/hfo3on3.png)

[Half Field Offense in RoboCup 2D Soccer](http://www.cs.utexas.edu/~AustinVilla/sim/halffieldoffense/) is a subtask in RoboCup simulated soccer, modeling a situation in which the offense of one team has to get past the defense of the opposition in order to shoot goals. This repository offers the ability to quickly and easily interface your learning agent with the HFO domain. Interfaces are provided for C++ and Python.

## Dependencies
 - Boost-system, filesystem
 - Flex
 - Qt4 [Required for SoccerWindow2 visualizer]: To not build the visualizer, add cmake flag `-DBUILD_SOCCERWINDOW=False`

## Install
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
make install
```
#### Python Install (required only for python interface)
From the main HFO directory: `pip install [--user] .`

## Demos
Start a simple 1v1 game played by Agent2D:
```
./bin/HFO --offense-npcs=1 --defense-npcs=1 --no-sync
```

Start an example agent on the empty goal task. This agent will move
forwards slowly. First start the server: `./bin/HFO --offense-agents=1
--no-sync &` and then connect the agent: `./example/hfo_example_agent`

Or do both in a single command:
```
(./bin/HFO --offense-agents=1 --no-sync &) && ./example/hfo_example_agent
```

## Example Agents
Example C++ and Python agents are provided in the `example` directory.

#### Example C++ Agent
```c++
  HFOEnvironment hfo;
  // Connect to the agent's server on port 6000 and request low-level
  // feature set. See manual for more information on feature sets.
  hfo.connectToAgentServer(6000, LOW_LEVEL_FEATURE_SET);
  // Play 5 episodes
  for (int episode=0; episode<5; episode++) {
    status_t status = IN_GAME;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const std::vector<float>& feature_vec = hfo.getState();
      // Create a dash action
      Action a = {DASH, 20.0, 0.0};
      // Perform the dash and recieve the current game status
      status = hfo.act(a);
    }
    // Check what the outcome of the episode was
    cout << "Episode " << episode << " ended with status: ";
```

## Documentation
The state and action spaces provided by the HFO domain are documented in the [manual](doc/manual.pdf).