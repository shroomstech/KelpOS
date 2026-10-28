#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_SERVICES 64
#define MAX_ARGUMENTS 64
#define MAX_DEPENDENCIES 64
#define MAX_LINE_LENGTH 4096

enum service_mode {
    SERVICE_SIMPLE,
    SERVICE_ONESHOT,
};

enum restart_policy {
    RESTART_NEVER,
    RESTART_ON_FAILURE,
    RESTART_ALWAYS,
};

struct service {
    char *name;
    char *arguments[MAX_ARGUMENTS + 1];
    size_t argument_count;
    char *dependencies[MAX_DEPENDENCIES];
    size_t dependency_count;
    enum service_mode mode;
    enum restart_policy restart;
    unsigned int max_restarts;
    unsigned int restart_count;
    double restart_delay;
    pid_t pid;
    bool active;
    unsigned int visit_state;
};

static struct service services[MAX_SERVICES];
static size_t service_count;
static struct service *start_order[MAX_SERVICES];
static size_t start_count;
static double shutdown_timeout = 5.0;
static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t received_signal;

static void request_stop(int signal_number)
{
    received_signal = signal_number;
    stop_requested = 1;
}

static void report_error(const char *path, size_t line_number, const char *message)
{
    if (line_number == 0) {
        fprintf(stderr, "ciswn: %s: %s\n", path, message);
    } else {
        fprintf(stderr, "ciswn: %s:%zu: %s\n", path, line_number, message);
    }
}

static bool parse_unsigned(const char *text, unsigned int maximum, unsigned int *result)
{
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > maximum) {
        return false;
    }
    *result = (unsigned int)value;
    return true;
}

static bool parse_seconds(const char *text, double maximum, double *result)
{
    char *end = NULL;
    errno = 0;
    double value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(value)
            || value < 0.0 || value > maximum) {
        return false;
    }
    *result = value;
    return true;
}

static struct service *find_service(const char *name)
{
    for (size_t index = 0; index < service_count; index++) {
        if (strcmp(services[index].name, name) == 0) {
            return &services[index];
        }
    }
    return NULL;
}

static bool add_service(char **tokens, size_t token_count)
{
    if (token_count < 7 || token_count > MAX_ARGUMENTS + 6) {
        return false;
    }
    if (service_count == MAX_SERVICES || find_service(tokens[1]) != NULL) {
        return false;
    }

    struct service *service = &services[service_count];
    if (strcmp(tokens[2], "simple") == 0) {
        service->mode = SERVICE_SIMPLE;
    } else if (strcmp(tokens[2], "oneshot") == 0) {
        service->mode = SERVICE_ONESHOT;
    } else {
        return false;
    }

    if (strcmp(tokens[3], "never") == 0) {
        service->restart = RESTART_NEVER;
    } else if (strcmp(tokens[3], "on-failure") == 0) {
        service->restart = RESTART_ON_FAILURE;
    } else if (strcmp(tokens[3], "always") == 0) {
        service->restart = RESTART_ALWAYS;
    } else {
        return false;
    }
    if (service->mode == SERVICE_ONESHOT && service->restart != RESTART_NEVER) {
        return false;
    }

    if (!parse_unsigned(tokens[4], 100, &service->max_restarts)
            || !parse_seconds(tokens[5], 60.0, &service->restart_delay)) {
        return false;
    }

    service->name = strdup(tokens[1]);
    if (service->name == NULL) {
        return false;
    }
    for (size_t index = 6; index < token_count; index++) {
        service->arguments[service->argument_count] = strdup(tokens[index]);
        if (service->arguments[service->argument_count] == NULL) {
            return false;
        }
        service->argument_count++;
    }
    service->arguments[service->argument_count] = NULL;
    service_count++;
    return true;
}

static bool add_dependency(const char *service_name, const char *dependency_name)
{
    struct service *service = find_service(service_name);
    if (service == NULL || service->dependency_count == MAX_DEPENDENCIES
            || strcmp(service_name, dependency_name) == 0) {
        return false;
    }
    for (size_t index = 0; index < service->dependency_count; index++) {
        if (strcmp(service->dependencies[index], dependency_name) == 0) {
            return false;
        }
    }
    service->dependencies[service->dependency_count] = strdup(dependency_name);
    if (service->dependencies[service->dependency_count] == NULL) {
        return false;
    }
    service->dependency_count++;
    return true;
}

static bool parse_config(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "ciswn: cannot open %s: %s\n", path, strerror(errno));
        return false;
    }

    char *line = NULL;
    size_t capacity = 0;
    size_t line_number = 0;
    bool valid = true;
    while (getline(&line, &capacity, file) != -1) {
        line_number++;
        if (strlen(line) > MAX_LINE_LENGTH) {
            report_error(path, line_number, "line is too long");
            valid = false;
            break;
        }
        char *comment = strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }

        char *tokens[MAX_ARGUMENTS + 7];
        size_t token_count = 0;
        char *save_pointer = NULL;
        for (char *token = strtok_r(line, " \t\r\n", &save_pointer);
                token != NULL;
                token = strtok_r(NULL, " \t\r\n", &save_pointer)) {
            if (token_count == sizeof(tokens) / sizeof(tokens[0])) {
                report_error(path, line_number, "too many fields");
                valid = false;
                break;
            }
            tokens[token_count++] = token;
        }
        if (!valid || token_count == 0) {
            if (!valid) {
                break;
            }
            continue;
        }

        if (strcmp(tokens[0], "shutdown_timeout") == 0) {
            if (token_count != 2 || !parse_seconds(tokens[1], 60.0, &shutdown_timeout)) {
                report_error(path, line_number, "shutdown_timeout expects one number from 0 to 60");
                valid = false;
                break;
            }
        } else if (strcmp(tokens[0], "service") == 0) {
            if (!add_service(tokens, token_count)) {
                report_error(path, line_number, "invalid or duplicate service definition");
                valid = false;
                break;
            }
        } else if (strcmp(tokens[0], "after") == 0) {
            if (token_count != 3 || !add_dependency(tokens[1], tokens[2])) {
                report_error(path, line_number, "after expects an existing service and one distinct dependency");
                valid = false;
                break;
            }
        } else {
            report_error(path, line_number, "unknown directive");
            valid = false;
            break;
        }
    }
    if (ferror(file)) {
        report_error(path, line_number, "error reading configuration");
        valid = false;
    }
    free(line);
    fclose(file);

    if (valid && service_count == 0) {
        report_error(path, 0, "configuration must define at least one service");
        valid = false;
    }
    return valid;
}

static bool visit_service(struct service *service)
{
    if (service->visit_state == 2) {
        return true;
    }
    if (service->visit_state == 1) {
        fprintf(stderr, "ciswn: service dependency cycle includes %s\n", service->name);
        return false;
    }
    service->visit_state = 1;
    for (size_t index = 0; index < service->dependency_count; index++) {
        struct service *dependency = find_service(service->dependencies[index]);
        if (dependency == NULL) {
            fprintf(stderr, "ciswn: service %s has unknown dependency %s\n",
                    service->name, service->dependencies[index]);
            return false;
        }
        if (!visit_service(dependency)) {
            return false;
        }
    }
    service->visit_state = 2;
    start_order[start_count++] = service;
    return true;
}

static bool build_start_order(void)
{
    for (size_t index = 0; index < service_count; index++) {
        if (!visit_service(&services[index])) {
            return false;
        }
    }
    return true;
}

static int spawn_service(struct service *service)
{
    pid_t child = fork();
    if (child < 0) {
        fprintf(stderr, "ciswn: cannot fork service %s: %s\n", service->name, strerror(errno));
        return -1;
    }
    if (child == 0) {
        (void)setpgid(0, 0);
        execvp(service->arguments[0], service->arguments);
        fprintf(stderr, "ciswn: cannot execute %s: %s\n", service->arguments[0], strerror(errno));
        _exit(127);
    }
    (void)setpgid(child, child);
    service->pid = child;
    service->active = true;
    printf("CISWN: started %s (pid %ld)\n", service->name, (long)child);
    fflush(stdout);
    return 0;
}

static bool wait_for_oneshot(struct service *service)
{
    if (spawn_service(service) != 0) {
        return false;
    }
    int status;
    pid_t result;
    do {
        result = waitpid(service->pid, &status, 0);
    } while (result < 0 && errno == EINTR && !stop_requested);

    service->active = result != service->pid;
    if (stop_requested) {
        return true;
    }
    if (result < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "ciswn: oneshot service %s failed\n", service->name);
        return false;
    }
    printf("CISWN: completed %s\n", service->name);
    fflush(stdout);
    return true;
}

static bool should_restart(const struct service *service, int status)
{
    if (service->restart == RESTART_ALWAYS) {
        return true;
    }
    return service->restart == RESTART_ON_FAILURE
        && (!WIFEXITED(status) || WEXITSTATUS(status) != 0);
}

static void delay_seconds(double seconds)
{
    struct timespec remaining;
    remaining.tv_sec = (time_t)seconds;
    remaining.tv_nsec = (long)((seconds - (double)remaining.tv_sec) * 1000000000.0);
    while (!stop_requested && nanosleep(&remaining, &remaining) < 0 && errno == EINTR) {
    }
}

static void signal_service(struct service *service, int signal_number)
{
    if (service->active && kill(-service->pid, signal_number) < 0 && errno != ESRCH) {
        fprintf(stderr, "ciswn: cannot signal service %s: %s\n", service->name, strerror(errno));
    }
}

static void stop_services(void)
{
    for (size_t index = start_count; index > 0; index--) {
        signal_service(start_order[index - 1], SIGTERM);
    }

    double waited = 0.0;
    while (waited < shutdown_timeout) {
        bool any_active = false;
        for (size_t index = 0; index < service_count; index++) {
            struct service *service = &services[index];
            if (!service->active) {
                continue;
            }
            int status;
            pid_t result = waitpid(service->pid, &status, WNOHANG);
            if (result == service->pid || (result < 0 && errno == ECHILD)) {
                service->active = false;
            } else {
                any_active = true;
            }
        }
        if (!any_active) {
            return;
        }
        struct timespec interval = { .tv_sec = 0, .tv_nsec = 50000000 };
        while (nanosleep(&interval, &interval) < 0 && errno == EINTR) {
        }
        waited += 0.05;
    }

    for (size_t index = start_count; index > 0; index--) {
        signal_service(start_order[index - 1], SIGKILL);
    }
    for (size_t index = 0; index < service_count; index++) {
        struct service *service = &services[index];
        if (service->active) {
            while (waitpid(service->pid, NULL, 0) < 0 && errno == EINTR) {
            }
            service->active = false;
        }
    }
}

static int supervise(void)
{
    bool failed = false;
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 || sigaction(SIGTERM, &action, NULL) < 0) {
        fprintf(stderr, "ciswn: cannot install signal handlers: %s\n", strerror(errno));
        return 1;
    }

    for (size_t index = 0; index < start_count && !stop_requested; index++) {
        struct service *service = start_order[index];
        if (service->mode == SERVICE_ONESHOT) {
            if (!wait_for_oneshot(service)) {
                stop_services();
                return 1;
            }
        } else if (spawn_service(service) != 0) {
            stop_services();
            return 1;
        }
    }

    while (!stop_requested) {
        bool any_active = false;
        for (size_t index = 0; index < service_count; index++) {
            struct service *service = &services[index];
            if (!service->active) {
                continue;
            }
            any_active = true;
            int status;
            pid_t result = waitpid(service->pid, &status, WNOHANG);
            if (result == 0) {
                continue;
            }
            if (result < 0 && errno == EINTR) {
                continue;
            }
            service->active = false;
            if (result < 0) {
                fprintf(stderr, "ciswn: waitpid failed for %s: %s\n", service->name, strerror(errno));
                stop_requested = 1;
                failed = true;
                break;
            }
            printf("CISWN: service %s exited\n", service->name);
            fflush(stdout);
            if (should_restart(service, status)) {
                if (service->restart_count >= service->max_restarts) {
                    fprintf(stderr, "ciswn: service %s exceeded its restart limit\n", service->name);
                    stop_requested = 1;
                    failed = true;
                    break;
                }
                service->restart_count++;
                delay_seconds(service->restart_delay);
                if (!stop_requested && spawn_service(service) != 0) {
                    stop_requested = 1;
                    failed = true;
                    break;
                }
            }
        }
        if (!any_active) {
            break;
        }
        delay_seconds(0.05);
    }

    stop_services();
    if (received_signal != 0) {
        return 128 + received_signal;
    }
    return failed ? 1 : 0;
}

static void free_config(void)
{
    for (size_t index = 0; index < service_count; index++) {
        struct service *service = &services[index];
        free(service->name);
        for (size_t argument = 0; argument < service->argument_count; argument++) {
            free(service->arguments[argument]);
        }
        for (size_t dependency = 0; dependency < service->dependency_count; dependency++) {
            free(service->dependencies[dependency]);
        }
    }
}

int main(int argc, char **argv)
{
    bool check_only = false;
    const char *config_path = NULL;
    if (argc == 3 && strcmp(argv[1], "--check") == 0) {
        check_only = true;
        config_path = argv[2];
    } else if (argc == 2) {
        config_path = argv[1];
    } else {
        fprintf(stderr, "usage: ciswn [--check] CONFIG\n");
        return 2;
    }

    if (!parse_config(config_path) || !build_start_order()) {
        free_config();
        return 2;
    }

    if (check_only) {
        printf("CISWN: configuration valid; start order:");
        for (size_t index = 0; index < start_count; index++) {
            printf(" %s", start_order[index]->name);
        }
        putchar('\n');
        free_config();
        return 0;
    }

    int result = supervise();
    free_config();
    return result;
}