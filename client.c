#include <arpa/inet.h>
#include <bits/getopt_core.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_LENGTH 1024

#define RED "\x1b[01;31m"
#define GREEN "\x1b[01;32m"
#define YELLOW "\x1b[01;33m"
#define BLUE "\x1b[01;34m"
#define END "\x1b[0m"

#define UP "\x1b[1F"
#define DOWN "\x1b[1E"

#define OK GREEN "[+]" END " "
#define WARN YELLOW "[*]" END " "
#define ERROR RED "[-]" END " "

char client_name[20];
char server_name[20];
volatile int send_status = 0;
volatile int recv_status = 0;

pthread_t tid_send_msg;
pthread_t tid_recv_msg;

void help() {
  printf("\r");
  printf(OK "help\n");
  printf("you can input this command with correct parameter(s):\n\n");
  printf("exit\n\texit the channel\n");
  printf("sendfile [FILE NAME]\n\tsend a file to the other side\n");
  return;
}

void send_file(int sockfd, char *filename) {
  pthread_cancel(tid_recv_msg);
  printf(OK "ready to send file...\n");
  FILE *fp = fopen(filename, "rb");

  if (fp == NULL) {
    perror(ERROR "open file error!\n");
    send_status = 0;
    recv_status = 0;
    return;
  }

  struct stat statbuf;
  stat(filename, &statbuf);
  long size = statbuf.st_size;

  printf(OK "%s's size: %ldb\n", filename, size);

  char *buf = (char *)malloc(5000);
  char shake_hand[36] = {0};
  long count = 0;
  long size_has_sent = 0;
  int send_percent = 0;

  snprintf(shake_hand, 36, "\xbe\xef%ld\x00", size);

  send(sockfd, shake_hand, 36, 0);
  recv(sockfd, shake_hand, 2, 0);

  if (strncmp(shake_hand, "\xde\xad", 2)) {
    perror("\r" ERROR "shake hand error!\n");
    send_status = 0;
    recv_status = 0;
    return;
  }

  while ((count = fread(buf, 1, 4096, fp)) > 0) {
    size_has_sent += count;
    send_percent = (size_has_sent * 30) / size;
    // printf(WARN "%d/%d = %d\n", size * 30, size_has_sent, send_percent);
    printf("\r" OK "%30s | %ld/%ld", "", size_has_sent, size);
    printf("\r" OK);
    for (int i = 0; i < send_percent; ++i) {
      printf(">");
    }
    fflush(stdout);
    send(sockfd, buf, count, 0);
  }

  printf("\n" OK "file %s has been sent successfully.\n", filename);
  send_status = 0;
  recv_status = 0;
  return;
}

void recv_file(int sockfd, long size) {
  pthread_cancel(tid_send_msg);
  printf("\r" OK "Receive remote sending-file request\n");
  printf(OK "Please input a name for this file\n");
  printf(YELLOW ">" END " ");
  fflush(stdout);

  char *buf = (char *)malloc(5000);
  char filename[36];
  long count = 0;
  long size_has_sent = 0;
  int send_percent = 0;

  scanf("%20s", filename);
  printf(OK "Prepare to receive file\n");
  send(sockfd, "\xde\xad", 2, 0);

  FILE *fp = fopen(filename, "wb");

  while ((count = recv(sockfd, buf, 4096, 0)) > 0) {
    size_has_sent += count;
    send_percent = (size_has_sent * 30) / size;
    // printf(WARN "%d/%d = %d\n", size * 30, size_has_sent, send_percent);
    printf("\r" OK "%30s | %ld/%ld", "", size_has_sent, size);
    printf("\r" OK);
    for (int i = 0; i < send_percent; ++i) {
      printf(">");
    }
    fflush(stdout);
    fwrite(buf, 1, count, fp);

    if (size_has_sent == size) {
      break;
    }
  }
  printf("\n" OK "file %s has been received successfully.\n", filename);

  send_status = 0;
  recv_status = 0;
  free(buf);
  return;
}

void get_name() {
  printf(OK "please input your name (no more than 16 characters)\n");
  printf(YELLOW ">" END " ");

  scanf("%16s", client_name);
  fflush(stdin);
  return;
}

void exchange_name(int sockfd) {
  printf(OK "exchanging name...\n");
  send(sockfd, client_name, 20, 0);
  recv(sockfd, server_name, 20, 0);
  printf(OK "successful to receive server's name:" BLUE " %s" END "\n",
         server_name);
  return;
}

void *send_message(void *arg) {
  int sockfd = (int)arg;
  char *msg = (char *)malloc(1000);
  printf(YELLOW ">" END " ");
  fflush(stdout);

  int read_num = read(0, msg, 1000);
  fflush(stdin);

  if (!strncmp(msg, "exit", 4)) {
    printf(OK "bye!\n");
    close(sockfd);
    exit(0);
  }

  if (!strncmp(msg, "help", 4)) {
    help();
    send_status = 0;
    return NULL;
  }

  msg[read_num - 1] = '\0';

  if (!strncmp(msg, "sendfile", 8)) {
    send_file(sockfd, &msg[9]);
    return NULL;
  }

  if (send(sockfd, msg, strlen(msg) + 1, 0) == -1) {
    perror(ERROR "sending message to server failed!\n");
    send_status = 0;
    pthread_exit((void *)1);
  }

  printf(UP "\r[" YELLOW "%s" END "] : %s\n", client_name, msg);
  free(msg);

  send_status = 0;
  return NULL;
}

void *recv_message(void *arg) {
  int sockfd = (int)arg;
  char *msg = (char *)malloc(1000);

  int read_num = recv(sockfd, msg, 1000, 0);

  if (read_num == 0) {
    printf("\r" WARN "remote connection seems closed\n");
    close(sockfd);
    exit(0);
  }

  if (!strncmp(msg, "\xbe\xef", 2)) {
    recv_file(sockfd, atol(&msg[2]));
    return NULL;
  }

  printf("\r[" BLUE "%s" END "] : %s\n", server_name, msg);
  printf(YELLOW ">" END " ");
  fflush(stdout);

  free(msg);

  recv_status = 0;
  return NULL;
}

int new_socket() {
  int sockfd_client = 0;
  sockfd_client = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd_client == -1) {
    perror("create socket failed!\n");
    exit(1);
  }
  return sockfd_client;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: client hostname port\n");
    exit(1);
  }

  char *ip = argv[1];
  int port = atoi(argv[2]);

  char *msg_send = (char *)malloc(1000);
  char *msg_recv = (char *)malloc(1000);

  struct sockaddr_in client_addr = {0};

  int sockfd = new_socket();

  get_name();

  memset(&client_addr, 0, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_port = htons(port);
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  inet_pton(AF_INET, ip, &client_addr.sin_addr);

  printf(OK "client starting...\n");

  if (connect(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) ==
      -1) {
    perror(ERROR "connecting server failed!\n");
    exit(1);
  }
  printf(OK "successful to connect %s:%d\n", ip, port);
  exchange_name(sockfd);
  printf(OK "now you can input your message (no more than 1000 characters)\n");
  printf(OK "(input exit to quit, help for more infomation)\n");

  while (1) {
    if (send_status == 0) {
      pthread_create(&tid_send_msg, NULL, send_message, (void *)sockfd);
      send_status = 1;
    }

    if (recv_status == 0) {
      pthread_create(&tid_recv_msg, NULL, recv_message, (void *)sockfd);
      recv_status = 1;
    }
  }

  return 0;
}
