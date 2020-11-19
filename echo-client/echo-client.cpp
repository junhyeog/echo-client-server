#include <arpa/inet.h>
#include <semaphore.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <thread>

using namespace std;

void usage() {
  printf("syntax : syntax : echo-client <ip> <port>\n");
  printf("sample : echo-client 192.168.10.2 1234\n");
  return;
}

void *receive(void *arg) {
  int server_sockfd = *(int *)arg;
  const static int BUFSIZE = 100000;  // 65536
  char buf[BUFSIZE];
  while (true) {
    ssize_t response_len = recv(server_sockfd, buf, BUFSIZE - 1, 0);
    if (response_len == 0 || response_len == -1) {
      perror("Failed to recieve");
      break;
    }
    buf[response_len] = '\0';
    printf("%s\n", buf);
  }
  printf("Server(%d) disconnected\n", server_sockfd);
  close(server_sockfd);
  return nullptr;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    usage();
    return -1;
  }
  int res;  //* for error check

  //? Get ip & port
  struct in_addr ip;
  int port = atoi(argv[2]);
  res = inet_pton(AF_INET, argv[1], &ip);
  if (res < 0) {
    perror("Failed to get ip address");
    return -1;
  } else if (res == 0) {
    perror("Invalid ip address");
    return -1;
  }

  //? Create a socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Failed to create a socket");
    return -1;
  }

  //? Build a socket address
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = ip.s_addr;
  memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

  //? Connect
  res = connect(sockfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  if (res < 0) {
    perror("Failed to connect");
    return -1;
  }
  printf("[+] Connected\n");

  //? Receive
  thread t(receive, (void *)&sockfd);
  t.detach();

  //? Request
  ssize_t sent_len;
  const static int BUFSIZE = 100000;  //65536
  char buf[BUFSIZE];
  while (true) {
    fgets(buf, BUFSIZE, stdin);
    sent_len = send(sockfd, buf, strlen(buf) - 1, 0);
    if (sent_len == 0 || sent_len == -1) {
      perror("Failed to send request");
      break;
    }
  }

  close(sockfd);
  return 0;
}