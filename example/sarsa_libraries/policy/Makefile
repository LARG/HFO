#Directories
FA_DIR = ../funcapprox

#Includes
INCLUDES = -I$(FA_DIR)

#Flags
CXXFLAGS = -g -O3 -Wall

#Compiler
CXX = g++

#Sources
SRCS = PolicyAgent.cpp SarsaAgent.cpp

#Objects
OBJS = $(SRCS:.cpp=.o)

#Target
TARGET = libpolicyagent.a

#Rules 	

all: $(TARGET)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $(@F:%.o=%.cpp)

$(TARGET): $(OBJS)
	ar cq $@ $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS) *~

