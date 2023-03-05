#include <arpa/inet.h> // inet_addr()
#include <Arduino.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <time.h>
#include <errno.h>
#include <WiFi.h>


#define MAX 80
#define MAX_DEVICES 5
#define MAX_IPLEN 16
#define PORT 8080
#define SA struct sockaddr


int firstTCPMessage(int sockfd);
int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, unsigned int timeout_ms);
String getSubnetString(String ip, String out);
String deviceDiscovery(int ip_start, int ip_end);
int sendTCPMessage(const char * ip, const char * message, int message_len);
int sendMessage(const char *ip, const char *message, int message_len);