DEBUGFLAG=-DNDEBUG

CXXFLAGS=-std=c++0x -Wall $(DEBUGFLAG) 

LIBS=-lboost_program_options -lboost_system -lboost_thread -lpthread

#----------------------------------------#

all: runserver runclient

runserver: runserver.o mixer.o session.o server.o
	$(CXX) -o $@ $^ $(LIBS)

runserver.o: runserver.cpp server.h server_params.h
	$(CXX) -c $(CXXFLAGS) -o $@ $<

mixer.o: mixer.cpp mixer.h
	$(CXX) -c $(CXXFLAGS) -o $@ $<

session.o: session.cpp session.h server_params.h
	$(CXX) -c $(CXXFLAGS) -o $@ $<

server.o: server.cpp server.h session.h server_params.h mixer.h
	$(CXX) -c $(CXXFLAGS) -o $@ $<


runclient: runclient.o client.o
	$(CXX) -o $@ $^ $(LIBS)

runclient.o: runclient.cpp client.h client_params.h
	$(CXX) -c $(CXXFLAGS) -o $@ $<

client.o: client.cpp client.h client_params.h
	$(CXX) -c $(CXXFLAGS) -o $@ $<


clean:
	rm --force *.o

distclean: clean
	rm --force runserver runclient

.PHONY: all clean cleandist
