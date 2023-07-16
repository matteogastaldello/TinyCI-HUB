#include <Arduino.h>
#include <utils/tcpUtils.hpp>
#include <utils/stringUtils.hpp>

// function used to connect to a server specified at addr
int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, unsigned int timeout_ms)
{
  int rc = 0;
  // Set O_NONBLOCK
  int sockfd_flags_before;
  if ((sockfd_flags_before = fcntl(sockfd, F_GETFL, 0) < 0))
    return -1;
  if (fcntl(sockfd, F_SETFL, sockfd_flags_before | O_NONBLOCK) < 0)
    return -1;
  // Start connecting (asynchronously)
  do
  {
    if (connect(sockfd, addr, addrlen) < 0)
    {
      // Did connect return an error? If so, we'll fail.
      if ((errno != EWOULDBLOCK) && (errno != EINPROGRESS))
      {
        rc = -1;
      }
      // Otherwise, we'll wait for it to complete.
      else
      {
        // Set a deadline timestamp 'timeout' ms from now (needed b/c poll can be interrupted)
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        {
          rc = -1;
          break;
        }
        struct timespec deadline = {.tv_sec = now.tv_sec,
                                    .tv_nsec = now.tv_nsec + (long)timeout_ms * 1000000l};
        // Wait for the connection to complete.
        do
        {
          // Calculate how long until the deadline
          if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
          {
            rc = -1;
            break;
          }
          int ms_until_deadline = (int)((deadline.tv_sec - now.tv_sec) * 1000l + (deadline.tv_nsec - now.tv_nsec) / 1000000l);
          if (ms_until_deadline < 0)
          {
            rc = 0;
            break;
          }
          // Wait for connect to complete (or for the timeout deadline)
          struct pollfd pfds[] = {{.fd = sockfd, .events = POLLOUT}};
          rc = poll(pfds, 1, ms_until_deadline);
          // If poll 'succeeded', make sure it *really* succeeded
          if (rc > 0)
          {
            int error = 0;
            socklen_t len = sizeof(error);
            int retval = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
            if (retval == 0)
              errno = error;
            if (error != 0)
              rc = -1;
          }
        }
        // If poll was interrupted, try again.
        while (rc == -1 && errno == EINTR);
        // Did poll timeout? If so, fail.
        if (rc == 0)
        {
          errno = ETIMEDOUT;
          rc = -1;
        }
      }
    }
  } while (0);
  // Restore original O_NONBLOCK state
  if (fcntl(sockfd, F_SETFL, sockfd_flags_before) < 0)
    return -1;
  // Success
  return rc;
}
// function used to simulate an handshake between the retrieved device and the ESP
int sendACKMessage(int sockfd, const char *msg, int msgSize)
{
  char buff[MAX];
  bzero(buff, sizeof(buff));
  strncpy(buff, msg, msgSize);
  write(sockfd, buff, sizeof(buff));
  // wait for ack
  bzero(buff, sizeof(buff));
  Serial.println("Waiting ACK from Server...");
  read(sockfd, buff, sizeof(buff));
  char message[MAX];
  sprintf(message, "ACK Message : %s", buff);
  Serial.println(message);

  if ((strcmp(buff, HANDSHAKE_ACK)) == 0)
  {
    Serial.println("Device Sent ACK!");
    // sprintf(buff, "exit");
    // write(sockfd, buff, sizeof(buff));
    return 0; // success, ack received
  }
  return -1;
}
// function used to simulate an handshake between the retrieved device and the ESP
int sendAndReceiveMessage(int sockfd, const char *msg, int msgSize, char * responseBuf, int responseLen)
{
  char buff[MAX];
  write(sockfd, msg, msgSize);
  Serial.print("message sent: ");
  Serial.println(msg);
  // wait for ack
  bzero(buff, sizeof(buff));
  Serial.println("Waiting Response from Server...");
  read(sockfd, buff, sizeof(buff));
  char message[MAX];
  sprintf(message, "Message : %s", buff);
  Serial.println(message);
  strcpy(responseBuf, buff);
  Serial.println("Message received from server!");
  return 0; // success, ack received
}
// Open a socket at the specified ip
int openSocket(const char *ip, int *sockfd)
{
  int connfd;
  struct sockaddr_in servaddr, cli;
  // socket create and verification
  *sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (*sockfd == -1)
  {
    Serial.println("socket creation failed...\n");
    return -3;
  }
  bzero(&servaddr, sizeof(servaddr));
  // assign IP, PORT
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(ip);
  servaddr.sin_port = htons(PORT);

  int ra = connect_with_timeout(*sockfd, (SA *)&servaddr, sizeof(servaddr), 2000);
  Serial.print("Socket: ");
  Serial.println(ra);
  Serial.println(*sockfd);
  // connect the client socket to server socket
  return ra;
}
// the result is equals to "int sendMessage(int sockfd, const char * msg, int msgSize)" but you can specify ip instead of sockfd
// note: the sockfd is created in the function
int sendAndReceiveMessage(const char *ip, const char *message, int message_len, char * responseBuf, int responseLen)
{
  int sockfd;
  int ra = openSocket(ip, &sockfd);
  if (ra >= 0)
  {
    Serial.println("connected to the server...");
    // function for chat
    Serial.print("Message is: ");
    Serial.println(message);
    int rm = sendAndReceiveMessage(sockfd, message, message_len, responseBuf, responseLen);
    // close the socket
    close(sockfd);
    if (rm == 0)
    {
      Serial.println("Message Sent!");
      Serial.println("closing socket...\n");
      close(sockfd);
    }
    else
      return -2; // impossible to send message
  }
  return 0;
}