// A C99+ option parser.
// Copyright (c) 2022 hippie68 (https://github.com/hippie68/optparse99)

#include "optparse99.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define MUTUALLY_EXCLUSIVE_GROUPS_MAX 16
#define PRINT_BUFFER_SIZE 8192

static struct optparse_cmd *optparse_main_cmd; // The command tree's root
static char **args; // Contains the current state of argv while parsing
static int args_index; // Keeps track of the currently parsed argument's index

#ifndef OPTPARSE_HELP_INDENTATION_WIDTH
#define OPTPARSE_HELP_INDENTATION_WIDTH 2
#endif
#ifndef OPTPARSE_HELP_MAX_DIVIDER_WIDTH
#define OPTPARSE_HELP_MAX_DIVIDER_WIDTH 32
#endif
#ifndef OPTPARSE_HELP_FLOATING_DESCRIPTIONS
#define OPTPARSE_HELP_FLOATING_DESCRIPTIONS true
#endif
#ifndef OPTPARSE_HELP_MAX_LINE_WIDTH
#define OPTPARSE_HELP_MAX_LINE_WIDTH 80
#endif
#ifndef OPTPARSE_HELP_WORD_WRAP
#define OPTPARSE_HELP_WORD_WRAP true
#endif
#ifndef OPTPARSE_HELP_USAGE_STYLE
#define OPTPARSE_HELP_USAGE_STYLE 1 // 0: short, 1: verbose
#endif
#ifndef OPTPARSE_HELP_USAGE_OPTIONS_STRING
#define OPTPARSE_HELP_USAGE_OPTIONS_STRING "OPTIONS"
#endif
#ifndef OPTPARSE_HELP_LETTER_CASE
#define OPTPARSE_HELP_LETTER_CASE 0 // 0: capitalized, 1: lower, 2: upper
#endif
#ifndef OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS
#define OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS true
#endif

/// Function "strtox" ----------------------------------------------------------

#include <ctype.h>
#include <errno.h>
#if OPTPARSE_FLOATING_POINT_SUPPORT
#include <float.h>
#endif
#include <stdbool.h>
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Converts a string to a different type.
// Return value:  0: success
//                1: string is not convertible
//               -1: string is too long
// Example:
//     int i;
//     strtox("512", &i, STRTOI);
int strtox(char *str, void *x, enum strtox_conversion_type conversion_type) {
    if (str == NULL) {
        return 1;
    }

    char *endptr = NULL;
    errno = 0;

    switch (conversion_type) {
        case STRTOC:
            if (strlen(str) > 1) {
                errno = ERANGE;
            }
            *(char *) x = str[0];
            break;
        case STRTOSC:
            if (strlen(str) > 1) {
                errno = ERANGE;
            }
            *(signed char *) x = str[0];
            break;
        case STRTOUC:
            if (strlen(str) > 1) {
                errno = ERANGE;
            }
            *(unsigned char *) x = str[0];
            break;
        case STRTOS:
            {
                long result = strtol(str, &endptr, 0);
                if (result < SHRT_MIN || result > SHRT_MAX) {
                    errno = ERANGE;
                }
                *(short *) x = result;
            }
            break;
        case STRTOUS:
            {
                unsigned long result = strtoul(str, &endptr, 0);
                if (result > USHRT_MAX) {
                    errno = ERANGE;
                }
                *(unsigned short *) x = result;
            }
            break;
        case STRTOI:
            {
                long result = strtol(str, &endptr, 0);
                if (result < INT_MIN || result > INT_MAX) {
                    errno = ERANGE;
                }
                *(int *) x = result;
            }
            break;
        case STRTOUI:
            {
                unsigned long result = strtoul(str, &endptr, 0);
                if (result > UINT_MAX) {
                    errno = ERANGE;
                }
                *(unsigned int *) x = result;
            }
            break;
        case STRTOL:
            *(long *) x = strtol(str, &endptr, 0);
            break;
        case STRTOUL:
            *(unsigned long *) x = strtoul(str, &endptr, 0);
            break;
        case STRTOLL:
            *(long long *) x = strtoll(str, &endptr, 0);
            break;
        case STRTOULL:
            *(unsigned long long *) x = strtoull(str, &endptr, 0);
            break;
#if OPTPARSE_FLOATING_POINT_SUPPORT
        case STRTOF:
            {
                double result = strtod(str, &endptr);
                if (result < FLT_MIN || result > FLT_MAX) {
                    errno = ERANGE;
                }
                *(float *) x = result;
            }
            break;
        case STRTOD:
            *(double *) x = strtod(str, &endptr);
            break;
        case STRTOLD:
            *(long double *) x = strtold(str, &endptr);
            break;
#endif
        case STRTOB:
            {
                int len = strlen(str);
                char temp[len + 1];
                strcpy(temp, str);
                for (int i = 0; i < len; i++) {
                    temp[i] = tolower(temp[i]);
                }
                if (strcmp(temp, "true") == 0) {
                    *(bool *) x = true;
                } else if (strcmp(temp, "false") == 0) {
                    *(bool *) x = false;
                } else if (strcmp(temp, "enabled") == 0) {
                    *(bool *) x = true;
                } else if (strcmp(temp, "disabled") == 0) {
                    *(bool *) x = false;
                } else if (strcmp(temp, "yes") == 0) {
                    *(bool *) x = true;
                } else if (strcmp(temp, "no") == 0) {
                    *(bool *) x = false;
                } else if (strcmp(temp, "on") == 0) {
                    *(bool *) x = true;
                } else if (strcmp(temp, "off") == 0) {
                    *(bool *) x = false;
                } else {
                    int result = strtox(str, x, STRTOI);
                    if (result == 1 || result == 0) {
                        *(bool *) x = result;
                    } else {
                        endptr = str;
                    }
                }
            }
            break;
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
        case STRTOI8:
            {
                int8_t result = strtol(str, &endptr, 0);
                if (result < INT8_MIN || result > INT8_MAX) {
                    errno = ERANGE;
                }
                *(int8_t *) x = result;
            }
            break;
        case STRTOUI8:
            {
                uint8_t result = strtol(str, &endptr, 0);
                if (result > UINT8_MAX) {
                    errno = ERANGE;
                }
                *(uint8_t *) x = result;
            }
            break;
        case STRTOI16:
            {
                int16_t result = strtol(str, &endptr, 0);
                if (result < INT16_MIN || result > INT16_MAX) {
                    errno = ERANGE;
                }
                *(int16_t *) x = result;
            }
            break;
        case STRTOUI16:
            {
                uint16_t result = strtol(str, &endptr, 0);
                if (result > UINT16_MAX) {
                    errno = ERANGE;
                }
                *(uint16_t *) x = result;
            }
            break;
        case STRTOI32:
            {
                int32_t result = strtol(str, &endptr, 0);
                if (result < INT32_MIN || result > INT32_MAX) {
                    errno = ERANGE;
                }
                *(int32_t *) x = result;
            }
            break;
        case STRTOUI32:
            {
                uint32_t result = strtol(str, &endptr, 0);
                if (result > UINT32_MAX) {
                    errno = ERANGE;
                }
                *(uint32_t *) x = result;
            }
            break;
        case STRTOI64:
            {
                int64_t result = strtoll(str, &endptr, 0);
                if (result < INT64_MIN || result > INT64_MAX) {
                    errno = ERANGE;
                }
                *(int64_t *) x = result;
            }
            break;
        case STRTOUI64:
            {
                uint64_t result = strtoll(str, &endptr, 0);
                if (result > UINT64_MAX) {
                    errno = ERANGE;
                }
                *(uint64_t *) x = result;
            }
            break;
#endif
    }

    if (endptr && (endptr == str || endptr[0] != '\0')) {
        return 1;
    } else if (errno == ERANGE) {
        return -1;
    } else {
        return 0;
    }
}

/// Private functions ----------------------------------------------------------

// Prints an error message and quits.
static void optparse_error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

// Safely prints to a buffer of size PRINT_BUFFER_SIZE;
static void bprintf(char *buffer, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t len = strlen(buffer);
    vsnprintf(buffer + len, PRINT_BUFFER_SIZE - len, fmt, ap);
    va_end(ap);
}

// Executes an option structure's tasks.
static void execute_option(struct optparse_opt *opt, char *arg) {
    int result = 0;

    // Set option's flag
    if (opt->flag) {
        switch (opt->flag_action) {
            case FLAG_ACTION_SET_TRUE:
                *(int *) opt->flag = 1;
                break;
            case FLAG_ACTION_SET_FALSE:
                *(int *) opt->flag = 0;
                break;
            case FLAG_ACTION_INCREMENT:
                *(int *) opt->flag += 1;
                break;
            case FLAG_ACTION_DECREMENT:
                *(int *) opt->flag -= 1;
                break;
        }
    }

    // Used to hold a type-converted option-argument
    union {
        char t_char;
        signed char t_schar;
        unsigned char t_uchar;
        short t_shrt;
        unsigned short t_ushrt;
        int t_int;
        unsigned int t_uint;
        long t_long;
        unsigned long t_ulong;
        long long t_llong;
        unsigned long long t_ullong;
#if OPTPARSE_FLOATING_POINT_SUPPORT
        float t_flt;
        double t_dbl;
        long double t_ldbl;
#endif
        _Bool t_bool;
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
        int8_t t_int8;
        uint8_t t_uint8;
        int16_t t_int16;
        uint16_t t_uint16;
        int32_t t_int32;
        uint32_t t_uint32;
        int64_t t_int64;
        uint64_t t_uint64;
#endif
    } conv_arg;

    // Convert option-argument to a different type
    if (opt->arg_type) {
        switch (opt->arg_type) {
            case ARG_TYPE_CHAR:
                result = strtox(arg, &conv_arg, STRTOC);
                break;
            case ARG_TYPE_SCHAR:
                result = strtox(arg, &conv_arg, STRTOSC);
                break;
            case ARG_TYPE_UCHAR:
                result = strtox(arg, &conv_arg, STRTOUC);
                break;
            case ARG_TYPE_SHRT:
                result = strtox(arg, &conv_arg, STRTOS);
                break;
            case ARG_TYPE_USHRT:
                result = strtox(arg, &conv_arg, STRTOUS);
                break;
            case ARG_TYPE_INT:
                result = strtox(arg, &conv_arg, STRTOI);
                break;
            case ARG_TYPE_UINT:
                result = strtox(arg, &conv_arg, STRTOUI);
                break;
            case ARG_TYPE_LONG:
                result = strtox(arg, &conv_arg, STRTOL);
                break;
            case ARG_TYPE_ULONG:
                result = strtox(arg, &conv_arg, STRTOUL);
                break;
            case ARG_TYPE_LLONG:
                result = strtox(arg, &conv_arg, STRTOLL);
                break;
            case ARG_TYPE_ULLONG:
                result = strtox(arg, &conv_arg, STRTOULL);
                break;
#if OPTPARSE_FLOATING_POINT_SUPPORT
            case ARG_TYPE_FLT:
                result = strtox(arg, &conv_arg, STRTOF);
                break;
            case ARG_TYPE_DBL:
                result = strtox(arg, &conv_arg, STRTOD);
                break;
            case ARG_TYPE_LDBL:
                result = strtox(arg, &conv_arg, STRTOLD);
                break;
#endif
            case ARG_TYPE_BOOL:
                result = strtox(arg, &conv_arg, STRTOB);
                break;
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
            case ARG_TYPE_INT8:
                result = strtox(arg, &conv_arg, STRTOI8);
                break;
            case ARG_TYPE_UINT8:
                result = strtox(arg, &conv_arg, STRTOUI8);
                break;
            case ARG_TYPE_INT16:
                result = strtox(arg, &conv_arg, STRTOI16);
                break;
            case ARG_TYPE_UINT16:
                result = strtox(arg, &conv_arg, STRTOUI16);
                break;
            case ARG_TYPE_INT32:
                result = strtox(arg, &conv_arg, STRTOI32);
                break;
            case ARG_TYPE_UINT32:
                result = strtox(arg, &conv_arg, STRTOUI32);
                break;
            case ARG_TYPE_INT64:
                result = strtox(arg, &conv_arg, STRTOI64);
                break;
            case ARG_TYPE_UINT64:
                result = strtox(arg, &conv_arg, STRTOUI64);
                break;
#endif
        }

        if (result == 1) {
            optparse_error("Argument not valid: \"%s\"\n", arg);
        } else if (result == -1) {
            optparse_error("Argument too long: \"%s\"\n", arg);
        }
    }

    // Store (type-converted) option-argument
    if (opt->arg_dest) {
        switch (opt->arg_type) {
            case ARG_TYPE_STR:
                *(char **) opt->arg_dest = arg;
                break;
            case ARG_TYPE_CHAR:
            case ARG_TYPE_SCHAR:
            case ARG_TYPE_UCHAR:
                memcpy(opt->arg_dest, &conv_arg, 1);
                break;
            case ARG_TYPE_SHRT:
            case ARG_TYPE_USHRT:
                memcpy(opt->arg_dest, &conv_arg, sizeof(short));
                break;
            case ARG_TYPE_INT:
            case ARG_TYPE_UINT:
                memcpy(opt->arg_dest, &conv_arg, sizeof(int));
                break;
            case ARG_TYPE_LONG:
            case ARG_TYPE_ULONG:
                memcpy(opt->arg_dest, &conv_arg, sizeof(long));
                break;
            case ARG_TYPE_LLONG:
            case ARG_TYPE_ULLONG:
                memcpy(opt->arg_dest, &conv_arg, sizeof(long long));
                break;
#if OPTPARSE_FLOATING_POINT_SUPPORT
            case ARG_TYPE_FLT:
                memcpy(opt->arg_dest, &conv_arg, sizeof(float));
                break;
            case ARG_TYPE_DBL:
                memcpy(opt->arg_dest, &conv_arg, sizeof(double));
                break;
            case ARG_TYPE_LDBL:
                memcpy(opt->arg_dest, &conv_arg, sizeof(long double));
                break;
#endif
            case ARG_TYPE_BOOL:
                memcpy(opt->arg_dest, &conv_arg, sizeof(_Bool));
                break;
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
            case ARG_TYPE_INT8:
            case ARG_TYPE_UINT8:
                memcpy(opt->arg_dest, &conv_arg, sizeof(int8_t));
                break;
            case ARG_TYPE_INT16:
            case ARG_TYPE_UINT16:
                memcpy(opt->arg_dest, &conv_arg, sizeof(int16_t));
                break;
            case ARG_TYPE_INT32:
            case ARG_TYPE_UINT32:
                memcpy(opt->arg_dest, &conv_arg, sizeof(int32_t));
                break;
            case ARG_TYPE_INT64:
            case ARG_TYPE_UINT64:
                memcpy(opt->arg_dest, &conv_arg, sizeof(int64_t));
                break;
#endif
        }
    }

    // Call option's function
    if (opt->function) {
        switch (opt->function_arg) {
            case FUNCTION_ARG_AUTO:
                if (opt->arg_type) {
                    goto converted_arg;
                } else if (opt->arg) {
                    goto string_arg;
                } else {
                    goto void_arg;
                }
                break;
            case FUNCTION_ARG_VOID:
                void_arg:
                ((void (*)(void)) opt->function)();
                break;
            case FUNCTION_ARG_OPTION_ARG:
                goto string_arg;
                break;
            case FUNCTION_ARG_CONVERTED_OPTION_ARG:
                converted_arg:
                switch (opt->arg_type) {
                    case ARG_TYPE_STR:
                    string_arg:
                        ((void (*)(char *)) opt->function)(arg);
                        break;
                    case ARG_TYPE_CHAR:
                        ((void (*)(char)) opt->function)(conv_arg.t_char);
                        break;
                    case ARG_TYPE_SCHAR:
                        ((void (*)(signed char)) opt->function)(conv_arg.t_schar);
                        break;
                    case ARG_TYPE_UCHAR:
                        ((void (*)(unsigned char)) opt->function)(conv_arg.t_uchar);
                        break;
                    case ARG_TYPE_SHRT:
                        ((void (*)(short)) opt->function)(conv_arg.t_shrt);
                        break;
                    case ARG_TYPE_USHRT:
                        ((void (*)(unsigned short)) opt->function)(conv_arg.t_ushrt);
                        break;
                    case ARG_TYPE_INT:
                        ((void (*)(int)) opt->function)(conv_arg.t_int);
                        break;
                    case ARG_TYPE_UINT:
                        ((void (*)(unsigned int)) opt->function)(conv_arg.t_uint);
                        break;
                    case ARG_TYPE_LONG:
                        ((void (*)(long)) opt->function)(conv_arg.t_long);
                        break;
                    case ARG_TYPE_ULONG:
                        ((void (*)(unsigned long)) opt->function)(conv_arg.t_ulong);
                        break;
                    case ARG_TYPE_LLONG:
                        ((void (*)(long long)) opt->function)(conv_arg.t_llong);
                        break;
                    case ARG_TYPE_ULLONG:
                        ((void (*)(unsigned long long)) opt->function)(conv_arg.t_ullong);
                        break;
#if OPTPARSE_FLOATING_POINT_SUPPORT
                    case ARG_TYPE_FLT:
                        ((void (*)(float)) opt->function)(conv_arg.t_flt);
                        break;
                    case ARG_TYPE_DBL:
                        ((void (*)(double)) opt->function)(conv_arg.t_dbl);
                        break;
                    case ARG_TYPE_LDBL:
                        ((void (*)(long double)) opt->function)(conv_arg.t_ldbl);
                        break;
#endif
                    case ARG_TYPE_BOOL:
                        ((void (*)(_Bool)) opt->function)(conv_arg.t_bool);
                        break;
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
                    case ARG_TYPE_INT8:
                        ((void (*)(int8_t)) opt->function)(conv_arg.t_int8);
                        break;
                    case ARG_TYPE_UINT8:
                        ((void (*)(uint8_t)) opt->function)(conv_arg.t_uint8);
                        break;
                    case ARG_TYPE_INT16:
                        ((void (*)(int16_t)) opt->function)(conv_arg.t_int16);
                        break;
                    case ARG_TYPE_UINT16:
                        ((void (*)(uint16_t)) opt->function)(conv_arg.t_uint16);
                        break;
                    case ARG_TYPE_INT32:
                        ((void (*)(int32_t)) opt->function)(conv_arg.t_int32);
                        break;
                    case ARG_TYPE_UINT32:
                        ((void (*)(uint32_t)) opt->function)(conv_arg.t_uint32);
                        break;
                    case ARG_TYPE_INT64:
                        ((void (*)(int64_t)) opt->function)(conv_arg.t_int64);
                        break;
                    case ARG_TYPE_UINT64:
                        ((void (*)(uint64_t)) opt->function)(conv_arg.t_uint64);
                        break;
#endif
                }
                break;
        }
    }
}

#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
// Prints an option's name ("-o, --option") to a buffer.
static void bprint_option_name(char *buffer, struct optparse_opt *opt) {
    if (opt->short_name) {
        bprintf(buffer, "-%c", opt->short_name);
        if (opt->long_name) {
            bprintf(buffer, ", ");
        }
    }
    if (opt->long_name) {
        bprintf(buffer, "--%s", opt->long_name);
    }
}

// Checks an option for mutual exclusivity violations and quits on error.
static void check_mutual_exclusivity(struct optparse_opt *opt) {
    static struct optparse_opt *exclusive_opts[MUTUALLY_EXCLUSIVE_GROUPS_MAX];

    if (opt->group > 0 && opt->group
            < MUTUALLY_EXCLUSIVE_GROUPS_MAX) {
        if (exclusive_opts[opt->group]) {
            char buffer1[PRINT_BUFFER_SIZE] = { 0 };
            char buffer2[PRINT_BUFFER_SIZE] = { 0 };
            bprint_option_name(buffer1, exclusive_opts[opt->group]);
            bprint_option_name(buffer2, opt);
            optparse_error("Options %s and %s are mutually exclusive.\n",
                buffer1, buffer2);
        } else {
            exclusive_opts[opt->group] = opt; // Add to
        }
    }
}
#endif

// Returns 1 if the option requires at least 1 argument.
static inline int arg_required(struct optparse_opt *opt) {
    if (opt->arg && opt->arg[0] != '[') {
        return 1;
    }

    return 0;
}

#if OPTPARSE_LONG_OPTIONS
// Identifies and executes a single known long option.
static void execute_long_option(char *long_name,
    struct optparse_opt options[]) {
    char *arg = strchr(long_name, '=');
    if (arg) {
        *arg++ = '\0';
    }

    struct optparse_opt *opt = options;
    while (opt->short_name != (char) END_OF_OPTIONS) {
        if (opt->long_name && strcmp(long_name, opt->long_name) == 0) {
#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
            check_mutual_exclusivity(opt);
#endif
            if (arg) {
                if (!opt->arg) {
                    optparse_error("Unwanted option-argument: \"%s\"\n", arg);
                }
            } else if (opt->arg && opt->arg[0] != '[') {
                arg = args[++args_index];
                if (arg == NULL) {
                    optparse_error("Option \"--%s\": argument required\n",
                        long_name);
                }
            }

            execute_option(opt, arg);
            return;
        }

        opt++;
    }

    optparse_error("Unknown option: \"--%s\"\n", long_name);
}
#endif

// Identifies and executes a group of known short options.
static void execute_short_option(char *option_group,
    struct optparse_opt options[]) {
    char *c = option_group;
    while (*++c != '\0') {
        char *arg = c + 1;
        if (*arg == '\0') {
            arg = NULL;
        }

        struct optparse_opt *opt = options;
        while (opt->short_name != (char) END_OF_OPTIONS) {
            if (*c == opt->short_name) {
#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
                check_mutual_exclusivity(opt);
#endif
                if (arg) {
                    if (!opt->arg) {
                        arg = NULL;
                    }
                } else if (opt->arg && opt->arg[0] != '[') {
                    arg = args[++args_index];
                    if (arg == NULL) {
                        optparse_error("Option -%c: argument required\n", *c);
                    }
                }

                execute_option(opt, arg);
                if (arg) {
                    return;
                } else {
                    goto next;
                }
            }

            opt++;
        }

        if (option_group[2] != '\0') {
            optparse_error("Unknown option: -%c (in argument \"%s\")\n", *c,
                option_group);
        } else {
            optparse_error("Unknown option: -%c\n", *c);
        }

        next:
        ;
    }
}

// Parses a command's command line options.
// After parsing, only operands remain in argv.
static void parse(int *argc, char ***argv, struct optparse_cmd *cmd) {
    args = *argv;
    args_index = 1;
    *argc = 1; // to keep argv[0]

    int ignore_options = 0;
    while (args[args_index] != NULL) {
        if (!ignore_options && args[args_index][0] == '-') {
            if (args[args_index][1] == '-') {
                if (args[args_index][2] == '\0') { // Option "--"
                    ignore_options = 1;
                    continue;
#if OPTPARSE_LONG_OPTIONS
                } else {
                    execute_long_option(args[args_index] + 2, cmd->options);
#endif
                }
            } else {
                execute_short_option(args[args_index], cmd->options);
            }
        } else {
#if OPTPARSE_SUBCOMMANDS
            if (cmd->subcommands) {
                struct optparse_cmd *subcmd = cmd->subcommands;
                while (subcmd->name != END_OF_SUBCOMMANDS) {
                    if (strcmp(args[args_index], subcmd->name) == 0) {
                        // Remove previous arguments, including the subcommand,
                        // from argv (args will be set in the next iteration)
                        do {
                            (*argv)[(*argc)++] = args[++args_index];
                        } while (args[args_index]);
                        (*argv)[*argc] = NULL;

                        // Continue parsing with the subcommand
                        parse(argc, argv, subcmd);

                        return;
                    }
                    subcmd++;
                }

                optparse_error("Unknown subcommand: \"%s\"\n", args[1]);
            } else
#endif
                // Treat argument as an operand, adding it to the new argv
                (*argv)[(*argc)++] = args[args_index];
        }

        if (args[args_index] != NULL) { // Could be NULL due to optparse_shift()
            args_index++;
        }
    }

    (*argv)[*argc] = NULL;

    // Run command's function on remaining operands
    if (cmd->function) {
        args_index = 0;
        cmd->function(*argc, *argv);
    }

}

/// "Help screen" functions ----------------------------------------------------

#if OPTPARSE_HELP_WORD_WRAP
// Prints a string using automatic word-wrapping.
static void blockprint(FILE *stream, char *str, int first_line_indent,
    int indent, int end) {
    if (str == NULL || str[0] == '\0') {
        fprintf(stream, "\n");
        return;
    }

    int first_line_printed = 0;
    int width = end - indent;

    while (1) {
        // Indentation
        if (first_line_printed) {
            fprintf(stream, "%*s", indent, "");
        } else {
            width = end - first_line_indent;
        }

        int n = 0; // Number of characters to be printed on the current line

        while (n <= width) {
            // Print early when encountering a newline character
            if (str[n] == '\n') {
                fprintf(stream, "%.*s", ++n, str);
                goto next;
            }
            // Print and finish if string is shorter than width
            if (str[n] == '\0') {
                fprintf(stream, "%s\n", str);
                return;
            }
            n++;
        }
        n--;

        // Make sure not to truncate the last word
        while (n > 0 && str[n] != ' ') {
            n--;
        }
        if (n == 0) {
            n = width;
        }

        fprintf(stream, "%.*s\n", n, str);

        next:
        str += n;
        // Remove leading spaces before printing the next line
        while (str[0] == ' ') {
            str++;
        }

        if (first_line_printed == 0) {
            first_line_printed = 1;
            width = end - indent;
        }
    }
}
#endif

// Prints an option's usage information ("-a ARG") to a buffer.
static void bprint_option_usage(char *buffer, struct optparse_opt *opt) {
    if (opt->short_name) {
        bprintf(buffer, "-%c", opt->short_name);
    } else {
        bprintf(buffer, "--%s", opt->long_name);
    }

    if (opt->arg) {
        if (opt->arg[0] == '[' && opt->long_name) {
            bprintf(buffer, "[=%s", opt->arg + 1);
        } else {
            bprintf(buffer, " %s", opt->arg);
        }
    }
}

#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
// Prints all of a specified group's mutually exlusive options to a buffer.
// Assumes there are at least 2 group members.
static void bprint_exclusive_option_group(char *buffer,
    struct optparse_opt *opt, int *printed_groups) {
    int group_index = opt->group;

    // Don't print groups that have already been printed
    if (printed_groups[group_index]) {
        return;
    }

    bprintf(buffer, " [");
    bprint_option_usage(buffer, opt);

    while ((++opt)->short_name != (char) END_OF_OPTIONS) {
        if (opt->group == group_index) {
            bprintf(buffer, "|");
            bprint_option_usage(buffer, opt);
        }
    }

    bprintf(buffer, "]");
    printed_groups[group_index] = 1; // Mark group as printed
}
#endif

// Prints a command's usage.
#if OPTPARSE_SUBCOMMANDS
static void print_usage(FILE *stream, struct optparse_cmd *cmd,
    char **subcmd_chain) {
#else
static void print_usage(FILE *stream, struct optparse_cmd *cmd) {
#endif
#if OPTPARSE_HELP_LETTER_CASE == 0
    printf("Usage:");
#elif OPTPARSE_HELP_LETTER_CASE == 1
    printf("usage:");
#elif OPTPARSE_HELP_LETTER_CASE == 2
    printf("USAGE:");
#endif

    char buffer[PRINT_BUFFER_SIZE] = { 0 };

    // If a custom usage string is provided, print it and return
    if (cmd->usage) {
        bprintf(buffer, " %s\n", cmd->usage);
        goto print;
    }

    // Print command name(s)
    bprintf(buffer, " %s", optparse_main_cmd->name);
#if OPTPARSE_SUBCOMMANDS
    if (subcmd_chain) {
        while (*subcmd_chain) {
            bprintf(buffer, " %s", *subcmd_chain);
            subcmd_chain++;
        }
    }
#endif

    // Print command's options
    if (cmd->options) {
#if OPTPARSE_HELP_USAGE_STYLE == 1
#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
        int printed_groups[MUTUALLY_EXCLUSIVE_GROUPS_MAX] = { 0 };
#endif

        struct optparse_opt *opt = cmd->options;
        while (opt->short_name != (char) END_OF_OPTIONS) {
#if OPTPARSE_HIDDEN_OPTIONS
            // Don't print options marked as "hidden"
            if (opt->hidden) {
                opt++;
                continue;
            }
#endif

#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
            if (opt->group) {
                bprint_exclusive_option_group(buffer, opt, printed_groups);
            } else {
                bprintf(buffer, " [");
                bprint_option_usage(buffer, opt);
                bprintf(buffer, "]");
            }
#else
            bprintf(buffer, " [");
            bprint_option_usage(buffer, opt);
            bprintf(buffer, "]");
#endif
            opt++;
        }
#else
        bprintf(buffer, "[" OPTPARSE_HELP_USAGE_OPTIONS_STRING "]");
#endif
    }

    // Print command's operands
    if (cmd->operands) {
        bprintf(buffer, " %s", cmd->operands);
    }

    print:
    blockprint(stream, buffer, 7, 7, OPTPARSE_HELP_MAX_LINE_WIDTH);
}

// Prints a set of options (names, arguments, descriptions).
// Returns the calculated divider width.
static void print_options(FILE *stream, struct optparse_opt options[]) {
    struct optparse_opt *opt = options;

    // Determine divider width -------------------------------------------------
    // (see section "Print" below to know where the numbers come from)
    int divider_width = 0;
    while (opt->short_name != (char) END_OF_OPTIONS) {
#if OPTPARSE_HIDDEN_OPTIONS
        if (opt->hidden) {
            opt++;
            continue;
        }
#endif

        int len = OPTPARSE_HELP_INDENTATION_WIDTH * 2;

        if (opt->short_name) {
            len += 2;
#if OPTPARSE_LONG_OPTIONS
            if (opt->long_name) {
                len += 2;
            }
        } else if (OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS) {
            len += 4;
#endif
        }

#if OPTPARSE_LONG_OPTIONS
        if (opt->long_name) {
            len += 2 + strlen(opt->long_name);
        }
#endif

        // Snap divider to options
        if (len > divider_width && len <= OPTPARSE_HELP_MAX_DIVIDER_WIDTH) {
            divider_width = len;
        }

        if (opt->arg) {
            if (opt->arg[0] == '[') {
                if (opt->long_name) {
                    len += 1 + strlen(opt->arg);
                } else {
                    len += strlen(opt->arg);
                }
            } else {
                len += 1 + strlen(opt->arg);
            }
        }

        // Snap divider to arguments
        if (len > divider_width && len <= OPTPARSE_HELP_MAX_DIVIDER_WIDTH) {
            divider_width = len;
        }
        opt++;
    }

    // Print -------------------------------------------------------------------
    opt = options;
    while (opt->short_name != (char) END_OF_OPTIONS) {
#if OPTPARSE_HIDDEN_OPTIONS
        if (opt->hidden) {
            opt++;
            continue;
        }
#endif

        int len = OPTPARSE_HELP_INDENTATION_WIDTH * 2;

        fprintf(stream, "%*c", OPTPARSE_HELP_INDENTATION_WIDTH, ' ');

        // Print option's short name
        if (opt->short_name) {
            fprintf(stream, "-%c", opt->short_name);
            len += 2;
#if OPTPARSE_LONG_OPTIONS
            if (opt->long_name) {
                fprintf(stream, ", ");
                len += 2;
            }
        } else if (OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS) {
            fprintf(stream, "    ");
            len += 4;
#endif
        }

#if OPTPARSE_LONG_OPTIONS
        // Print option's long name
        if (opt->long_name) {
            fprintf(stream, "--%s", opt->long_name);
            len += 2 + strlen(opt->long_name);
        }
#endif

        // Print option's arguments
        if (opt->arg) {
            if (opt->arg[0] == '[') {
                if (opt->long_name) {
                    fprintf(stream, "[=%s", opt->arg + 1);
                    len += 1 + strlen(opt->arg);
                } else {
                    fprintf(stream, "%s", opt->arg);
                    len += strlen(opt->arg);
                }
            } else {
                fprintf(stream, " %s", opt->arg);
                len += 1 + strlen(opt->arg);
            }
        }

        fprintf(stream, "%*c", OPTPARSE_HELP_INDENTATION_WIDTH, ' ');

        // Adjust spacing before printing option's description
        if (len < divider_width) {
            for (int i = 0; i < divider_width - len; i++) {
                fprintf(stream, " ");
            }
        } else if (len > OPTPARSE_HELP_MAX_LINE_WIDTH) {
            fprintf(stream, "\n%*c", divider_width, ' ');
        } else if (!OPTPARSE_HELP_FLOATING_DESCRIPTIONS
            && len > divider_width) {
            fprintf(stream, "\n");
            for (int i = 0; i < divider_width; i++) {
                fprintf(stream, " ");
            }
        }

        // Print option's description
        if (opt->description) {
#if !OPTPARSE_HELP_WORD_WRAP
            fprintf(stream, "%s\n", opt->description);
#else
#if OPTPARSE_HELP_FLOATING_DESCRIPTIONS
            int first_line_indent;
            if (len < divider_width || len > OPTPARSE_HELP_MAX_LINE_WIDTH) {
                first_line_indent = divider_width;
            } else {
                first_line_indent = len;
            }
            blockprint(stream, opt->description, first_line_indent,
                divider_width, OPTPARSE_HELP_MAX_LINE_WIDTH);
#else
            blockprint(stream, opt->description, divider_width, divider_width,
                OPTPARSE_HELP_MAX_LINE_WIDTH);
#endif
#endif
        } else {
            fprintf(stream, "\n");
        }

        opt++;
    }
}

#if OPTPARSE_SUBCOMMANDS
// Prints a list of a command's subcommands.
static void print_subcommands(FILE *stream, struct optparse_cmd *subcmd,
    int divider_width) {
    while (subcmd->name != END_OF_SUBCOMMANDS) {
        char buffer[PRINT_BUFFER_SIZE] = { 0 };
        bprintf(buffer, "%*c%s%*c", OPTPARSE_HELP_INDENTATION_WIDTH, ' ',
            subcmd->name, OPTPARSE_HELP_INDENTATION_WIDTH, ' ');
        fprintf(stream, buffer);

        size_t len = strlen(buffer);
        if (len < divider_width) {
            len = divider_width - len;
            fprintf(stream, "%*c", len, ' ');
        }
        blockprint(stream, subcmd->about, len, divider_width,
            OPTPARSE_HELP_MAX_LINE_WIDTH);

        subcmd++;
    }
}
#endif

// Prints a command's complete help information: about, usage, description,
// options, subcommands.
// cmd_chain: a NULL-terminated array that contains a valid subcommand chain
#if OPTPARSE_SUBCOMMANDS
static void print_help(FILE *stream, struct optparse_cmd *cmd,
    char **subcmd_chain) {
#else
static void print_help(FILE *stream, struct optparse_cmd *cmd) {
#endif
    if (cmd->about) {
        blockprint(stream, cmd->about, 0, 0, OPTPARSE_HELP_MAX_LINE_WIDTH);
    }

    // Print command's usage
#if OPTPARSE_SUBCOMMANDS
    print_usage(stream, cmd, subcmd_chain);
#else
    print_usage(stream, cmd);
#endif

    // Print command's description
    if (cmd->description) {
        fprintf(stream, "\n");
        blockprint(stream, cmd->description, 0, 0,
            OPTPARSE_HELP_MAX_LINE_WIDTH);
    }

    // Print command's options
    if (cmd->options) {
#if OPTPARSE_HELP_LETTER_CASE == 0
        fprintf(stream, "\nOptions:\n");
#elif OPTPARSE_HELP_LETTER_CASE == 1
        fprintf(stream, "\noptions:\n");
#elif OPTPARSE_HELP_LETTER_CASE == 2
        fprintf(stream, "\nOPTIONS:\n");
#endif
        print_options(stream, cmd->options);
    }

#if OPTPARSE_SUBCOMMANDS
    // Print list of subcommands
    if (cmd->subcommands) {
#if OPTPARSE_HELP_LETTER_CASE == 0
        printf("\nCommands:\n");
#elif OPTPARSE_HELP_LETTER_CASE == 1
        printf("\ncommands:\n");
#elif OPTPARSE_HELP_LETTER_CASE == 2
        printf("\nCOMMANDS:\n");
#endif

        // Determine subcommand list's divider width
        int divider_width = 0;
        struct optparse_cmd *subcmd = cmd->subcommands;
        while (subcmd->name != END_OF_SUBCOMMANDS) {
            int len = strlen(subcmd->name);
            if (len > divider_width) {
                divider_width = len;
            }
            subcmd++;
        }
        divider_width += 2 * OPTPARSE_HELP_INDENTATION_WIDTH;

        print_subcommands(stream, cmd->subcommands, divider_width);
    }
#endif

    exit(0);
}

#if OPTPARSE_SUBCOMMANDS
// Companion function for optparse_print_help_subcmd(); parses remaining
// operands, which must represent a valid chain of subcommands.
// Return value: the subcommand the chain leads to.
static struct optparse_cmd *find_subcmd(struct optparse_cmd *cmd, char **argv) {
    if (*argv) {
        if (cmd->subcommands) {
            struct optparse_cmd *subcmd = cmd->subcommands;
            while (subcmd->name != END_OF_SUBCOMMANDS) {
                if (strcmp(subcmd->name, *argv) == 0) {
                    return find_subcmd(subcmd, ++argv);
                }
                subcmd++;
            }
        }

        optparse_error("Unknown command: \"%s\"\n", *argv);
    } else {
        return cmd;
    }
}
#endif

/// Public functions -----------------------------------------------------------

// Parses command line options as described in the provided command structure.
void optparse_parse(struct optparse_cmd *cmd, int *argc, char ***argv) {
    optparse_main_cmd = cmd;
    if (optparse_main_cmd) {
        parse(argc, argv, optparse_main_cmd);
    }
}

// Advances the parser index by 1 and returns the next command line argument.
char *optparse_shift(void) {
    if (args == NULL) {
        return NULL;
    }

    if (args[args_index] == NULL) {
        return NULL;
    } else {
        return args[++args_index];
    }
}

// Undoes the previously called optparse_shift().
char *optparse_unshift(void) {
    if (args == NULL) {
        return NULL;
    }

    if (args_index > 0) {
        return args[--args_index];
    } else {
        return NULL;
    }
}

// Prints the main command's help.
void optparse_print_help(void) {
#if OPTPARSE_SUBCOMMANDS
    print_help(stdout, optparse_main_cmd, NULL);
#else
    print_help(stdout, optparse_main_cmd);
#endif
}

#if OPTPARSE_SUBCOMMANDS
// Prints a subcommand's help by parsing remaining operands.
void optparse_print_help_subcmd(int argc, char **argv) {
    argv++; // To ignore the program's name
    if (*argv) {
        print_help(stdout, find_subcmd(optparse_main_cmd, argv), argv);
    } else {
        print_help(stdout, optparse_main_cmd, NULL);
    }
}
#endif
