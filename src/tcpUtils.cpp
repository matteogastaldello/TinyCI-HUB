#include <Arduino.h>
#include <tcpUtils.hpp>

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
                                    .tv_nsec = now.tv_nsec + timeout_ms * 1000000l};
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
String getSubnetString(String ip, String out)
{
  int i;
  for (i = ip.length() - 1; i > 0 && ip[i] != '.'; i--)
    ;
  out = ip.substring(0, i);
  Serial.println(out);
  return out;
}
int firstTCPMessage(int sockfd)
{
  char buff[MAX];
  int n;
  bzero(buff, sizeof(buff));
  // Serial.println("Enter the string : ");
  sprintf(buff, "Test connection");
  write(sockfd, buff, sizeof(buff));
  // wait for ack
  bzero(buff, sizeof(buff));
  Serial.println("Waiting ACK from Server...");
  read(sockfd, buff, sizeof(buff));
  char message[MAX];
  sprintf(message, "ACK Message : %s", buff);
  Serial.println(message);

  if ((strcmp(buff, "MSP432_ACK")) == 0)
  {
    Serial.println("Device Sent ACK!");
    // sprintf(buff, "exit");
    // write(sockfd, buff, sizeof(buff));
    return 0; // success, ack received
  }
  return -1;
}
String deviceDiscovery(int ip_start, int ip_end)
{
  char outBuf[50];
  int sockfd, connfd;
  struct sockaddr_in servaddr, cli;
  String subIP;
  subIP = getSubnetString(WiFi.localIP().toString(), subIP);

  for (int i = ip_start; i < ip_end; i++)
  {
    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
      Serial.println("socket creation failed...\n");
      exit(0);
    }
    bzero(&servaddr, sizeof(servaddr));
    char ip[MAX_IPLEN] = "";
    sprintf(ip, "%s.%d", subIP, i);
    sprintf(outBuf, "ip: %s\t", ip);
    Serial.print(outBuf);
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    int ra = connect_with_timeout(sockfd, (SA *)&servaddr, sizeof(servaddr), 2000);
    if (ra >= 0)
    {
      Serial.println("connected to the server...");
      // function for chat
      int rm = firstTCPMessage(sockfd);
      // close the socket
      close(sockfd);
      if (rm == 0)
      {
        return String(ip);
      }
    }
    else
    {
      sprintf(outBuf, "Addr not working... %d", ra);
      Serial.println(outBuf);
    }
    Serial.println("closing socket...\n");
    close(sockfd);
  }
  return "error";
}
int openSocket(const char *ip)
{
  int sockfd, connfd;
  struct sockaddr_in servaddr, cli;
  // socket create and verification
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1)
  {
    Serial.println("socket creation failed...\n");
    return -3;
  }
  bzero(&servaddr, sizeof(servaddr));
  // assign IP, PORT
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(ip);
  servaddr.sin_port = htons(PORT);

  // connect the client socket to server socket
  return connect_with_timeout(sockfd, (SA *)&servaddr, sizeof(servaddr), 2000);
}
int sendTCPMessage(const char *ip, const char *message, int message_len)
{
  if (message_len >= MAX)
    return -2;

  // open socket
  int sockfd = openSocket(ip);
  if (sockfd >= 0)
  {
    char buff[MAX];
    bzero(buff, sizeof(buff));
    sprintf(buff, "Test Set");
    //strcpy(buff, message);
    Serial.println(buff);
    write(sockfd, buff, sizeof(buff));
    // wait for ack
    bzero(buff, sizeof(buff));
    Serial.println("Waiting ACK from Server...");
    read(sockfd, buff, sizeof(buff));
    char rcv_message[MAX];
    sprintf(rcv_message, "ACK Message : %s", buff);
    Serial.println(rcv_message);

    if ((strcmp(buff, "MSP432_ACK")) == 0)
    {
      Serial.println("Device Sent ACK!");
      return 0; // success, ack received
    }
  }
  return -1;
}