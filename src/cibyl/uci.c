
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "cibyl.h"
#include "uci.h"

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

/* Non-uci command strings. */
const char STR_DISPLAY[] = "d";
const char STR_EVAL[] = "eval";

/* Engine and author information. */
const char ENGINE_NAME[] = "Cibyl";
const char ENGINE_AUTHOR[] = "Joel Kuehne";

cibyl_errno_t uci_report_panic(engine_t *eng, void *udata)
{
    /* TODO: Implement me. */
    return CIBYL_EABORT;
}

cibyl_errno_t uci_report_bestmove(engine_t *eng, void *udata)
{
    /* TODO: Implement me. */
    return CIBYL_EABORT;
}

cibyl_errno_t uci_report_info(engine_t *eng, void *udata)
{
    /* TODO: Implement me. */
    return CIBYL_EABORT;
}

cibyl_errno_t handle_position(uci_engine_t *eng, char *opts)
{
    return eng_set_ucifen(&eng->eng, opts);
}

cibyl_errno_t handle_searchmoves(char *moves)
{
    /* TODO: Implement me. */
    return CIBYL_EABORT;
}

cibyl_errno_t parse_i64(char *token, int64_t *out)
{
    char *endp;

    if (token == NULL && *token == '\0') {
        cibyl_write_log("parse_i64: valid string expected\n");
        return CIBYL_EABORT;
    }
    *out = strtoll(token, &endp, 10);
    if (*endp != '\0') {
        cibyl_write_log("parse_i64: token is not an integer\n");
        return CIBYL_EABORT;
    }
    
    return CIBYL_EOK;
}

cibyl_errno_t handle_go(uci_engine_t *eng, char *opts)
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

    go_param_t go_params;
    char *token;
    char *endp;
    
    /* Clear go params. */
    clear_go_params(&go_params);

    /* Handle null input. */
    if (opts == NULL) {
        return CIBYL_EABORT;
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
            go_params.ponder = true;
        } else if (strcmp(token, STR_GO_INFINITE) == 0) {
            go_params.infinite = true;
        }

        /* Handle time information. */
        else if (strcmp(token, STR_GO_WTIME) == 0) {
            if (parse_i64(strtok(opts, STR_UCI_DELIMS), &go_params.wtime) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_BTIME) == 0) {
            if (parse_i64(strtok(opts, STR_UCI_DELIMS), &go_params.btime) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_WINC) == 0) {
            if (parse_i64(strtok(opts, STR_UCI_DELIMS), &go_params.winc) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_BINC) == 0) {
            if (parse_i64(strtok(opts, STR_UCI_DELIMS), &go_params.binc) < 0)
                return CIBYL_EABORT;
        }

        /* Handle move stopping. */
        else if (strcmp(token, STR_GO_MOVESTOGO) == 0) {
            if (parse_i64(strtok(opts, STR_UCI_DELIMS), &go_params.movestogo) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_DEPTH) == 0) {
            if (parse_i64(strtok(opts, STR_UCI_DELIMS), &go_params.depth) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_NODES) == 0) {
            if (parse_i64(strtok(opts, STR_UCI_DELIMS), &go_params.nodes) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_MATE) == 0) {
            if (parse_i64(strtok(opts, STR_UCI_DELIMS), &go_params.mate) < 0)
                return CIBYL_EABORT;
        } else if (strcmp(token, STR_GO_MOVETIME) == 0) {
            if (parse_i64(strtok(opts, STR_UCI_DELIMS), &go_params.movetime) < 0)
                return CIBYL_EABORT;
        }

        /* Grab the next token. */
        token = strtok(NULL, STR_UCI_DELIMS);
    }

    /* Start thinking. */
    eng_notify_go(&eng->eng, &go_params);

    return CIBYL_EOK;
}

void hanlde_display()
{

}

cibyl_errno_t handle_cmd(uci_engine_t *eng, char *cmd)
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

            /* Begin asynchronous initialization. */
            eng_begin_init(&eng->eng);
        } else if (strcmp(cmd, STR_DEBUG) == 0) {
            eng->debug = true;
        } else if (strcmp(cmd, STR_ISREADY) == 0) {
            /* Wait for the engine to be ready to process input. */
            eng_await_ready(&eng->eng);
            printf("readyok\n");
        } else if (strcmp(cmd, STR_SETOPTION) == 0) {
            /* TODO: Implement me. */
        } else if (strcmp(cmd, STR_REGISTER) == 0) {
            /* Do nothing as cibyl is not password protected. */
        }

        /* Board setup commands. */
        else if (strcmp(cmd, STR_UCINEWGAME) == 0) {
            /* TODO: Implement me. */
            eng_set_ucifen(&eng->eng, "startpos");
        } else if (strcmp(cmd, STR_POSITION) == 0) {
            eng_set_ucifen(&eng->eng, strtok(cmd, "\n"));
        }

        /* Functions for controlling thinking. */
        else if (strcmp(cmd, STR_GO) == 0) {
            result = handle_go(eng, strtok(NULL, ""));   /* Slice off the rest of the cmd. */
        } else if (strcmp(cmd, STR_STOP) == 0) {
            eng_notify_stop(&eng->eng);
        } else if (strcmp(cmd, STR_PONDERHIT) == 0) {
            eng_notify_ponderhit(&eng->eng);
        }

        /* Miscelaneous functions. */
        else if (strcmp(cmd, STR_QUIT) == 0) {
            eng_cleanup(&eng->eng);
        } else if (strcmp(cmd, STR_DISPLAY) == 0) {
            /* TODO: Implement me. */
        }

        /* If the first word is invalid, then parse starting with the next as per the spec. */
        else {
            token = strtok(NULL, " ");
            token_accepted = false;
        }
    }

    return result;
}

cibyl_errno_t uci_init_slow(uci_engine_t *eng)
{
    int result = CIBYL_EOK;

    /* Create the panic notification pipe. */
#ifdef _WIN32
    if (CreatePipe(&eng->h_msg_read, &hWritePipe->h_msg_write, NULL, WIN_PIPE_SIZE) {
        cibyl_write_log("CreatePipe: %s\n", _strerror(NULL));
        result = CIBYL_EABORT;
        goto out;
    }
#else
    if (pipe(eng->error_pipe) == -1) {
        cibyl_write_log("pipe: %s\n", strerror(errno));
        result = CIBYL_EABORT;
        goto out;
    }
#endif

    /* Do not initialize the engine yet. */


    /* Initialize the underlying engine. */
    if (eng_begin_init(&eng->eng)) {
        cibyl_write_log("eng_begin_init: %s\n", strerror(errno));
        result = CIBYL_EABORT;
        goto out;
    }

err_close_pipe:
#ifdef _WIN32
    CloseHandle(eng->h_msg_read)):
    CloseHandle(eng->h_msg_write));
#else
    close(eng->error_pipe[0]);
    close(eng->error_pipe[1]);
#endif

out:
    return result;
}

cibyl_errno_t uci_init(uci_engine_t *eng, uint32_t nthreads)
{
}

cibyl_errno_t uci_process(uci_engine_t *eng)
{
    /* TODO: Implement me. */
}

