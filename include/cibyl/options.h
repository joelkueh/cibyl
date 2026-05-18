
#ifndef CYBIL_OPTIONS_H
#define CYBIL_OPTIONS_H

typedef enum {
	CIBYL_OPT_NONE,
	CIBYL_OPT_THREADS,
} option_type_t;

typedef struct {
	int nthreads;
} option_threads_t;

typedef struct {
	option_type_t type;
    union {
    	option_threads_t threads;
    };
} option_t;

#endif /* CIBYL_OPTIONS_H */
