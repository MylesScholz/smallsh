#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define FILE_NAME_MAX 255
#define PATH_LEN_MAX 4095

static volatile int sigint_flag = 0;

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

	free(cmd->i_file);
	free(cmd->o_file);

	free(cmd);
	return;
}

void print_command(struct command* cmd) {
	printf("\n");
	if (cmd == NULL) {
		printf("%p\n\n", cmd);
		return;
	}

	printf("cmd: %s\n", cmd->cmd);
	printf("argc: %d\nargv[]: ", cmd->argc);
	for (int i = 0; i < cmd->argc; i++) {
		printf("%s, ", cmd->argv[i]);
	}
	printf("\ni_file: %s\n", cmd->i_file);
	printf("o_file: %s\n", cmd->o_file);
	printf("background: %d\n\n", cmd->background);
}

char** expand_sh_vars (char** cmd_buf) {
	int len = sizeof(*cmd_buf) / sizeof(*cmd_buf[0]);
	char* buf_end = *cmd_buf + 2048;

	char* ptr = strstr(*cmd_buf, "$$");
	while (ptr != NULL) {	
		char temp[2049];
		strncpy(temp, ptr + 2, 2048);

		int curr_pid = getpid();
		int digits = ceil(log10(curr_pid));
		char digit_str[digits + 1];
		sprintf(digit_str, "%d", curr_pid);

		strncpy(ptr, digit_str, buf_end - ptr - 1);
		
		if (ptr + digits - 1 < buf_end) {
			strncpy(ptr + digits - 1, temp, buf_end - (ptr + digits - 1));
		}
		
		ptr = strstr(*cmd_buf, "$$");
	}

	return cmd_buf;
}

struct command* parse_cmd_args(struct command* cmd, char** cmd_buf) {
	char* token, * saveptr;
	
	token = strtok_r(*cmd_buf, " ", &saveptr);
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

	if (strcmp(cmd->argv[cmd->argc - 1], "&") == 0) {
		cmd->background = true;

		free(cmd->argv[cmd->argc - 1]);
		cmd->argv[cmd->argc - 1] = NULL;
		cmd->argc--;
	} else {
		cmd->background = false;
	}

	return cmd;
}

struct command* parse_cmd_io_files(struct command* cmd) {
	cmd->i_file = NULL;
	cmd->o_file = NULL;

	int args_removed = 0;
	for (int i = 1; i < cmd->argc - 1; i++) {
		char* arg = cmd->argv[i];

		if (strcmp(arg, "<") == 0) {
			int file_len = strlen(cmd->argv[i + 1]);
			if (file_len > FILE_NAME_MAX) file_len = FILE_NAME_MAX;

			cmd->i_file = (char*) malloc(sizeof(char) * (file_len + 1));
			strncpy(cmd->i_file, cmd->argv[i + 1], file_len + 1);
			
			free(cmd->argv[i]);
			cmd->argv[i] = NULL;
			free(cmd->argv[i + 1]);
			cmd->argv[i + 1] = NULL;
			i++;
			args_removed += 2;
		} else if (strcmp(arg, ">") == 0) {
			int file_len = strlen(cmd->argv[i + 1]);
			if (file_len > FILE_NAME_MAX) file_len = FILE_NAME_MAX;

			cmd->o_file = (char*) malloc(sizeof(char) * (file_len + 1));
			strncpy(cmd->o_file, cmd->argv[i + 1], file_len + 1);
			
			free(cmd->argv[i]);
			cmd->argv[i] = NULL;
			free(cmd->argv[i + 1]);
			cmd->argv[i + 1] = NULL;
			i++;
			args_removed += 2;
		}
	}
	
	if (cmd->background) {
		if (cmd->i_file == NULL) {
			cmd->i_file = (char*) malloc(sizeof(char) * 10);
			strncpy(cmd->i_file, "/dev/null", 10);
		}

		if (cmd->o_file == NULL) {
			cmd->o_file = (char*) malloc(sizeof(char) * 10);
			strncpy(cmd->o_file, "/dev/null", 10);
		}
	}

	cmd->argc -= args_removed;

	return cmd;
}

struct command* get_cmd() {
	struct command* cmd = (struct command*) malloc(sizeof(struct command));
	if (cmd == NULL) return NULL;
	
	printf(": ");
	fflush(stdout);
	
	char* cmd_buf = (char*) malloc(sizeof(char) * 2049);
	fgets(cmd_buf, 2049, stdin);
	cmd_buf[strlen(cmd_buf) - 1] = '\0';

	if (sigint_flag == 1 || strcmp(cmd_buf, "") == 0 || strcmp(cmd_buf, "\n") == 0) return NULL;

	expand_sh_vars(&cmd_buf);
	parse_cmd_args(cmd, &cmd_buf);
	parse_cmd_io_files(cmd);

	free(cmd_buf);
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

int redirect_io(struct command* cmd) {
	if (cmd->i_file != NULL) {
		int fd = open(cmd->i_file, O_RDONLY);
		if (fd == -1) {
			perror(cmd->i_file);
			return -1;
		}

		int err = dup2(fd, STDIN_FILENO);
		if (err == -1) {
			perror("Redirect");
			return -1;
		}
		close(fd);
	}

	if (cmd->o_file != NULL) {
		int fd = open(cmd->o_file, O_WRONLY | O_TRUNC | O_CREAT, 0770);
		if (fd == -1) {
			perror(cmd->o_file);
			return -1;
		}

		int err = dup2(fd, STDOUT_FILENO);
		if (err == -1) {
			perror("Redirect");
			return -1;
		}
		close(fd);
	}

	return 0;
}

void handle_SIGINT(int sig_num) {
	sigint_flag = 1;
	printf("\n");

	/*
	int stat, pid;
	pid = waitpid(-1, &stat, WNOHANG);
	if (pid > 0) {
		printf("\nChild exited with status %d.\n", stat);
		fflush(stdout);
		exit(EXIT_SUCCESS);
	}
	*/

	return;
}

int main(int argc, char** argv) {
	struct sigaction SIGINT_action = {0};
	SIGINT_action.sa_handler = &handle_SIGINT;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	struct command* cmd;
	int last_exit_status = 0;

	while(true) {
		sigint_flag = 0;

		int child_status, pid;
		pid = waitpid(-1, &child_status, WNOHANG);
		if (pid > 0) {
			printf("Background process (pid = %d) done. Exited with status %d.\n", pid, child_status);
			fflush(stdout);
			last_exit_status = child_status;
		}

		cmd = get_cmd();
		if (cmd == NULL || sigint_flag == 1) continue;
		
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
		int err;
		
		spawn_pid = fork();
		switch (spawn_pid) {
			case -1:
				perror("");
				exit(1);
				break;
			case 0:	
				err = redirect_io(cmd);
				if (err == -1) {
					return EXIT_FAILURE;
				}

				execvp(cmd->cmd, cmd->argv);

				return EXIT_FAILURE;
				break;
			default:
				if (cmd->background) {
					printf("Background pid = %d\n", spawn_pid);
					fflush(stdout);
				} else {
					waitpid(spawn_pid, &child_status, 0);
					last_exit_status = WEXITSTATUS(child_status);
				}
				break;
		}

		free_command(cmd);
	}

	return EXIT_SUCCESS;
}

