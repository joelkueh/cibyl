
#include "cibyl.h"
#include "eval.h"
#include "uci.h"

#include <threads.h>
#include <unistd.h>

#define BOOLEAN_ARGS \
    BOOLEAN_ARG(help, "-h", "Show help")
#include "easy_args.h"

FILE *log_file;
mtx_t log_mtx;

void cibyl_vwrite_log(char *format, va_list args)
{
    if (log_file == NULL)
        return;
    if (mtx_lock(&log_mtx))
        return;
    vfprintf(log_file, format, args);
    mtx_unlock(&log_mtx);
}

void cibyl_write_log(char *format, ...)
{
    va_list args;
    va_start(args, format);
    cibyl_vwrite_log(format, args);
    va_end(args);
}

int main(int argc, char *argv[])
{
    int result = 0;
    uci_engine_t eng;
    args_t args = make_default_args();
    if (!parse_args(argc, argv, &args) || args.help) {
        print_help(argv[0]);
        result = 1;
        goto out;
    }

    /* Set the log file to stderr. */
    log_file = stderr;

    /* Initialize the engine. */
    if (uci_init(&eng)) {
        result = 1;
        goto out;
    };

    /* Initialize the engine and begin processing messages. */
    if (uci_process(&eng)) {
        result = 1;
        goto out_uci_cleanup;
    }

out_uci_cleanup:
    uci_deinit(&eng);
out:
    return result;
}
