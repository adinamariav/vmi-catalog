#define _GNU_SOURCE
#define STACK_FRAME_LIMIT 35

#include <libvmi/libvmi.h>
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>

volatile sig_atomic_t keep_running = 1;

typedef struct stack_frame {
    addr_t frame_pointer;
    addr_t return_address;
    char *line;
} stack_frame_t;

void setup_signal_handlers(void);
int setup_timer(unsigned int timeout);
void handle_timer(int sig, siginfo_t * si, void *uc);
void handle_sigint(int sig);
void show_usage(const char *progname);
void cleanup_resources(vmi_instance_t vmi, vmi_init_data_t * init_data,
                       char *outfile);
stack_frame_t *create_stack_frame(void);
void release_stack_frame(stack_frame_t * stack);
int gather_stack_trace(vmi_instance_t vmi, stack_frame_t * stack,
                       int vmem_enabled, int debug);

int
main(int argc, char *argv[])
{
    int opt;
    char *vm_name = NULL;
    char *socket_path = NULL;
    char *output_file = NULL;
    int vmem_enabled = 0;
    unsigned int interval = 10;
    unsigned int timeout = 0;
    char *output_format = "flamegraph";
    int debug = 0;

    while ((opt = getopt(argc, argv, "n:s:o:t:i:f:vdh")) != -1) {
        switch (opt) {
        case 'n':
            vm_name = optarg;
            break;
        case 's':
            socket_path = optarg;
            break;
        case 'o':
            output_file = strdup(optarg);
            if (!output_file) {
                perror("Memory allocation for output file failed");
                cleanup_resources(NULL, NULL, NULL);
                exit(EXIT_FAILURE);
            }
            break;
        case 'i':
            interval = atoi(optarg);
            break;
        case 'f':
            output_format = optarg;
            break;
        case 't':
            timeout = atoi(optarg);
            break;
        case 'v':
            vmem_enabled = 1;
            break;
        case 'd':
            debug = 1;
            break;
        case 'h':
        default:
            show_usage(argv[0]);
            cleanup_resources(NULL, NULL, output_file);
            exit(EXIT_FAILURE);
        }
    }

    if (!vm_name || !socket_path) {
        fprintf(stderr,
                "Error: Both -n (vm_name) and -s (socket) options are required.\n");
        show_usage(argv[0]);
        cleanup_resources(NULL, NULL, output_file);
        exit(EXIT_FAILURE);
    }

    if (!output_file) {
        output_file = strdup("out.trace");
        if (!output_file) {
            perror("Memory allocation for default output file failed");
            cleanup_resources(NULL, NULL, NULL);
            exit(EXIT_FAILURE);
        }
    }

    vmi_instance_t vmi = NULL;
    vmi_init_data_t *init_data =
        calloc(1, sizeof(vmi_init_data_t) + sizeof(vmi_init_data_entry_t));
    if (!init_data) {
        perror("Memory allocation for init_data failed");
        cleanup_resources(vmi, init_data, output_file);
        exit(EXIT_FAILURE);
    }

    init_data->count = 1;
    init_data->entry[0].type = VMI_INIT_DATA_KVMI_SOCKET;
    init_data->entry[0].data = strdup(socket_path);
    if (!init_data->entry[0].data) {
        perror("Memory allocation for socket data failed");
        cleanup_resources(vmi, init_data, output_file);
        exit(EXIT_FAILURE);
    }

    vmi_mode_t mode = 0;
    if (VMI_FAILURE ==
        vmi_get_access_mode(NULL, vm_name,
                            VMI_INIT_DOMAINNAME | VMI_INIT_EVENTS, init_data,
                            &mode)) {
        fprintf(stderr, "Failed to get access mode for VM: %s\n", vm_name);
        cleanup_resources(vmi, init_data, output_file);
        exit(EXIT_FAILURE);
    }

    if (VMI_FAILURE ==
        vmi_init_complete(&vmi, vm_name, VMI_INIT_DOMAINNAME, init_data,
                          VMI_CONFIG_GLOBAL_FILE_ENTRY, NULL, NULL)) {
        fprintf(stderr, "Failed to initialize LibVMI for VM: %s\n", vm_name);
        cleanup_resources(vmi, init_data, output_file);
        exit(EXIT_FAILURE);
    }

    setup_signal_handlers();

    if (timeout > 0 && setup_timer(timeout) != 0) {
        cleanup_resources(vmi, init_data, output_file);
        exit(EXIT_FAILURE);
    }

    FILE *output = fopen(output_file, "w");
    if (!output) {
        perror("Failed to open output file");
        cleanup_resources(vmi, init_data, output_file);
        exit(EXIT_FAILURE);
    }

    stack_frame_t *stack = create_stack_frame();
    if (!stack) {
        fprintf(stderr, "Failed to allocate memory for stack frames\n");
        cleanup_resources(vmi, init_data, output_file);
        fclose(output);
        exit(EXIT_FAILURE);
    }

    while (keep_running) {
        usleep(interval * 1000);

        if (vmi_pause_vm(vmi) != VMI_SUCCESS) {
            fprintf(stderr, "Failed to pause VM\n");
            release_stack_frame(stack);
            cleanup_resources(vmi, init_data, output_file);
            fclose(output);
            exit(EXIT_FAILURE);
        }

        if (gather_stack_trace(vmi, stack, vmem_enabled, debug) != 0) {
            fprintf(stderr,
                    "Failed to collect stack trace, resuming VM and retrying...\n");
            if (vmi_resume_vm(vmi) == VMI_FAILURE) {
                fprintf(stderr,
                        "Failed to resume VM after failed stack trace collection\n");
                release_stack_frame(stack);
                cleanup_resources(vmi, init_data, output_file);
                fclose(output);
                exit(EXIT_FAILURE);
            }
            continue;
        }

        if (strcmp(output_format, "human") == 0) {
            fprintf(output, "Stack trace:\n");
            for (int i = 0; i < STACK_FRAME_LIMIT && stack[i].line; i++) {
                fprintf(output, "%2d: %s (0x%lx)\n", i, stack[i].line,
                        stack[i].return_address);
            }
            fprintf(output, "\n");
        } else {
            for (int i = 0; i < STACK_FRAME_LIMIT && stack[i].line; i++) {
                if (i > 0) {
                    fprintf(output, ";");
                }
                fprintf(output, "%s", stack[i].line);
            }
            fprintf(output, " 1\n");
        }

        if (vmi_resume_vm(vmi) == VMI_FAILURE) {
            fprintf(stderr, "Failed to resume VM\n");
            release_stack_frame(stack);
            cleanup_resources(vmi, init_data, output_file);
            fclose(output);
            exit(EXIT_FAILURE);
        }
    }

    release_stack_frame(stack);
    fclose(output);
    cleanup_resources(vmi, init_data, output_file);
    return EXIT_SUCCESS;
}

void
setup_signal_handlers(void)
{
    struct sigaction sa_int = {.sa_handler = handle_sigint,.sa_flags = 0 };
    sigemptyset(&sa_int.sa_mask);

    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("Failed to set SIGINT handler");
        cleanup_resources(NULL, NULL, NULL);
        exit(EXIT_FAILURE);
    }
}

int
setup_timer(unsigned int timeout)
{
    timer_t timer;
    struct sigevent sev = {.sigev_notify = SIGEV_SIGNAL,.sigev_signo =
            SIGRTMIN };
    struct sigaction sa_timer = {.sa_sigaction = handle_timer,.sa_flags =
            SA_SIGINFO };
    struct itimerspec its = {.it_value.tv_sec = timeout };

    if (timer_create(CLOCK_REALTIME, &sev, &timer) != 0) {
        perror("Failed to create timer");
        return -1;
    }

    sigemptyset(&sa_timer.sa_mask);
    if (sigaction(SIGRTMIN, &sa_timer, NULL) == -1) {
        perror("Failed to set SIGRTMIN handler");
        timer_delete(timer);
        return -1;
    }

    if (timer_settime(timer, 0, &its, NULL) != 0) {
        perror("Failed to set timer");
        timer_delete(timer);
        return -1;
    }

    return 0;
}

void
handle_timer(int sig __attribute__((unused)), siginfo_t * si
             __attribute__((unused)), void *uc __attribute__((unused)))
{
    printf("Timeout reached. Ending stack tracing.\n");
    keep_running = 0;
}

void
handle_sigint(int sig __attribute__((unused)))
{
    printf("Interrupt signal received (Ctrl+C). Ending stack tracing.\n");
    keep_running = 0;
}

void
show_usage(const char *progname)
{
    fprintf(stderr,
            "Usage: %s [-n vm_name] [-s socket] [-o output_file] [-t timeout (s)] [-i interval (ms)] [-f format] [-v (vmem enabled)] [-d (debug mode)]\n",
            progname);
    fprintf(stderr, "Formats: human, flamegraph (default)\n");
}

void
cleanup_resources(vmi_instance_t vmi, vmi_init_data_t * init_data,
                  char *outfile)
{
    if (vmi) {
        vmi_destroy(vmi);
    }
    if (init_data) {
        free(init_data->entry[0].data);
        free(init_data);
    }
    free(outfile);
}

stack_frame_t *
create_stack_frame(void)
{
    stack_frame_t *stack = calloc(STACK_FRAME_LIMIT, sizeof(stack_frame_t));
    if (!stack) {
        perror("Failed to allocate memory for stack frames");
        return NULL;
    }
    return stack;
}

void
release_stack_frame(stack_frame_t * stack)
{
    if (stack) {
        for (int i = 0; i < STACK_FRAME_LIMIT; i++) {
            free(stack[i].line);
        }
        free(stack);
    }
}

int
gather_stack_trace(vmi_instance_t vmi, stack_frame_t * stack,
                   int vmem_enabled, int debug)
{
    registers_t regs;
    if (vmi_get_vcpuregs(vmi, &regs, 0) != VMI_SUCCESS) {
        fprintf(stderr, "Failed to get vCPU registers\n");
        return -1;
    }

    addr_t frame_pointer = regs.x86.rbp;
    addr_t return_address = 0;
    int index = 0;

    ACCESS_CONTEXT(ctx,.translate_mechanism = VMI_TM_PROCESS_PID,.pid = 0);

    while (frame_pointer && index < STACK_FRAME_LIMIT) {
        if (stack[index].line) {
            free(stack[index].line);
            stack[index].line = NULL;
        }

        stack[index].frame_pointer = frame_pointer;

        if (debug) {
            printf("Debug: Frame pointer at index %d: 0x%lx\n", index,
                   frame_pointer);
        }

        addr_t next_frame_pointer;
        if (vmem_enabled) {
            unsigned long paddr_vmi;
            if (vmi_translate_kv2p(vmi, frame_pointer, &paddr_vmi) !=
                VMI_SUCCESS) {
                fprintf(stderr,
                        "Failed to translate virtual address to physical address\n");
                return -1;
            }
            if (vmi_read_64_pa(vmi, paddr_vmi, &next_frame_pointer) !=
                VMI_SUCCESS) {
                fprintf(stderr,
                        "Failed to read memory at physical address: 0x%lx\n",
                        paddr_vmi);
                return -1;
            }
        } else {
            if (vmi_read_64_va(vmi, frame_pointer, 0, &next_frame_pointer) !=
                VMI_SUCCESS) {
                fprintf(stderr,
                        "Failed to read memory at virtual address: 0x%lx\n",
                        frame_pointer);
                return -1;
            }
        }

        if (next_frame_pointer == 0 || next_frame_pointer == frame_pointer) {
            if (debug) {
                printf
                    ("Debug: Invalid or cyclic frame pointer detected at index %d\n",
                     index);
            }
            break;
        }

        addr_t return_address_fp = frame_pointer + sizeof(addr_t);

        if (vmem_enabled) {
            unsigned long paddr_vmi;
            if (vmi_translate_kv2p(vmi, return_address_fp, &paddr_vmi) !=
                VMI_SUCCESS) {
                fprintf(stderr,
                        "Failed to translate return address to physical address\n");
                return -1;
            }
            if (vmi_read_64_pa(vmi, paddr_vmi, &return_address) !=
                VMI_SUCCESS) {
                fprintf(stderr,
                        "Failed to read return address from physical address: 0x%lx\n",
                        paddr_vmi);
                return -1;
            }
        } else {
            if (vmi_read_64_va(vmi, return_address_fp, 0, &return_address) !=
                VMI_SUCCESS) {
                fprintf(stderr,
                        "Failed to read return address from virtual address: 0x%lx\n",
                        return_address_fp);
                return -1;
            }
        }

        stack[index].return_address = return_address;

        const char *result =
            vmi_translate_v2ksym(vmi, &ctx, stack[index].return_address);
        stack[index].line = strdup(result ? result : "unknown");

        if (!stack[index].line) {
            fprintf(stderr, "Failed to allocate memory for stack line\n");
            return -1;
        }

        if (debug) {
            printf("Debug: Return address at index %d: 0x%lx (%s)\n", index,
                   return_address, stack[index].line);
        }

        frame_pointer = next_frame_pointer;
        index++;
    }

    return 0;
}
