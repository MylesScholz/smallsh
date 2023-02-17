#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define FILE_NAME_MAX 255

struct command {
	char* cmd;
	int argc;
	char* argv[512];
	char* i_file, * o_file;
	bool background;
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

	char* buffer = NULL;
	size_t buffer_size = 0;
	getline(&buffer, &buffer_size, stdin);
	
	if (strlen(buffer) == 1) return NULL;

	strncpy(cmd_buf, buffer, 2048);

	cmd_buf[strlen(cmd_buf) - 1] = '\0';
	
	char* token, * saveptr;
	token = strtok_r(cmd_buf, " ", &saveptr);
	cmd->cmd = (char*) malloc(sizeof(char) * (strlen(token) + 1));
	strncpy(cmd->cmd, token, strlen(token) + 1);

	token = strtok_r(NULL, " ", &saveptr);
	cmd->argc = 0;
	while (token != NULL) {
		cmd->argv[cmd->argc] = (char*) malloc(sizeof(char) * (strlen(token) + 1));
		strncpy(cmd->argv[cmd->argc], token, strlen(token) + 1);
		cmd->argc++;

		token = strtok_r(NULL, " ", &saveptr);
	}

	for (int i = 0; i < cmd->argc - 1; i++) {
		char* arg = cmd->argv[i];
		if (cmd->i_file == NULL && strcmp(arg, "<") == 0) {
			int file_len = strlen(cmd->argv[i + 1]);
			if (file_len > FILE_NAME_MAX) file_len = FILE_NAME_MAX;

			cmd->i_file = (char*) malloc(sizeof(char) * (file_len + 1));
			strncpy(cmd->i_file, cmd->argv[i + 1], file_len + 1);
		} else if (cmd->o_file == NULL && strcmp(arg, ">") == 0) {
			int file_len = strlen(cmd->argv[i + 1]);
			if (file_len > FILE_NAME_MAX) file_len = FILE_NAME_MAX;

			cmd->o_file = (char*) malloc(sizeof(char) * (file_len + 1));
			strncpy(cmd->o_file, cmd->argv[i + 1], file_len + 1);
		}
	}

	if (cmd->argc >= 1 && strcmp(cmd->argv[cmd->argc - 1], "&") == 0) {
		cmd->background = true;
	}

	return cmd;
}

int main(int argc, char** argv) {
	struct command* cmd;
	while(true) {
		cmd = get_cmd();
		if (cmd == NULL) continue;

		printf("cmd: %s\targs: ", cmd->cmd);
		for (int i = 0; i < cmd->argc; i++) {
			printf("%s, ", cmd->argv[i]);
		}
		printf("\ni_file: %s\to_file: %s\tbackground: %d\n", cmd->i_file, cmd->o_file, cmd->background);

		if (strcmp(cmd->cmd, "#") == 0) {
			free_command(cmd);
			continue;
		}
	
		if (strcmp(cmd->cmd, "exit") == 0) {
			break;
		} else {
			free_command(cmd);
		}
	}

	return EXIT_SUCCESS;
}

