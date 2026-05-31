
#include "cibyl.h"
#include "eval.h"
#include "uci.h"

#include <pthread.h>
#include <unistd.h>

#define BOOLEAN_ARGS \
    BOOLEAN_ARG(help, "-h", "Show help")
#include "utils/easy_args.h"

FILE *log_file;
pthread_mutex_t log_lock;

int main(int argc, char *argv[])
{
    cibyl_error_t err;
    int result = 0;
    uci_engine_t eng;
    args_t args = make_default_args();

    /* Set the log file to stderr. */
    log_file = NULL;
    pthread_mutex_init(&log_lock, NULL);

    /* Parse arguments. */
    if (!parse_args(argc, argv, &args) || args.help) {
        print_help(argv[0]);
        result = 1;
        goto out;
    }

    /* Initialize the engine. */
    if (uci_init(&err, &eng)) {
        CIBYL_WRITE_ERR(&err);
        result = 1;
        goto out;
    };

    /* Initialize the engine and begin processing messages. */
    if (uci_process(&err, &eng)) {
        CIBYL_WRITE_ERR(&err);
        result = 1;
        goto out_uci_cleanup;
    }

out_uci_cleanup:
    uci_deinit(&eng);
out:
    return result;
}
