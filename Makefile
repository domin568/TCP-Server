CXX = g++
CXXFLAGS_CC = -Wall -std=c++14 
LDFLAGS_CC = 

CXXFLAGS_COMMAND = -Wall -std=c++14 
LDFLAGS_COMMAND = -pthread -lreadline

OBJDIR := obj

COMMAND := src/command.cpp
CC2 := src/cc2.cpp

.PHONY: all

all : cc2 command

cc2: obj/cc2.o
	$(CXX) $^ $(LDFLAGS_CC) -o $@
obj/cc2.o: $(CC2)
	@mkdir -p obj 
	$(CXX) $(CXXFLAGS_CC) -c $< -o $@

command: obj/command.o
	$(CXX) $^ $(LDFLAGS_COMMAND) -o $@
obj/command.o: $(COMMAND)
	@mkdir -p obj 
	$(CXX) $(CXXFLAGS_COMMAND) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJDIR) command cc2