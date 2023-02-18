#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>

#define FILE_NAME_MAX 255
#define PATH_LEN_MAX 4095

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
	cmd->argv[0] = (char*) malloc(sizeof(char) * (strlen(cmd->cmd) + 1));
	strncpy(cmd->argv[0], cmd->cmd, strlen(cmd->cmd) + 1);

	token = strtok_r(NULL, " ", &saveptr);
	cmd->argc = 1;
	while (token != NULL) {
		cmd->argv[cmd->argc] = (char*) malloc(sizeof(char) * (strlen(token) + 1));
		strncpy(cmd->argv[cmd->argc], token, strlen(token) + 1);
		cmd->argc++;

		token = strtok_r(NULL, " ", &saveptr);
	}

	for (int i = 1; i < cmd->argc - 1; i++) {
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

	if (strcmp(cmd->argv[cmd->argc - 1], "&") == 0) {
		cmd->background = true;
	}

	cmd->argv[cmd->argc] = NULL;

	return cmd;
}

int cd(struct command* cmd) {
	if (cmd == NULL) return -1;

	int err;
	if (cmd->argc > 1) {
		err = chdir(cmd->argv[1]);
	} else {
		err = chdir(getenv("HOME"));
	}
	
	if (err == -1) {
		perror("");
		return -1;
	}

	return 0;
}

int status(int last_exit_status) {
	printf("Exit status %d\n", last_exit_status);
	return 0;
}

int main(int argc, char** argv) {
	struct command* cmd;
	int last_exit_status = 0;

	while(true) {
		cmd = get_cmd();
		if (cmd == NULL) continue;
		
		if (strcmp(cmd->cmd, "exit") == 0) {
			// TODO: kill all processes
			free_command(cmd);
			break;
		}

		if (strcmp(cmd->cmd, "#") == 0) {
			free_command(cmd);
			continue;
		}

		if (strcmp(cmd->cmd, "cd") == 0) {
			cd(cmd);
			free_command(cmd);
			continue;
		}

		if (strcmp(cmd->cmd, "status") == 0) {
			status(last_exit_status);
			free_command(cmd);
			continue;
		}

		pid_t spawn_pid = -5;
		int child_pid, child_status;
		
		spawn_pid = fork();
		switch (spawn_pid) {
			case -1:
				perror("");
				exit(1);
				break;
			case 0:
				execvp(cmd->cmd, cmd->argv);

				return EXIT_FAILURE;
				break;
			default:
				// TODO: handle background processes
				child_pid = waitpid(spawn_pid, &child_status, 0);
				
				last_exit_status = WEXITSTATUS(child_status);
				break;
		}

		free_command(cmd);
	}

	return EXIT_SUCCESS;
}

