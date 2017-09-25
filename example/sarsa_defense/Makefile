#Directories
FA_DIR = ../sarsa_libraries/funcapprox
POLICY_DIR = ../sarsa_libraries/policy
HFO_SRC_DIR = ../../src
HFO_LIB_DIR = ../../lib

#Includes
INCLUDES = -I$(FA_DIR) -I$(POLICY_DIR) -I$(HFO_SRC_DIR)

#Libs
FA_LIB = funcapprox
POLICY_LIB = policyagent

#Flags
CXXFLAGS = -g -Wall -std=c++11 -pthread
LDFLAGS = -l$(FA_LIB) -l$(POLICY_LIB) -lhfo -pthread
LDLIBS = -L$(FA_DIR) -L$(POLICY_DIR) -L$(HFO_LIB_DIR)
LINKEROPTIONS = -Wl,-rpath,$(HFO_LIB_DIR)

#Compiler
CXX = g++

#Sources
SRC = high_level_sarsa_defense_agent.cpp

#Objects
OBJ = $(SRC:.cpp=.o)

#Target
TARGET = high_level_sarsa_agent

#Rules 	

.PHONY: $(FA_LIB)

all: $(TARGET)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $(@F:%.o=%.cpp)

$(FA_LIB):
	$(MAKE) -C $(FA_DIR)

$(POLICY_LIB):
	$(MAKE) -C $(POLICY_DIR)

$(TARGET): $(FA_LIB) $(POLICY_LIB) $(OBJ)
	$(CXX) $(OBJ) $(CXXFLAGS) $(LDLIBS) $(LDFLAGS) -o $(TARGET) $(LINKEROPTIONS) 

cleanfa:
	$(MAKE) clean -C $(FA_DIR)

cleanpolicy:
	$(MAKE) clean -C $(POLICY_DIR)

clean: cleanfa cleanpolicy
	rm -f $(TARGET) $(OBJ) *~

