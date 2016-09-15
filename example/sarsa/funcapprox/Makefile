#Flags
CXXFLAGS = -g -O3 -Wall

#Compiler
CXX = g++

#Sources
SRCS = FuncApprox.cpp tiles2.cpp CMAC.cpp

#Objects
OBJS = $(SRCS:.cpp=.o)

#Target
TARGET = libfuncapprox.a

#Rules 	

all: $(TARGET)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $(@F:%.o=%.cpp)

$(TARGET): $(OBJS)
	ar cq $@ $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS) *~

