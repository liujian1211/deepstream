#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
<<<<<<< HEAD
#include <vector>
#include <mutex>
#include <queue>
#include "messageGenerate.h"
// #include "getGPS.h"
// #include "deepstream_test.h"

void sendHeartbeat(int sockfd);
void executeCommand(); //执行cmd命令 
void sendLocation(int sockfd);
int MessageType(int sockfd);

class tcpIpMessage
{
    private:

    public:
        int message();
        
};
=======

int message();

void sendHeartbeat(int sockfd);

void sendLocation(int sockfd);

int MessageType(int sockfd);

void executeCommand(); //执行cmd命令 
void terminateCommand(); //终止上述命令
>>>>>>> 329743696f9a96baab72e2c31a85a6480e200af4
