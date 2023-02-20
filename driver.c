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

static volatile int last_exit_status = 0;

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

// int_to_str
// Converts an integer into a string
// Parameters: the integer to be converted and a buffer to receive the output; the buffer is assumed to be large enough for the number
// Returns: the string form of the integer
char* int_to_str(int n, char* buffer) {
	// i is the index of the first digit of the number
	int i = 0;
	// If the integer is negative, add a negative sign and step i to the next index
	if (n < 0) buffer[i++] = "-";
	
	// j is the index of the last digit of the number; default j to zero
	int j = 0;
	// To avoid domain errors, check that the integer is greater than zero before calculating the number of digits in it
	if (n > 0) j = (int) log10(n) + i;
	
	// Add a null-terminator to the string
	buffer[j] = "\0";

	// Loop through each digit of the integer from the right to left
	while (j >= i) {
		// Get the rightmost digit
		int digit = n % 10;
		// Increment the integer to the next digit
		n /= 10;
		// Convert the current digit into a character and add it to the string
		buffer[j--] = (char) (digit + 48);
	}

	// Return the string of the number
	return buffer;
}

char** expand_sh_vars (char** cmd_buf) {
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

	if (strcmp(cmd_buf, "") == 0) return NULL;

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

void SIGCHLD_handler(int sig_num) {
	int stat, pid;
	pid = waitpid(-1, &stat, 0);
	
	if (pid > 0 && stat != SIGINT) {
		char pid_buf[50];
		int_to_str(pid, pid_buf);
		char stat_buf[10];
		int_to_str(stat, stat_buf);

		write(STDOUT_FILENO, "Background process (pid = ", 26);
		write(STDOUT_FILENO, pid_buf, 50);
		write(STDOUT_FILENO, ") done. Exited with status ", 27);
		write(STDOUT_FILENO, stat_buf, 10);
		write(STDOUT_FILENO, ".\n", 2);
	}
	
	last_exit_status = stat;
	return;
}

void SIGINT_handler(int sig_num) {
	write(STDIN_FILENO, "\nCaught.\n", 9);
	last_exit_status = sig_num;
	exit(sig_num);
	return;
}

int main(int argc, char** argv) {
	struct command* cmd;

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

		struct sigaction SIGINT_action = {0};
		if (cmd->background == true) {
			SIGINT_action.sa_handler = SIG_IGN;
		} else {
			SIGINT_action.sa_handler = &SIGINT_handler;
		}
		sigfillset(&SIGINT_action.sa_mask);
		SIGINT_action.sa_flags = SA_RESTART;
		sigaction(SIGINT, &SIGINT_action, NULL);

		struct sigaction SIGCHLD_action = {0};
		SIGCHLD_action.sa_handler = &SIGCHLD_handler;
		sigfillset(&SIGCHLD_action.sa_mask);
		SIGCHLD_action.sa_flags = SA_RESTART;
		sigaction(SIGCHLD, &SIGCHLD_action, NULL);
	
		pid_t spawn_pid = -5;
		int child_status, err;
		
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

				perror(cmd->cmd);
				return EXIT_FAILURE;
				break;
			default:
				if (cmd->background) {
					printf("Background process (pid = %d) created.\n", spawn_pid);
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

