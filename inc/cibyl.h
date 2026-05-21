
#ifndef CIBYL_H
#define CIBYL_H

#include "cb_lib.h"

/**
 * @breif Function to write to the log file.
 * @param format A printf compliant format string.
 * @param args A va_list vector of arguments.
 */
void cibyl_vwrite_log(char *format, va_list args);

/**
 * @breif Function to write to the log file.
 * @param format A printf compliant format string.
 * @param ... All remaining arguments passed into the format string.
 */
void cibyl_write_log(char *format, ...);
 
/**
 * @breif Error codes for different operations.
 */
typedef enum {
    CIBYL_ENOMEM = -2,      /**< Non-recoverable error for malloc failures. */
    CIBYL_EABORT = -1,      /**< Generic non-recoverable error. */
    CIBYL_EOK = 0           /**< Default return value. */
} cibyl_errno_t;

#endif /* CIBYL_H */
