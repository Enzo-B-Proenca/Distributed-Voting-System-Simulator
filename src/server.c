#include "../include/server.h"

election_t globalElection;

command_type_t getCommandType(const char *command) {
  if (command == NULL)
    return CMD_INVALID;
  if (strcmp(command, "HELLO") == 0)
    return CMD_HELLO;
  if (strcmp(command, "LIST") == 0)
    return CMD_LIST;
  if (strcmp(command, "VOTE") == 0)
    return CMD_VOTE;
  if (strcmp(command, "SCORE") == 0)
    return CMD_SCORE;
  if (strcmp(command, "BYE") == 0)
    return CMD_BYE;
  return CMD_UNKNOWN;
}

void logEvent(const char *event) {
  pthread_mutex_lock(&globalElection.log_mutex);

  FILE *log_file = fopen(LOG_FILE, "a");

  if (log_file) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char time_str[26];
    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_file, "[%s] %s\n", time_str, event);

    fclose(log_file);
  } else {
    fprintf(stderr, "ERRO: Não foi possível abrir o arquivo de log (%s).\n",
            event);
  }

  pthread_mutex_unlock(&globalElection.log_mutex);
}

uint8_t hasVoted(const char *voter_id) {
  for (int i = 0; i < globalElection.total_voters; i++)
    if (strcmp(globalElection.unique_voters[i], voter_id) == 0)
      return 1;

  return 0;
}

uint8_t addVoter(const char *voter_id) {
  if (globalElection.total_voters < MAX_VOTERS) {
    strcpy(globalElection.unique_voters[globalElection.total_voters], voter_id);

    globalElection.total_voters++;

    return 1;
  }

  return 0;
}

void getScoreString(char *buffer) {
  int options_count = globalElection.num_options;
  snprintf(buffer, BUF_SIZE, "SCORE %d", options_count);
  for (int i = 0; i < options_count; i++) {
    char temp[100];

    snprintf(temp, 100, " %s:%d", globalElection.options[i].name,
             globalElection.options[i].count);

    strncat(buffer, temp, BUF_SIZE - strlen(buffer) - 1);
  }
}

void *clientHandler(void *socket) {
  int sock = *(int *)socket;
  char buffer[BUF_SIZE];
  char voter_id[50] = "";
  int read_size;

  pthread_detach(pthread_self());

  while ((read_size = recv(sock, buffer, BUF_SIZE - 1, 0)) > 0) {
    buffer[read_size] = '\0';
    buffer[strcspn(buffer, "\r\n")] = 0;

    printf("[CLIENTE %d] Recebido: %s\n", sock, buffer);
    char response[BUF_SIZE] = "";

    char temp_buffer[BUF_SIZE];
    strcpy(temp_buffer, buffer);
    char *command = strtok(temp_buffer, " ");
    char *arg1 = strtok(NULL, " ");

    pthread_mutex_lock(&globalElection.vote_mutex);
    int closed = globalElection.is_closed;
    pthread_mutex_unlock(&globalElection.vote_mutex);

    command_type_t cmd_type = getCommandType(command);

    if (closed && cmd_type != CMD_BYE) {
      char final_score[BUF_SIZE];
      pthread_mutex_lock(&globalElection.vote_mutex);
      getScoreString(final_score);
      pthread_mutex_unlock(&globalElection.vote_mutex);

      sprintf(response, "CLOSED FINAL %s\n", final_score + 6);
      write(sock, response, strlen(response));
      break;
    }

    switch (cmd_type) {
    case CMD_HELLO:
      if (arg1 != NULL) {
        if (!closed) {
          strcpy(voter_id, arg1);
          sprintf(response, "WELCOME %s\n", voter_id);
          char log_msg[100];
          sprintf(log_msg, "Cliente conectado: ID=%s", voter_id);
          logEvent(log_msg);
        } else
          strcpy(response, "ERR CLOSED\n");
      } else
        strcpy(response, "ERR MSSING_VOTER_ID\n");
      break;
    case CMD_LIST:
      if (strlen(voter_id) == 0)
        strcpy(response, "ERR NOT_IDENTIFIED\n");
      else {
        sprintf(response, "OPTIONS %d", globalElection.num_options);
        for (int i = 0; i < globalElection.num_options; i++) {
          char temp[100];
          sprintf(temp, " %s", globalElection.options[i].name);
          strcat(response, temp);
        }
        strcat(response, "\n");
      }
      break;
    case CMD_VOTE:
      if (strlen(voter_id) == 0)
        strcpy(response, "ERR NOT_IDENTIFIED\n");
      else if (arg1 == NULL)
        strcpy(response, "ERR MISSING_OPTION\n");
      else {
        pthread_mutex_lock(&globalElection.vote_mutex);

        if (hasVoted(voter_id))
          strcpy(response, "ERR DUPLICATE\n");
        else {
          int valid_vote = 0;
          for (int i = 0; i < globalElection.num_options; i++) {
            if (strcmp(globalElection.options[i].name, arg1) == 0) {
              globalElection.options[i].count++;
              addVoter(voter_id);
              sprintf(response, "OK VOTED %s\n", arg1);
              char log_msg[100];
              sprintf(log_msg, "Voto registrado: ID=%s, Opção=%s", voter_id,
                      arg1);
              logEvent(log_msg);
              valid_vote = 1;
              break;
            }
          }
          if (!valid_vote)
            strcpy(response, "ERR INVALID_OPTION\n");
        }
        pthread_mutex_unlock(&globalElection.vote_mutex);
      }
      break;
    case CMD_SCORE:
      if (strlen(voter_id) == 0)
        strcpy(response, "ERR NOT_IDENTIFIED\n");
      else {
        pthread_mutex_lock(&globalElection.vote_mutex);
        getScoreString(response);
        strcat(response, "\n");
        pthread_mutex_unlock(&globalElection.vote_mutex);
      }
      break;
    case CMD_BYE:
      strcpy(response, "BYE\n");
      write(sock, response, strlen(response));
      goto end_of_session;

    case CMD_INVALID:
      strcpy(response, "ERR INVALID_COMMAND\n");
      break;

    case CMD_UNKNOWN:
    default:
      strcpy(response, "ERR UNKNOWN_COMMAND\n");
      break;
    }

    write(sock, response, strlen(response));
  }

end_of_session:
  if (read_size == 0)
    printf("[CLIENTE %d] Cliente desconectado. ID=%s \n", sock, voter_id);
  else if (read_size == -1)
    perror("[CLIENTE] Erro de recv");

  char log_msg_close[100];
  sprintf(log_msg_close, "Sessão encerrada: Socket=%d, ID=%s", sock, voter_id);
  logEvent(log_msg_close);

  close(sock);
  free(socket);
  return NULL;
}
int main() { return EXIT_SUCCESS; }
