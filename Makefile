all: chatserver chatclient

chatserver: chatserver.cc
	g++ chatserver.cc -o chatserver -lpthread

chatclient: chatclient.cc
	g++ chatclient.cc -o chatclient -lpthread

clean:
	rm -f chatserver chatclient
