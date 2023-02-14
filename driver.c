#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct command {
	char* cmd;
	int argc;
	char* argv[512];
};

void free_command(struct command* cmd) {
	free(cmd->cmd);

	for (int i = 0; i < cmd->argc; i++) {
		free(cmd->argv[i]);
	}

	return;
}

struct command* get_cmd() {
	struct command* cmd = (struct command*) malloc(sizeof(struct command));
	if (cmd == NULL) return NULL;
	
	printf(": ");	
	char cmd_buf[2049];
	fflush(stdin);
	fgets(cmd_buf, 2049, stdin);
	
	cmd_buf[strlen(cmd_buf) - 1] = '\0';

	char* token, * saveptr;
	token = strtok_r(cmd_buf, " ", &saveptr);
	cmd->cmd = (char*) malloc(sizeof(char) * (strlen(token) + 1));
	strncpy(cmd->cmd, token, strlen(token) + 1);

	token = strtok_r(NULL, " ", &saveptr);
	while (token != NULL) {
		cmd->argv[cmd->argc] = (char*) malloc(sizeof(char) * (strlen(token) + 1));
		strncpy(cmd->argv[cmd->argc], token, strlen(token) + 1);
		cmd->argc++;

		token = strtok_r(NULL, " ", &saveptr);
	}

	return cmd;
}

int main(int argc, char** argv) {
	struct command* cmd;
	while(true) {
		cmd = get_cmd();

		printf("cmd: %s\targs: ", cmd->cmd);
		for (int i = 0; i < cmd->argc; i++) {
			printf("%s ", cmd->argv[i]);
		}
		printf("\n");

		if (strcmp(cmd->cmd, "exit") == 0) {
			break;
		} else {
			free_command(cmd);
		}
	}

	return EXIT_SUCCESS;
}

