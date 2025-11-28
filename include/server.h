#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define MAX_OPTIONS 10
#define MAX_VOTERS 100
#define PORT 8080
#define LOG_FILE "eleicao.log"
#define FINAL_RESULT_FILE "resultado_final.txt"

typedef struct {
  char name[50];
  uint8_t count;
} option_t;

typedef struct {
  uint8_t is_closed;
  uint8_t num_options;
  option_t options[MAX_OPTIONS];

  char unique_voters[MAX_VOTERS][50];
  uint8_t total_voters;

  pthread_mutex_t vote_mutex;
  pthread_mutex_t log_mutex;

} election_t;

typedef enum {
  CMD_INVALID = 0,
  CMD_HELLO,
  CMD_LIST,
  CMD_VOTE,
  CMD_SCORE,
  CMD_BYE,
  CMD_UNKNOWN
} command_type_t;

extern election_t election;

void logEvent(const char *event);
void getScoreString(char *buffer);
void saveFinalResult();
uint8_t hasVoted(const char *voter_id);
uint8_t addVoter(const char *voter_id);
command_type_t getCommandType(const char *command);

void *clientHandler(void *socket);
void *adminThread(void *arg);
#endif
