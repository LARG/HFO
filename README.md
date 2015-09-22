RoboCup 2D Half Field Offense
===============

![3 on 3 HFO](https://github.com/mhauskn/HFO/blob/master/img/hfo3on3.png)

[Half Field Offense in RoboCup 2D Soccer](http://www.cs.utexas.edu/~AustinVilla/sim/halffieldoffense/) is a subtask in RoboCup simulated soccer, modeling a situation in which the offense of one team has to get past the defense of the opposition in order to shoot goals. This repository offers the ability to quickly and easily interface your learning agent with the HFO domain.

## Dependencies
 - Boost-system, filesystem
 - Flex
 - Qt4 [Optional]: Required for SoccerWindow2 visualizer. To skip this step, add cmake flag `-DBUILD_SOCCERWINDOW=False`

## Install
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
make install
```
[Optional Python Install]: `pip install [--user] .`



## Example Agents
Example C++ and Python agents are provided in the `example` directory.

## Demo
Start a simple 1v1 game played by Agent2D:
```
./bin/start.py --offense-npcs=1 --defense-npcs=1
```

Start an example agent on the empty goal task. This agent will move
forwards slowly. First start the server: `./bin/start.py
--offense-agents=1 &` and then connect the agent:
`./example/hfo_example_agent`.

Or do both in a single command:
```
(./bin/start.py --offense-agents=1 &) && ./example/hfo_example_agent
```

## Documentation
The state and action spaces provided by the HFO domain are documented in the [manual](doc/manual.pdf).