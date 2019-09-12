// This program wraps the Minecraft server (executing it as a subprocess and piping STDIN / STDOUT/ STDERR) since the
// Spigot server doesn't save correctly upon receipt of SIGINT / SIGTERM (the wrapper injects a `/stop` command)

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

#define JAVA_CMD "java"
#define MIN_MEM "512M"

#define MAX_MEM_ENV "MAX_MEMORY"
#define MAX_MEM_DEFAULT "2048M"

#define BUF_SIZE 4096

int stdin_pipe[2];
int stdout_pipe[2];
int stderr_pipe[2];


int exec_server(int argc, char **argv) {
    // change the process group so we minecraft doesn't receive SIGINT / SIGTERM
    setpgid(0, 0);

    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    if (dup2(stdin_pipe[0], STDIN_FILENO) == -1 ||
        dup2(stdout_pipe[1], STDOUT_FILENO) == -1 ||
        dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
        perror("dup2()");
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    char *max_mem = getenv(MAX_MEM_ENV);
    if (!max_mem) {
        max_mem = MAX_MEM_DEFAULT;
    }
    char max_mem_arg[256];
    snprintf(max_mem_arg, 256, "-Xmx%s", max_mem);

    char **args = malloc((4 + argc + 1) * sizeof(char*));
    args[0] = JAVA_CMD;
    args[1] = "-Xms" MIN_MEM;
    args[2] = max_mem_arg;
    for (int i = 2; i < argc; i++) {
        args[i + 1] = argv[i];
    }
    args[1 + argc] = "-jar";
    args[1 + argc + 1] = argv[1];
    args[1 + argc + 2] = "nogui";
    args[1 + argc + 3] = NULL;

    /*for (int i = 0; i < 4 + argc; i++) {
        printf("***** arg: %s\n", args[i]);
    }
    fflush(stdout);*/

    if (execvp(JAVA_CMD, args) == -1) {
        perror("execvp()");
        return -3;
    }
}

static int do_pipe(int in_fd, int out_fd, char *buf) {
    ssize_t bytes_read = read(in_fd, buf, BUF_SIZE);
    if (bytes_read == -1) {
        perror("read()");
        return -1;
    } else if (bytes_read == 0) {
        // EOF
        return 1;
    }

    if (write(out_fd, buf, bytes_read) != bytes_read) {
        perror("write()");
        return -1;
    }

    return 0;
}

#define DO_PIPE(p, i, o)\
if (pfds[p].revents & POLLIN) {\
    switch(do_pipe(i, o, buf)) {\
    case -1:\
        return -8;\
    case 1:\
        close(i);\
        close(o);\
        break;\
    case 0:\
        break;\
    }\
}
int monitor() {
    // we're going to use a signalfd to handle signals
    sigset_t blockset;
    sigemptyset(&blockset);
    sigaddset(&blockset, SIGINT);
    sigaddset(&blockset, SIGTERM);
    sigaddset(&blockset, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &blockset, NULL) != 0) {
        perror("sigprocmask()");
        return -4;
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    int exit_fd = signalfd(-1, &blockset, 0);
    if (exit_fd == -1) {
        perror("signalfd()");
        return -5;
    }

    struct pollfd pfds[4];
    pfds[0].fd = exit_fd;
    pfds[0].events = POLLIN;
    pfds[1].fd = STDIN_FILENO;
    pfds[1].events = POLLIN;
    pfds[2].fd = stdout_pipe[0];
    pfds[2].events = POLLIN;
    pfds[3].fd = stderr_pipe[0];
    pfds[3].events = POLLIN;

    struct signalfd_siginfo si;
    int server_status;
    char buf[4096];
    while (true) {
        int ready = poll((struct pollfd *)&pfds, 4, -1);
        if (ready == -1) {
            perror("poll()");
            return -6;
        } else if (ready == 0) {
            continue;
        } else if (pfds[0].revents & POLLIN) {
            ssize_t size = read(exit_fd, &si, sizeof(struct signalfd_siginfo));
            if (size == -1) {
                perror("read()");
                return -7;
            } else if (size != sizeof(struct signalfd_siginfo)) {
                fprintf(stderr, "sizeof(struct signalfd_siginfo): %ld != %ld", size, sizeof(struct signalfd_siginfo));
                return -7;
            }

            switch (si.ssi_signo) {
            case SIGINT:
            case SIGTERM:
                puts("[wrapper] doing /stop for clean shutdown");
                dprintf(stdin_pipe[1], "stop\n");
                break;
            case SIGCHLD:
                if (waitpid(-1, &server_status, WNOHANG) == -1) {
                    perror("waitpid()");
                    return -9;
                }

                if (WIFEXITED(server_status)) {
                    return WEXITSTATUS(server_status);
                } else if (WIFSIGNALED(server_status)) {
                    fprintf(stderr, "minecraft server terminated by signal: %d\n", server_status);
                    return -WTERMSIG(server_status);
                }
                return 0;
            }
        } else {
            DO_PIPE(1, STDIN_FILENO, stdin_pipe[1])
            DO_PIPE(2, stdout_pipe[0], STDOUT_FILENO)
            DO_PIPE(3, stderr_pipe[0], STDERR_FILENO)
        }
    }
}
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <minecraft server jar file> <java additional params>\n", argv[0]);
        return 1;
    }

    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        perror("pipe()");
        return -1;
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork()");
        return -2;
    } else if (child_pid == 0) {
        // child
        _exit(exec_server(argc, argv));
    } else {
        // parent
        int result = monitor();
        if (result != 0) {
            kill(child_pid, SIGKILL);
        }
        return result;
    }
}
