#include <iostream>         // For std::cout, std::cerr
#include <thread>           // For std::thread, used to create threads for delayed and periodic sending
#include <string>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>          // For fcntl, used to set non-blocking mode
#include <unistd.h>         // For close, used to close sockets
#include <atomic>           // For std::atomic, used to control periodic sending
#include <vector>           // For std::vector,used to manage threads
#include <cstring>
#include <sys/time.h>       // For timeval


#define MAX_RETRIES 3u
#define DEFAULT_TIMEOUT_SECONDS 2u


class CommSender
{
    
    std::vector<std::thread> threads;
    std::atomic<bool> periodicRunning = true;
    int maxRetries = MAX_RETRIES;

    /* 
       Initializes a UDP socket and connects it to the specified IP and port.
       It creates a socket using the socket() function, 
       sets up the sockaddr_in structure with the provided IP and port, 
       and then connects the socket to the address using the connect() function.  
       Also add a default timeout for the socket send operations 
       so that blocking calls dont get stuck indefinitely.
    */

    int initSocket(std::string ip, int port)
    {
        struct timeval tv;
        tv.tv_sec  = DEFAULT_TIMEOUT_SECONDS;   // seconds
        tv.tv_usec = 0;   // microseconds
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &address.sin_addr);
        connect(sock, (struct sockaddr *)&address, sizeof(address));
        return sock;
    }
    /*
     Sends a message through the specified socket.
      It attempts to send the message using the send() function. 
      It will retry sending the message up to maxRetries times.
      If it fails a debug message is printed to the console indicating the failure.
    */
    void SendMessage(int sock, std::string message)
    {
        int sendSuccessful = 0;
        for (int attempt = 1; attempt <= maxRetries; attempt++)
        {
            ssize_t bytes = send(sock, message.c_str(), message.size(), 0);
            if (bytes >= 0)
            {
                sendSuccessful = 1;
                std::cout << "Send successful "  << std::endl;
                break;                
            }
            if (bytes == -1 && errno != EWOULDBLOCK)
            {
                std::cerr << "Send failed: " << strerror(errno) << std::endl;
                
            }
            std::cerr << "Send blocked, retry " << attempt << "/" << maxRetries << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if(!sendSuccessful)
        {
            std::cerr << "Send failed after " << maxRetries << " retries" << std::endl;
        }
        
    }


    public:

    /*
      Destructor :Stop Sending all the periodic messages.
      Also join all the threads created for delayed and periodic sending 
      to ensure they finish before the object is destroyed.
     */
    ~CommSender()
    {
        StopPeriodicSend();
        for (auto& t :threads)
        {if(t.joinable())
            t.join();
        }
    }

    void StopPeriodicSend()
    {
        periodicRunning = false;
    }

    /*
     Sends a message immediately to the specified IP and port.
    */
   void SendImmediate(std::string message,std::string ip,int port)
   {
         int sock = initSocket(ip, port);
         std::cout<< "Sending message immediately..." << std::endl;
         SendMessage(sock, message);
         close(sock);
   }

   /*
      Sends a message with a requested delay to the specified IP and port.
      It creates a new thread that waits for the specified delay before sending the message.
   */
    void SendWithDelay(std::string message,std::string ip,int port, uint8_t delay_in_seconds)
    {
        if(delay_in_seconds == 0)
        {
            std::cout << "Delay must be greater than 0 seconds." << std::endl;
        }
        else
        {

            threads.emplace_back(std::thread([this,message, port, ip, delay_in_seconds]()
            {   
                
                std::cout<< "Received request to send message.." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(delay_in_seconds));
                int sock = initSocket(ip, port);
                fcntl(sock, F_SETFL, O_NONBLOCK); // Set socket to non-blocking mode
                std::cout<< "Sending after delay of " << int(delay_in_seconds) << " seconds..." << std::endl;
                SendMessage(sock, message);
                close(sock);
            }));
        }

    }

    /*
      Sends a message periodically to the specified IP and port.
      It creates a new thread that sends the message periodically at specified intervals.
      Checks for the periodicRunning flag to determine when to stop sending messages.
    */
    void SendPeriodically(std::string message,std::string ip,int port, uint8_t interval_in_seconds)
    {
        if(interval_in_seconds == 0)
        {
            std::cout << "Interval must be greater than 0 seconds." << std::endl;
        }
        else
        {
            threads.emplace_back(std::thread([this, message, port, ip, interval_in_seconds]()
            {
                int sock = initSocket(ip, port);
                while (periodicRunning)
                {
                std::cout<< "Send message every " << int(interval_in_seconds) << " seconds..." << std::endl;
                SendMessage(sock, message);
                std::this_thread::sleep_for(std::chrono::seconds(interval_in_seconds));
                }
                close(sock);
            }));
        }

    }
};

int main ()
{   

    CommSender sender;
    sender.SendImmediate("Hello, World!","127.0.0.1", 8080);
    sender.SendWithDelay("Hello after 5 seconds!", "127.0.0.1", 8080,5);
    sender.SendPeriodically("Hello every 5 seconds!", "127.0.0.1", 8080, 5);

    // Keep main alive 
    std::cin.get();
}
