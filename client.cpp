#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <cstring>
#include <netdb.h>
#include <arpa/inet.h>
#include <fstream>
#include <chrono>
#include <thread>

int main(int argc, char* argv[])
{
	if(argc < 4)
	{
		std::cerr << "Usage: ./client <\"ip\"> <port> <filename>" << std::endl;
		return 1;
	}
	
	std::string file_path = argv[3];
	std::ifstream file(file_path, std::ios::binary);
	if(!file)
	{
		std::cerr << "Error file opening" << std::endl;
		return 1;
	}
	
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1)
	{
		return 1;
	}
	
	int port = std::stoi(argv[2]);
	std::string ipAddr = argv[1];
	
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(port); 
	inet_pton(AF_INET, ipAddr.c_str(), &hint.sin_addr);
	
	if(connect(sock, (sockaddr*)&hint, sizeof(hint)) == -1)
	{
		std::cerr << "Error connecting to server..." << std::endl;
		return 1;
	}
	
	char buf[4096];
	while(file.good())
	{
		file.read(buf, sizeof(buf));
		int bytesToRead = file.gcount();
		int bytesSent = 0;

		while (bytesSent < bytesToRead)
		{
		    int sendResult = send(sock, buf + bytesSent, bytesToRead - bytesSent, 0);
		    if(sendResult == -1)
		    {
		        std::cerr << "Error sending data to server" << std::endl;
		        close(sock);
		        return 1;
		    }
		    bytesSent += sendResult;
		}
	}
	
	std::cout << "Wait response from server..." << std::endl;
	memset(buf, 0, 4096);
	int bytesRec = recv(sock, buf, 4096, 0);
	std::cout << "SERVER > " << std::string(buf, bytesRec) << std::endl;
	
	close(sock);
	return 0;
}

