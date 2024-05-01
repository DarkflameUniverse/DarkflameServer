#include <iostream>

#include <TCPInterface.h>
#include <HTTPConnection.h>
#include <RakSleep.h>

int main(int argc, const char** argv) {
	std::cout << "Hello World!" << std::endl;

	TCPInterface tcp;
	if (!tcp.Start(0, 0)) {
		std::cerr << "Failed to start TCP Interface" << std::endl;
		return 1;
	}

	SystemAddress addr = tcp.Connect("127.0.0.1", 8000);
	std::cout << "addr: " << addr.ToString() << std::endl;

	std::string req = "\
GET /helloFromRakNet.txt HTTP/1.1\r\n\
User-Agent: raknet / 3.25\r\n\
\r\n";
	std::cout << req;

	tcp.Send(req.c_str(), req.size(), addr);

	RakSleep(500);

	tcp.CloseConnection(addr);

	return 0;
}
