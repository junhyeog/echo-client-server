#include <arpa/inet.h>
#include <semaphore.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <set>
#include <thread>

using namespace std;

sem_t m;

bool echo_flag = 0, broadcast_flag = 0;
set<int> client_set;

void usage() {
  printf("syntax : echo-server <port> [-e[-b]]\n");
  printf("\t-e : echo\n");
  printf("\t-b : broadcast\n");
  printf("sample : echo-server 1234 -e -b\n");
  return;
}

void* echo_response(void* arg) {
  int client_sockfd = *(int*)arg;
  ssize_t response_len, sent_len;
  const static int BUFSIZE = 100000;  //65536
  char buf[BUFSIZE];
  while (true) {
    //? Receive response
    response_len = recv(client_sockfd, buf, BUFSIZE - 1, 0);
    if (response_len == 0 || response_len == -1) {
      perror("Failed to receive");
      break;
    }
    buf[response_len] = '\0';

    //? Print response
    printf("%s\n", buf);

    //? broadcast mode
    if (broadcast_flag) {
      sem_wait(&m);
      for (auto sockfd : client_set) {
        sent_len = send(sockfd, buf, strlen(buf), 0);
        if (sent_len == 0 || sent_len == -1) {
          perror("Failed to broadcast");
          break;
        }
      }
      sem_post(&m);
    }
    //? echo mode
    else if (echo_flag) {
      sent_len = send(client_sockfd, buf, strlen(buf), 0);
      if (sent_len == 0 || sent_len == -1) {
        perror("Failed to echo");
        break;
      }
    }
  }
  // end of while
  //* Remove client
  printf("Client(%d) disconnected\n", client_sockfd);
  sem_wait(&m);
  client_set.erase(client_sockfd);
  sem_post(&m);
  close(client_sockfd);
  return nullptr;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    usage();
    return -1;
  }
  sem_init(&m, 0, 1);
  int res;  //* for error check

  //* Get port number
  int port = atoi(argv[1]);

  //* Parse options
  while ((res = getopt(argc, argv, "eb")) != -1) {
    switch (res) {
      case 'e':
        echo_flag = 1;
        break;
      case 'b':
        broadcast_flag = 1;
        break;
      default:
        perror("Unexpected flag");
        usage();
        return -1;
    }
  }

  //* Create a socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    perror("Failed to create a socket");
    return -1;
  }

  //* Set socket options
  int optval = 1;
  res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  if (sockfd == -1) {
    perror("Failed to create a socket");
    return -1;
  }

  //* Build a socket address
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

  //* Bind
  res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
  if (res == -1) {
    perror("Failed to bind");
    return -1;
  }

  //* Listen - create queue
  res = listen(sockfd, SOMAXCONN);
  if (res == -1) {
    perror("Failed to listen");
    return -1;
  }

  while (true) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sockfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    if (client_sockfd == -1) {
      perror("Failed to accept");
      break;
    }
    printf("[+] Client connected\n");

    //* Insert client in set
    sem_wait(&m);
    client_set.insert(client_sockfd);
    sem_post(&m);

    //* Create new thread
    thread t(echo_response, (void*)&client_sockfd);
    t.detach();
  }

  close(sockfd);
  sem_destroy(&m);
  return 0;
}
