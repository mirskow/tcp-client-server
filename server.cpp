#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <signal.h>
#include <mutex>
#include <fstream>
#include <netinet/in.h>
#include <atomic>
#include <vector>
#include <thread>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <fcntl.h>
#include <sstream>
#include <iomanip>

std::atomic<bool> isServerRunning;
int serverSocket;
std::queue<int> client_queue;
std::mutex queue_mutex;
std::condition_variable queue_condition;


std::string generate_file_name()
{
	auto now = std::chrono::system_clock::now();
	time_t now_c = std::chrono::system_clock::to_time_t(now);
	struct tm* parts = std::localtime(&now_c);
	
	std::ostringstream oss;
	oss << std::put_time(parts, "%Y-%m-%d_%H-%M-%S");
	return oss.str();
}

void response_handler(int clientSocket, std::string response)
{
	int sendResult = send(clientSocket, response.c_str(), response.size() + 1, 0);
	if(sendResult == -1)
	{
		std::cerr << "Error sending data to client" << std::endl;
	}
}

void signal_handler(int sig) 
{
	std::cout << "Exiting server..." << std::endl;
	isServerRunning.store(false);
	queue_condition.notify_all();
	close(serverSocket);
}

void connection_handler(std::string path, int MAX_FILE_SIZE)
{
	while(isServerRunning.load())
	{
	        int client_socket;
		{
		    std::unique_lock<std::mutex> lock(queue_mutex);
		    std::cout << std::this_thread::get_id() << " wait connection or stop server" << std::endl;
		    queue_condition.wait(lock, [=]() { return !client_queue.empty() || !isServerRunning.load();});
		    
		    if(!isServerRunning.load()) {
		    	break; 
		    }
		    
		    client_socket = client_queue.front();
		    client_queue.pop();
		}
		
		int flags = fcntl(client_socket, F_GETFL, 0);
		if (flags == -1) {
		    std::cerr << "Error getting socket flags" << std::endl;
		} else {
		    if (fcntl(client_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
			std::cerr << "Error setting socket to non-blocking mode" << std::endl;
		    }
		}
		
		char buf[MAX_FILE_SIZE];
		memset(buf, 0, sizeof(buf));
		
		int bytes_received;
		int bytesRead = 0;
		std::string received_data;
		
		while ((bytes_received = recv(client_socket, buf, sizeof(buf), 0)) > 0) {
			std::cout << "Start reading...";
		    	bytesRead += bytes_received;
		    	received_data.append(buf, bytes_received);
		    	memset(buf, 0, sizeof(buf));
		}
				
		if(bytes_received == -1 && (errno != EWOULDBLOCK || errno != EAGAIN))
		{
			std::cerr << "There was a connection issue" << std::endl;
			close(client_socket);
			continue;
		}
		
		if(bytesRead > MAX_FILE_SIZE)
		{
			response_handler(client_socket, "Maximum file size " + std::to_string(MAX_FILE_SIZE) + " exceeded");
			close(client_socket);
			continue;
		}
		
		std::string filename = path + "/" + generate_file_name() + ".txt";
            	std::ofstream file(filename);
            	if (!file) 
            	{
                	std::cerr << "Failed to create file" << std::endl;
            	} 
            	else 
            	{
                	file << received_data;
                	file.close();      	
            		std::cout << std::this_thread::get_id() << " save file from client " << client_socket << std::endl;           
                	response_handler(client_socket, "File received and saved.");
            	}
            	
    		close(client_socket);		
	}
	
}

void thread_handler(int serverSocket, int MAX_THREAD, int MAX_FILE_SIZE, std::string PATH_SAVE_DIRECTORY) 
{
	std::vector<std::thread> threads;
	for (int i = 0; i < MAX_THREAD; ++i) {
		threads.emplace_back(connection_handler, PATH_SAVE_DIRECTORY, MAX_FILE_SIZE);
	}
	while (isServerRunning.load()) 
	{
		struct sockaddr_in client_addr;
		socklen_t client_size = sizeof(client_addr);
		
		int client_socket = accept(serverSocket, (struct sockaddr *)&client_addr, &client_size);
		
		if (client_socket == -1 && isServerRunning.load()) {
		    std::cerr << "Error in accepting connection" << std::endl;
		    continue;
		}
		
		if(client_socket == -1 && !isServerRunning.load()) { break; }
		
		{
		    std::unique_lock<std::mutex> lock(queue_mutex);
		    client_queue.push(client_socket);
		}
		
		queue_condition.notify_one();
	}
	std::cout << "Wait all threads" << std::endl;
	for (auto& thread : threads) { thread.join(); }
}

int main(int argc, char* argv[])
{
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	
	if(argc < 5)
	{
		std::cerr << "Usage: ./server <max_thread> <port> <max_filesize> <save_directory>" << std::endl;
		return 1;
	}
	
	int MAX_THREAD = std::stoi(argv[1]);
	int PORT = std::stoi(argv[2]);
	int MAX_FILE_SIZE = std::stoi(argv[3]);
	std::string PATH_SAVE_DIRECTORY = argv[4];
	
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(serverSocket == -1)
	{
		std::cerr << "Can't create a socket." << std::endl;
		return -1;
	}
	
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(PORT);
	inet_pton(AF_INET, "0.0.0.0", &hint.sin_addr);
	
	if(bind(serverSocket, (sockaddr*)&hint, sizeof(hint)) == -1)
	{
		std::cerr << "Can't bind to IP/port" << std::endl;
		return -2;
	}
	
	if(listen(serverSocket, MAX_THREAD) == -1)
	{
		std::cerr << "Can't listen" << std::endl;
		return -3;
	}
	
	isServerRunning.store(true);
	thread_handler(serverSocket, MAX_THREAD, MAX_FILE_SIZE, PATH_SAVE_DIRECTORY);
	
	std::cout << "Server stopped" << std::endl;
}
