#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <vector>
#include <cstring>



class CommSender
{

    std::vector<std::thread> threads;
    std::atomic<bool> periodicRunning = true;
    int maxRetries = 3;

    int initSocket(std::string ip, int port)
    {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &address.sin_addr);
        connect(sock, (struct sockaddr *)&address, sizeof(address));
        return sock;
    }

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

   void SendImmediate(std::string message,std::string ip,int port)
   {
         int sock = initSocket(ip, port);
         std::cout<< "Sending message immediately..." << std::endl;
         SendMessage(sock, message);
         close(sock);
   }

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
    //sender.SendWithDelay("Hello after 5 seconds!", "127.0.0.1", 8080,5);
    sender.SendPeriodically("Hello every 5 seconds!", "127.0.0.1", 8080, 5);

    // Keep main alive long enough for detached threads to finish
    std::cin.get();
}
