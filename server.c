#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SOCK_PORT 9988
#define BUFFER_LENGTH 1024
#define MAX_CONN_LIMIT 512

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

int sockfd_server = 0;

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
  fflush(stdout);
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

  scanf("%16s", server_name);
  fflush(stdin);
  return;
}

void exchange_name(int sockfd) {
  printf(OK "exchanging name...\n");
  send(sockfd, server_name, 20, 0);
  recv(sockfd, client_name, 20, 0);
  printf(OK "successful to receive client's name:" BLUE " %s" END "\n",
         client_name);
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
    close(sockfd_server);
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

  printf(UP "\r[" YELLOW "%s" END "] : %s\n", server_name, msg);
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
    close(sockfd_server);
    exit(0);
  }

  if (!strncmp(msg, "\xbe\xef", 2)) {
    recv_file(sockfd, atol(&msg[2]));
    return NULL;
  }

  printf("\r[" BLUE "%s" END "] : %s\n", client_name, msg);
  printf(YELLOW ">" END " ");
  fflush(stdout);

  free(msg);

  recv_status = 0;
  return NULL;
}

int main(int argc, char *argv[]) {
  struct sockaddr_in server_addr = {0};
  int readnum = 0;
  char buf[128] = {0};
  int listenResult = -1;
  int sockfd_connect = -1;

  printf(OK "Server Starting...\n");

  sockfd_server = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd_server == -1) {
    perror(ERROR "create socket failed!\n");
    exit(1);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(SOCK_PORT);
  if (bind(sockfd_server, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) == -1) {
    perror(ERROR "bind socket error!\n");
    exit(1);
  }

  get_name();

  if (listen(sockfd_server, MAX_CONN_LIMIT) == -1) {
    perror(ERROR "listen socket error");
    exit(1);
  }
  printf(OK "Server start successs，listening client message...\n");

  if ((sockfd_connect = accept(sockfd_server, NULL, NULL)) == -1) {
    perror(ERROR "receiving message error!\n");
    exit(1);
  }
  printf(OK "successful to connect\n");

  exchange_name(sockfd_connect);

  printf(OK "now you can input your message (no more than 1000 characters)\n");
  printf(OK "(input exit to quit, help for more infomation)\n");

  while (1) {
    if (send_status == 0) {
      pthread_create(&tid_send_msg, NULL, send_message, (void *)sockfd_connect);
      send_status = 1;
    }

    if (recv_status == 0) {
      pthread_create(&tid_recv_msg, NULL, recv_message, (void *)sockfd_connect);
      recv_status = 1;
    }
  }

  return 0;
}
