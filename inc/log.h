
#ifndef CIBYL_LOG_H
#define CIBYL_LOG_H

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#define DESC_STRLEN 128
#define CTX_STRLEN (1024 - DESC_STRLEN)

/* Macro to add context to an error struct. Only works in debug mode. */
#ifdef NDEBUG
#define CIBYL_ERR_ADD_CONTEXT(err) (err)->num
#else
#define CIBYL_ERR_ADD_CONTEXT(err) (err)->num; if (err != NULL) { \
	snprintf((err)->context + strlen((err)->context), CTX_STRLEN - strlen((err)->context), \
		"- in %s (%s:%d)\n", __func__, __FILE_NAME__, __LINE__); \
}
#endif

/* Macro to populate an error struct. */
#ifdef NDEBUG
#define CIBYL_MKERR(err, errno, format, ...) errno; if (err != NULL) {\
    (err)->num = errno; \
    snprintf((err)->desc, DESC_STRLEN, format __VA_OPT__(,) __VA_ARGS__); \
}
#else
#define CIBYL_MKERR(err, errno, format, ...) errno; if (err != NULL) {\
    (err)->num = errno; \
    snprintf((err)->desc, DESC_STRLEN, format __VA_OPT__(,) __VA_ARGS__); \
    (err)->context[0] = '\0'; \
    CIBYL_ERR_ADD_CONTEXT(err); \
}
#endif

/* Macro to write an error struct. */
#ifdef NDEBUG
#define CIBYL_WRITE_ERR(err) (err)->num; if (err != NULL) {\
	CIBYL_ERR_ADD_CONTEXT((err)); \
	cibyl_write_log("%s\n", (err)->desc); \
	printf("info string CRITICAL ERROR: %s\n", (err)->desc); \
}
#else
#define CIBYL_WRITE_ERR(err) (err)->num; if (err != NULL) {\
	CIBYL_ERR_ADD_CONTEXT((err)); \
	cibyl_write_log("%s\n%s\n", (err)->desc, (err)->context); \
	printf("info string CRITICAL ERROR: %s\n", (err)->desc); \
}
#endif

/**
 * @brief File pointer for the log output.
 */
extern FILE *log_file;
extern pthread_mutex_t log_lock;

/**
 * @breif Error codes for different operations that can take place.
 */
typedef enum {
    CIBYL_ENOMEM = -2, /**< Non-recoverable error for malloc failures. */
    CIBYL_EABORT = -1, /**< Generic non-recoverable error. */
    CIBYL_EOK = 0,     /**< Default return value. */
    CIBYL_EINVAL,      /**< Recoverable invalid formatting error. */
} cibyl_errno_t;

/**
 * @breif Defines an error type for errors that will be returned by the library.
 */
typedef struct {
    cibyl_errno_t num;        /**< The error code for the error. */
    char desc[DESC_STRLEN];   /**< A string description of the error. */
#ifndef NDEBUG
    char context[CTX_STRLEN]; /**< A string description of the context of the error. */
#endif
} cibyl_error_t;

/**
 * @breif Function to write to the log file.
 * @param format A printf compliant format string.
 * @param args A va_list vector of arguments.
 */
static void cibyl_vwrite_log(char *format, va_list args)
{
    if (log_file == NULL)
        return;
    if (pthread_mutex_lock(&log_lock))
        return;
    vfprintf(log_file, format, args);
    pthread_mutex_unlock(&log_lock);
}

/**
 * @breif Function to write to the log file.
 * @param format A printf compliant format string.
 * @param ... All remaining arguments passed into the format string.
 */
static void cibyl_write_log(char *format, ...)
{
    va_list args;
    va_start(args, format);
    cibyl_vwrite_log(format, args);
    va_end(args);
}

#endif /* CIBYL_LOG_H */
