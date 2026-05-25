#include "vecu.h"
#include "uds.h"
#include "can_io.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/can.h>

/* Set by SIGINT/SIGTERM handlers to break out of the receive loop cleanly. */
static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static void print_frame(const struct can_frame *f) {
    /* Standard 11-bit ID for now; extended IDs will come with ISO-TP work. */
    unsigned can_id = (unsigned)(f->can_id & CAN_SFF_MASK);
    unsigned dlc    = (unsigned)f->can_dlc;

    printf("RX  id=0x%03X  dlc=%u  data=", can_id, dlc);
    for (unsigned i = 0; i < dlc; ++i) {
        printf("%02X", f->data[i]);
        if (i + 1u < dlc) {
            printf(" ");
        }
    }
    printf("\n");
}

int main(int argc, char **argv) {
    const char *ifname = (argc > 1) ? argv[1] : "vcan0";

    vecu_state_t state;
    vecu_init(&state);

    can_io_t io;
    if (can_io_open(&io, ifname) != 0) {
        fprintf(stderr,
                "Failed to open CAN interface '%s'.\n"
                "Hint: run `./scripts/setup_vcan.sh %s` first.\n",
                ifname, ifname);
        return EXIT_FAILURE;
    }

    /* Graceful shutdown on Ctrl+C / kill. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("vecu-uds v0.1.0 — listening on %s, session=0x%02X (Ctrl+C to stop)\n",
           ifname, state.session);
    fflush(stdout);

    struct can_frame frame;
    while (!g_stop) {
        int rc = can_io_recv(&io, &frame);
        if (rc == 1) {
            continue;  /* interrupted by signal */
        }
        if (rc != 0) {
            fprintf(stderr, "can_io_recv failed, exiting.\n");
            break;
        }
        print_frame(&frame);
        fflush(stdout);
    }

    printf("\nShutting down.\n");
    can_io_close(&io);
    return EXIT_SUCCESS;
}
