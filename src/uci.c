
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "cb/cb.h"
#include "uci.h"
#include "log.h"

const size_t MAX_COMMAND_LEN = 4096;

/* Command strings for matching. */
const char STR_UCI[] = "uci";
const char STR_DEBUG[] = "debug";
const char STR_ISREADY[] = "isready";
const char STR_SETOPTION[] = "setoption";
const char STR_REGISTER[] = "register";
const char STR_UCINEWGAME[] = "ucinewgame";
const char STR_POSITION[] = "position";
const char STR_GO[] = "go";
const char STR_STOP[] = "stop";
const char STR_PONDERHIT[] = "ponderhit";
const char STR_QUIT[] = "quit";
const char STR_UCI_DELIMS[] = " \t";

/* Supported option strings. */
const char STR_OPT_HASH[] = "Hash";
const char DECL_OPT_HASH[] = "option name Hash type spin default 16 min 1 max 32768\n";
const char STR_OPT_THREADS[] = "Threads";
const char DECL_OPT_THREADS[] = "option name Threads type spin default 1 min 1 max 16\n";
const char STR_OPT_CLEAR[] = "Clear Hash";
const char DECL_OPT_CLEAR[] = "option name Clear Hash type button";

/* Could suppor these later although it would be difficult. */
// const char STR_OPT_MULITPV[] = "MultiPV";
// const char STR_OPT_PONDER[] = "Ponder";
// const char STR_OPT_SYZYGY_PATH[] = "SyzygyPath";
// const char STR_OPT_SYZYGY_PROBE_DEPTH[] = "SyzygyProbeDepth";

/* Non-uci command strings. */
const char STR_DISPLAY[] = "d";
const char STR_EVAL[] = "eval";

/* Engine and author information. */
const char ENGINE_NAME[] = "Cibyl";
const char ENGINE_AUTHOR[] = "Joel Kuehne";

cibyl_errno_t uci_report_error(engine_t *eng, void *udata)
{
    /* TODO: Implement me. */
    printf("Error!\n");
    return CIBYL_EOK;
}

cibyl_errno_t uci_report_bestmove(engine_t *eng, void *udata)
{
    char buf[6];
    cb_mv_to_uci_algbr(buf, eng->bestmove);
    printf("bestmove %s\n", buf);
    return CIBYL_EOK;
}

cibyl_errno_t uci_report_info(engine_t *eng, void *udata)
{
    /* TODO: Implement me. */
    printf("Info!\n");
    return CIBYL_EOK;
}

cibyl_errno_t handle_position(cibyl_error_t *err, uci_engine_t *eng, char *opts)
{
    return eng_set_ucifen(err, &eng->eng, opts);
}

cibyl_errno_t handle_searchmoves(char *moves)
{
    /* TODO: Implement me. */
    return CIBYL_EABORT;
}

cibyl_errno_t parse_i64(cibyl_error_t *err, char *token, int64_t *out)
{
    char *endp;

    if (token == NULL && *token == '\0') {
        return CIBYL_MKERR(err, CIBYL_EINVAL, "parse_i64: string expected\n");
    }
    *out = strtoll(token, &endp, 10);
    if (*endp != '\0') {
        return CIBYL_MKERR(err, CIBYL_EINVAL, "parse_i64: token is not an integer\n");
    }
    
    return CIBYL_EOK;
}

cibyl_errno_t handle_go(cibyl_error_t *err, uci_engine_t *eng, char *opts)
{
    typedef enum {
        GO_SEARCHMOVES,
        GO_PONDER,
        GO_WTIME,
        GO_BTIME,
        GO_WINC,
        GO_BINC,
        GO_MOVESTOGO,
        GO_DEPTH,
        GO_NODES,
        GO_MATE,
        GO_MOVETIME,
        GO_INFINITE
    } go_opt_t;

    const char STR_GO_SEARCHMOVES[] = "searchmoves";
    const char STR_GO_PONDER[] = "ponder";
    const char STR_GO_WTIME[] = "wtime";
    const char STR_GO_BTIME[] = "btime";
    const char STR_GO_WINC[] = "winc";
    const char STR_GO_BINC[] = "binc";
    const char STR_GO_MOVESTOGO[] = "movestogo";
    const char STR_GO_DEPTH[] = "depth";
    const char STR_GO_NODES[] = "nodes";
    const char STR_GO_MATE[] = "mate";
    const char STR_GO_MOVETIME[] = "movetime";
    const char STR_GO_INFINITE[] = "infinite";

    search_params_t search_params;
    char *token;
    char *endp;
    
    /* Clear go params. */
    clear_search_params(&search_params);

    /* Handle null input. */
    if (opts == NULL) {
        search_params.infinite = true;
        goto search;
    }

    /* Loop through all of the arguments to the go command. */
    token = strtok(opts, STR_UCI_DELIMS);
    while (token != NULL) {
        /* Handle searchmoves, this will consume all remaining arguments. */
        if (strcmp(token, STR_GO_SEARCHMOVES) == 0) {
            handle_searchmoves(strtok(opts, STR_UCI_DELIMS));
            break;
        }

        /* Handle various flags. */
        else if (strcmp(token, STR_GO_PONDER) == 0) {
            search_params.ponder = true;
        } else if (strcmp(token, STR_GO_INFINITE) == 0) {
            search_params.infinite = true;
        }

        /* Handle time information. */
        else if (strcmp(token, STR_GO_WTIME) == 0) {
            if (parse_i64(err, strtok(opts, STR_UCI_DELIMS), &search_params.wtime) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_BTIME) == 0) {
            if (parse_i64(err, strtok(opts, STR_UCI_DELIMS), &search_params.btime) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_WINC) == 0) {
            if (parse_i64(err, strtok(opts, STR_UCI_DELIMS), &search_params.winc) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_BINC) == 0) {
            if (parse_i64(err, strtok(opts, STR_UCI_DELIMS), &search_params.binc) < 0)
                return CIBYL_EABORT;
        }

        /* Handle move stopping. */
        else if (strcmp(token, STR_GO_MOVESTOGO) == 0) {
            if (parse_i64(err, strtok(opts, STR_UCI_DELIMS), &search_params.movestogo) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_DEPTH) == 0) {
            if (parse_i64(err, strtok(opts, STR_UCI_DELIMS), &search_params.depth) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_NODES) == 0) {
            if (parse_i64(err, strtok(opts, STR_UCI_DELIMS), &search_params.nodes) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_MATE) == 0) {
            if (parse_i64(err, strtok(opts, STR_UCI_DELIMS), &search_params.mate) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_MOVETIME) == 0) {
            if (parse_i64(err, strtok(opts, STR_UCI_DELIMS), &search_params.movetime) < 0)
                return CIBYL_EABORT;
        }

        /* Grab the next token. */
        token = strtok(NULL, STR_UCI_DELIMS);
    }

search:
    /* Start thinking. */
    if (eng_start_search(err, &eng->eng, &search_params)) {
        return CIBYL_ERR_ADD_CONTEXT(err);
    }

    return CIBYL_EOK;
}

void hanlde_display()
{

}

cibyl_errno_t handle_cmd(cibyl_error_t *err, uci_engine_t *eng, char *cmd)
{
    cibyl_errno_t result = CIBYL_EOK;

    char *token = strtok(cmd, " \n");
    char *opts;
    bool token_accepted = false;

    while (!token_accepted && token != NULL) {
        token_accepted = true;

        /* Initialization commands. */
        if (strcmp(cmd, STR_UCI) == 0) {
            /* Respond to the UCI command. */
            printf("id name %s\n", ENGINE_NAME);
            printf("id author %s\n", ENGINE_AUTHOR);
            printf("uciok\n");
        } else if (strcmp(cmd, STR_DEBUG) == 0) {
            eng->debug = true;
        } else if (strcmp(cmd, STR_ISREADY) == 0) {
            /* Wait for the engine to be ready to process input. */
            if (eng_prepare(err, &eng->eng) != CIBYL_EOK) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
            printf("readyok\n");
        } else if (strcmp(cmd, STR_SETOPTION) == 0) {
            /* TODO: Implement me. */
        } else if (strcmp(cmd, STR_REGISTER) == 0) {
            /* Do nothing as cibyl is not password protected. */
        }

        /* Board setup commands. */
        else if (strcmp(cmd, STR_UCINEWGAME) == 0) {
            if (eng_set_ucifen(err, &eng->eng, "startpos") != CIBYL_EOK) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
        } else if (strcmp(cmd, STR_POSITION) == 0) {
            if (eng_set_ucifen(err, &eng->eng, strtok(cmd, "\n")) != CIBYL_EOK) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
        }

        /* Functions for controlling thinking. */
        else if (strcmp(cmd, STR_GO) == 0) {
            result = handle_go(err, eng, strtok(NULL, ""));
        } else if (strcmp(cmd, STR_STOP) == 0) {
            eng_broadcast_stop(&eng->eng);
        } else if (strcmp(cmd, STR_PONDERHIT) == 0) {
            eng_notify_ponderhit(&eng->eng);
        }

        /* Miscelaneous functions. */
        else if (strcmp(cmd, STR_QUIT) == 0) {
            eng->exit = true;
        } else if (strcmp(cmd, STR_DISPLAY) == 0) {
            /* TODO: Implement me. */
        }

        /* If the first word is invalid, then parse starting with the next as per the spec. */
        else {
            token = strtok(NULL, " ");
            token_accepted = false;
        }
    }

err:
    return result;
}

void uci_init(uci_engine_t *eng)
{
    /* Set the engine as uninitialized. */
    eng->debug = false;
    eng->initialized = false;
    eng->exit = false;

    /* Bring the underlying engine to a sane state. */
    eng_init(&eng->eng);
}

cibyl_errno_t uci_process(cibyl_error_t *err, uci_engine_t *eng)
{
    char cmd[MAX_COMMAND_LEN];

    /* TODO: Use select to wait for data from multiple fds.
     * This isn't a big deal until threads can throw errors (tablebase reads). */
    while (!eng->exit && fgets(cmd, MAX_COMMAND_LEN, stdin) != NULL) {
        if (handle_cmd(err, eng, cmd)) {
            CIBYL_ERR_ADD_CONTEXT(err);
            return CIBYL_EABORT;
        }
    }

    /* TODO: Detect if the exit was caused by an error in the engine. */
    return CIBYL_EOK;
}

void uci_deinit(uci_engine_t *eng)
{
    /* Deinitialize the engine. */
    eng_deinit(&eng->eng);
}
