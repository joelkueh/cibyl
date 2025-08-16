
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "event2/event.h"
#include "cibyl.h"
#include "uci.h"
#include "logging.h"

#define USR_MSG_BUFFER 512

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

/* Non-uci command strings. */
const char STR_DISPLAY[] = "d";
const char STR_EVAL[] = "eval";

const char ENGINE_NAME[] = "Cibyl";
const char ENGINE_AUTHOR[] = "Joel Kuehne";

uci_engine_t engine;

int handle_position(char *opts)
{
    return eng_set_ucifen(&engine.eng, opts);
}

int handle_searchmoves(char *moves)
{
    /* TODO: Implement me. */
    return -1;
}

int parse_i64(char *token, int64_t *out)
{
    char *endp;

    if (token == NULL && *token == '\0') {
        cibyl_write_log("parse_i64: valid string expected\n");
        return -1;
    }
    *out = strtoll(token, &endp, 10);
    if (*endp != '\0') {
        cibyl_write_log("parse_i64: token is not an integer\n");
        return -1;
    }
    
    return 0;
}

int handle_go(char *opts)
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

    go_params_t go_params;
    char *token;
    char *endp;
    
    /* Clear go params. */
    clear_go_params(&go_params);

    /* Handle null input. */
    if (opts == NULL) {
        return KH_EABORT;
    }

    /* Loop through all of the arguments to the go command. */
    token = strtok(opts, " \t\n");
    while (token != NULL) {
        /* Handle searchmoves, this will consume all remaining arguments. */
        if (strcmp(token, STR_GO_SEARCHMOVES) == 0) {
            handle_searchmoves(strtok(opts, " \t\n"));
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
            if (parse_i64(strtok(opts, " \t\n"), &go_params.wtime) < 0)
                return KH_EABORT;
        } else if (strcmp(token, STR_GO_BTIME) == 0) {
            if (parse_i64(strtok(opts, " \t\n"), &go_params.btime) < 0)
                return KH_EABORT;
        } else if (strcmp(token, STR_GO_WINC) == 0) {
            if (parse_i64(strtok(opts, " \t\n"), &go_params.winc) < 0)
                return KH_EABORT;
        } else if (strcmp(token, STR_GO_BINC) == 0) {
            if (parse_i64(strtok(opts, " \t\n"), &go_params.binc) < 0)
                return KH_EABORT;
        }

        /* Handle move stopping. */
        else if (strcmp(token, STR_GO_MOVESTOGO) == 0) {
            if (parse_i64(strtok(opts, " \t\n"), &go_params.movestogo) < 0)
                return KH_EABORT;
        } else if (strcmp(token, STR_GO_DEPTH) == 0) {
            if (parse_i64(strtok(opts, " \t\n"), &go_params.depth) < 0)
                return KH_EABORT;
        } else if (strcmp(token, STR_GO_NODES) == 0) {
            if (parse_i64(strtok(opts, " \t\n"), &go_params.nodes) < 0)
                return KH_EABORT;
        } else if (strcmp(token, STR_GO_MATE) == 0) {
            if (parse_i64(strtok(opts, " \t\n"), &go_params.mate) < 0)
                return KH_EABORT;
        } else if (strcmp(token, STR_GO_MOVETIME) == 0) {
            if (parse_i64(strtok(opts, " \t\n"), &go_params.movetime) < 0)
                return KH_EABORT;
        }

        /* Grab the next token. */
        token = strtok(NULL, " \t\n");
    }

    /* Start thinking. */
    eng_notify_go(&engine.eng, &go_params);

    return KH_EOK;
}

void hanlde_display()
{

}

int handle_cmd(char *cmd)
{
    int result = 0;

    char *token = strtok(cmd, " \t\n");
    char *opts;
    bool token_accepted = false;

    while (token_accepted && token != NULL) {
        token_accepted = true;

        /* Initialization commands. */
        if (strcmp(cmd, STR_UCI) == 0) {
            /* Respond to the UCI command. */
            printf("id name %s\n", ENGINE_NAME);
            printf("id author %s\n", ENGINE_AUTHOR);
            printf("uciok\n");
        } else if (strcmp(cmd, STR_DEBUG) == 0) {
            engine.debug = true;
        } else if (strcmp(cmd, STR_ISREADY) == 0) {
            /* Wait for initialization to be complete. */
            eng_await_isready(&engine.eng);
            printf("readyok\n");
        } else if (strcmp(cmd, STR_SETOPTION) == 0) {
            /* TODO: Implement me. */
        } else if (strcmp(cmd, STR_REGISTER) == 0) {
            /* Do nothing as this is not password protected. */
        }

        /* Board setup commands. */
        else if (strcmp(cmd, STR_UCINEWGAME) == 0) {
            /* TODO: Implement me. */
            eng_set_ucifen(&engine.eng, "startpos");
        } else if (strcmp(cmd, STR_POSITION) == 0) {
            eng_set_ucifen(&engine.eng, strtok(NULL, "\n"));
        }

        /* Functions for controlling thinking. */
        else if (strcmp(cmd, STR_GO) == 0) {
            result = handle_go(strtok(NULL, "\n"));
        } else if (strcmp(cmd, STR_STOP) == 0) {
            eng_notify_stop(&engine.eng);
        } else if (strcmp(cmd, STR_PONDERHIT) == 0) {
            eng_notify_ponderhit(&engine.eng);
        }

        /* Miscelaneous functions. */
        else if (strcmp(cmd, STR_QUIT) == 0) {
            eng_cleanup(&engine.eng);
        } else if (strcmp(cmd, STR_DISPLAY) == 0) {
            /* TODO: Implement me. */
        }

        /* If the first word is invalid, then parse starting with the next as per the spec. */
        else {
            token = strtok(NULL, " \t\n");
            token_accepted = false;
        }
    }

    return result;
}

void uci_eng_done_cb(evutil_socket_t sock, short flags, void *eng_addr)
{
    char best_mv_str[6];
    char ponder_mv_str[6];
    uci_engine_t *ueng = (uci_engine_t *)eng_addr;

    /* Clear the timeout event. */
    event_del(ueng->ev_timeout);

    /* Send a message to the user. */
    cb_mv_to_uci_algbr(best_mv_str, ueng->eng.best_mv);
    cb_mv_to_uci_algbr(ponder_mv_str, ueng->eng.ponder_mv);
    printf("bestmove %s ponder %s\n", best_mv_str, ponder_mv_str);
}

/* This event could possibly be omitted, might be fine enough
 * to do a timeout on ev_done and just take whatever is computed
 * in the transposition table. */
void uci_eng_timeout_cb(evutil_socket_t sock, short flags, void *eng_addr)
{
    uci_engine_t *ueng = (uci_engine_t *)eng_addr;
    eng_notify_stop(&ueng->eng);
}

void uci_usr_msg_cb(evutil_socket_t sock, short flags, void *eng_addr)
{
    char *uci_msg_buf = NULL;
    size_t bufsize;
    int result = 0;

    uci_engine_t *ueng = (uci_engine_t *)eng_addr;
    if (getline(&uci_msg_buf, &bufsize, stdin) == -1) {
        cibyl_perror("getline: ", errno);
        goto free_buf;
    }

    if (handle_cmd(uci_msg_buf) != 0) {
        cibyl_write_log("handle_cmd failed");
        goto free_buf;
    }

free_buf:
    free(uci_msg_buf);
}

int uci_main(uci_engine_t *ueng)
{
    int result = 0;

    /* Initialize events. */
    ueng->ev_base = event_base_new();
    ueng->ev_done = event_new(ueng->ev_base, -1, 0, uci_eng_done_cb, (void *)ueng);
    ueng->ev_timeout = evtimer_new(ueng->ev_base, uci_eng_timeout_cb, (void *)ueng);
    ueng->ev_usr_msg = event_new(ueng->ev_base, STDIN_FILENO,
                                 EV_READ, uci_usr_msg_cb, (void *)ueng);

    /* Add events. */
    if (event_add(ueng->ev_done, NULL)) {
        cibyl_write_log("event_add: failed to add event ev_done");
        result = -1;
        goto free_events;
    }
    if (event_add(ueng->ev_usr_msg, NULL)) {
        cibyl_write_log("event_add: failed to add event ev_usr_msg");
        result = -1;
        goto free_events;
    }

    /* Begin chessboard initialization. */
    /* NOTE: Why doesn't this function return an error struct? */
    if (eng_init(&engine.eng, ueng->ev_done)) {
        cibyl_write_log("eng_init: failed");
        result = -1;
        goto free_events;
    }

    /* Enter event loop. */
    if (event_base_dispatch(ueng->ev_base)) {
        cibyl_write_log("event_base_dispatch: failed to enter event loop");
        result = -1;
        goto free_events;
    }

free_events:
    event_free(ueng->ev_done);
    event_free(ueng->ev_timeout);
    event_free(ueng->ev_usr_msg);
    event_base_free(ueng->ev_base);

    return result;
}

