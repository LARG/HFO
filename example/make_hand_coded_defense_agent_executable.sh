# Run this file to create an executable of hand_coded_defense_agent.cpp
g++ -c hand_coded_defense_agent.cpp -I ../src/ -std=c++0x -pthread
g++ -L ../lib/ hand_coded_defense_agent.o -lhfo -pthread -o hand_coded_defense_agent  -Wl,-rpath,../lib
