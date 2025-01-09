all:
	g++ server.cpp -o server
	g++ -std=c++11 client.cpp -o client
