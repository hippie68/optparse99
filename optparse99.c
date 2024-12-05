// A C99+ option parser.

/*
MIT License

Copyright (c) 2022 hippie68 (https://github.com/hippie68/optparse99)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// More information, including example code, is found in the file "README.md".

#include "optparse99.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#if OPTPARSE_FLOATING_POINT_SUPPORT
#include <float.h>
#endif
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global variables
static struct optparse_cmd *optparse_main_cmd; // The command tree's root.
static char **args; // Contains the current state of argv while parsing.
static int args_index; // Keeps track of the currently parsed argument's index.
#if OPTPARSE_SUBCOMMANDS
static struct optparse_cmd *active_cmd; // Keeps track of the currently running
                                        // command.
static int subcmd_depth; // The command tree's depth (automatically calculated).
#endif
static FILE *help_stream; // The stream help information is printed to.

/// Private functions ----------------------------------------------------------

// Prints an error message and quits. Should be used for parsing errors only.
static void optparse_error(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
#if OPTPARSE_PRINT_HELP_ON_ERROR
    optparse_fprint_help(stderr, EXIT_FAILURE, false);
#endif
    exit(EXIT_FAILURE);
}

// Safely prints to a buffer of size OPTPARSE_PRINT_BUFFER_SIZE;
static int bprintf(char *buffer, const char *fmt, ...)
{
    int n;
    va_list ap;
    va_start(ap, fmt);
    size_t len = strlen(buffer);
    n = vsnprintf(buffer + len, OPTPARSE_PRINT_BUFFER_SIZE - len, fmt, ap);
    va_end(ap);
    return n;
}

#if OPTPARSE_HELP_USAGE_STYLE == 1
// Prints an option's usage information ("-a ARG") to a buffer.
static void bprint_option_usage(char *buffer, struct optparse_opt *opt)
{
    if (opt->short_name) {
        bprintf(buffer, "-%c", opt->short_name);
    }
#if OPTPARSE_LONG_OPTIONS
    else {
        bprintf(buffer, "--%s", opt->long_name);
    }
#endif

    if (opt->arg_name) {
        if (opt->arg_name[0] == '[') {
            if (opt->short_name) {
                bprintf(buffer, "%s", opt->arg_name);
            }
#if OPTPARSE_LONG_OPTIONS
            else if (opt->long_name) {
                bprintf(buffer, "[=%s", opt->arg_name + 1);
            }
#endif
        } else {
            bprintf(buffer, " %s", opt->arg_name);
        }
    }
}
#endif

// Returns a data type's size.
static int get_data_type_size(enum optparse_data_type data_type)
{
    switch (data_type) {
        case DATA_TYPE_STR:
            return sizeof (char *);
        case DATA_TYPE_CHAR:
        case DATA_TYPE_SCHAR:
        case DATA_TYPE_UCHAR:
            return 1;
        case DATA_TYPE_INT:
        case DATA_TYPE_UINT:
            return sizeof (int);
        case DATA_TYPE_LONG:
        case DATA_TYPE_ULONG:
            return sizeof (long);
        case DATA_TYPE_LLONG:
        case DATA_TYPE_ULLONG:
            return sizeof (long long);
#if OPTPARSE_FLOATING_POINT_SUPPORT
        case DATA_TYPE_FLT:
            return sizeof (float);
        case DATA_TYPE_DBL:
            return sizeof (double);
        case DATA_TYPE_LDBL:
            return sizeof (long double);
#endif
        case DATA_TYPE_BOOL:
            return sizeof (_Bool);
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
        case DATA_TYPE_INT8:
        case DATA_TYPE_UINT8:
            return 1;
        case DATA_TYPE_INT16:
        case DATA_TYPE_UINT16:
            return 2;
        case DATA_TYPE_INT32:
        case DATA_TYPE_UINT32:
            return 4;
        case DATA_TYPE_INT64:
        case DATA_TYPE_UINT64:
            return 8;
#endif
        default:
            return 0;
    }
}

#if OPTPARSE_LIST_SUPPORT
// Converts a non-literal string that has the form of a list into an array of
// specified data type. The string will be altered and cannot be used anymore in
// its original form. The array's data type must match the specified data type.
// If the list contains items, the array's memory will be dynamically
// allocated - free() should be called if the memory is no longer needed.
// To avoid compiler warnings, the array pointer can be explicitly cast to
// void *: "strtoarr(..., (void *) &array, ...);".
// Return value: the number of list items stored in the array.
static size_t strtoarr(char *string, void **array, char *delim,
    enum optparse_data_type data_type)
{
    if (string == NULL || delim == NULL) {
        *array = NULL;
        return 0;
    }

    // Get temporary array size.
    size_t array_size = 1;
    char *c = string;
    while (*c != '\0') {
        for (size_t i = 0; i < strlen(delim); i++) {
            if (*c == delim[i]) {
                array_size++;
                break;
            }
        }
        c++;
    }

    int data_type_size = get_data_type_size(data_type);

    // Allocate temporary array size.
    *array = malloc(array_size * data_type_size);
    if (*array == NULL) {
        optparse_error("Out of memory.\n");
    }

    // Convert list items to specified data type and store them in the array.
    array_size = 0;
    char *list_item = strtok(string, delim);
    while (list_item != NULL) {
        int ret = strtox(list_item, ((char *) *array) + array_size
            * data_type_size, data_type);
        if (ret) {
            free(*array);
            if (ret == 1) {
                optparse_error("List item not valid: \"%s\"\n", list_item);
            } else if (ret == -1) {
                optparse_error("List item out of range: \"%s\"\n", list_item);
            }
        }

        array_size++;
        list_item = strtok(NULL, delim);
    }

    // Allocate final array size.
    void *ret = realloc(*array, array_size * data_type_size);
    if (ret == NULL && array_size != 0) {
        free(*array);
        optparse_error("Out of memory.\n");
    } else {
        *array = ret;
    }

    return array_size;
}
#endif

// Executes an option structure's tasks.
// arg: the option's option-argument; NULL if none provided by the user.
static void execute_option(struct optparse_opt *opt, char *arg)
{
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
    } conv_arg;              // Used to temporarily hold a single type-converted
                             // option-argument.
#if OPTPARSE_LIST_SUPPORT
    void *list_array = NULL; // Used to temporarily or permanently store a
                             // type-converted list.
    size_t list_size = 0;    // The converted list's size.
    char *oarg = arg;        // Optionally used to back up the original
                             // option-argument.
#endif

    // Set option's flag.
    if (opt->flag != NULL) {
        switch (opt->flag_type) {
            case FLAG_TYPE_SET_TRUE:
                *(int *) opt->flag = 1;
                break;
            case FLAG_TYPE_SET_FALSE:
                *(int *) opt->flag = 0;
                break;
            case FLAG_TYPE_INCREMENT:
                *(int *) opt->flag += 1;
                break;
            case FLAG_TYPE_DECREMENT:
                *(int *) opt->flag -= 1;
                break;
        }
    }

    // Type-convert the option-argument.
    if (arg) {
#if OPTPARSE_LIST_SUPPORT
        if (opt->arg_delim) { // Option-argument is a list.
            // Back up the original option-argument, if necessary.
            if (opt->function && opt->function_type == FUNCTION_TYPE_OARG) {
                oarg = malloc(strlen(arg) + 1);
                if (oarg == NULL) {
                    optparse_error("Out of memory.\n");
                }
                strcpy(oarg, arg);
            }

            list_size = strtoarr(arg, &list_array, opt->arg_delim,
                opt->arg_data_type);
        } else
#endif
        if (opt->arg_data_type) { // Option-argument is a single value.
            int ret;
            ret = strtox(arg, &conv_arg, opt->arg_data_type);
            if (ret == 1) {
                optparse_error("Argument not valid: \"%s\"\n", arg);
            } else if (ret == -1) {
                optparse_error("Value out of range: \"%s\"\n", arg);
            }
        }

        // Store the (type-converted) option-argument...
        if (opt->arg_storage) {
#if OPTPARSE_LIST_SUPPORT
            if (opt->arg_delim) {
                *(void **) opt->arg_storage = list_array;
            } else
#endif
            if (opt->arg_data_type == DATA_TYPE_STR) {
                *(char **) opt->arg_storage = arg;
            } else {
                size_t n = get_data_type_size(opt->arg_data_type);
                memcpy(opt->arg_storage, &conv_arg, n);
            }
        }
    }

#if OPTPARSE_LIST_SUPPORT
    // Store the storage size.
    if (opt->arg_delim && opt->arg_storage_size) {
        *opt->arg_storage_size = list_size;
    }
#endif

    // Call option's function.
    if (opt->function) {
        switch (opt->function_type) {
            case FUNCTION_TYPE_AUTO:
                if (opt->arg_name) {
#if OPTPARSE_LIST_SUPPORT
                    if (opt->arg_delim) {
                        goto type_targ_array;
                    } else
#endif
                    goto type_targ;
                } else {
                    goto type_void;
                }
            case FUNCTION_TYPE_OARG:
#if OPTPARSE_LIST_SUPPORT
                ((void (*)(char *)) opt->function)(oarg);
#else
                ((void (*)(char *)) opt->function)(arg);
#endif
                break;
            case FUNCTION_TYPE_TARG:
                type_targ:
                switch (opt->arg_data_type) {
                    case DATA_TYPE_STR:
                        ((void (*)(char *)) opt->function)(arg);
                        break;
                    case DATA_TYPE_CHAR:
                        ((void (*)(char)) opt->function)(conv_arg.t_char);
                        break;
                    case DATA_TYPE_SCHAR:
                        ((void (*)(signed char)) opt->function)(
                            conv_arg.t_schar);
                        break;
                    case DATA_TYPE_UCHAR:
                        ((void (*)(unsigned char)) opt->function)(
                            conv_arg.t_uchar);
                        break;
                    case DATA_TYPE_SHRT:
                        ((void (*)(short)) opt->function)(conv_arg.t_shrt);
                        break;
                    case DATA_TYPE_USHRT:
                        ((void (*)(unsigned short)) opt->function)(
                            conv_arg.t_ushrt);
                        break;
                    case DATA_TYPE_INT:
                        ((void (*)(int)) opt->function)(conv_arg.t_int);
                        break;
                    case DATA_TYPE_UINT:
                        ((void (*)(unsigned int)) opt->function)(
                            conv_arg.t_uint);
                        break;
                    case DATA_TYPE_LONG:
                        ((void (*)(long)) opt->function)(conv_arg.t_long);
                        break;
                    case DATA_TYPE_ULONG:
                        ((void (*)(unsigned long)) opt->function)(
                            conv_arg.t_ulong);
                        break;
                    case DATA_TYPE_LLONG:
                        ((void (*)(long long)) opt->function)(conv_arg.t_llong);
                        break;
                    case DATA_TYPE_ULLONG:
                        ((void (*)(unsigned long long)) opt->function)(
                            conv_arg.t_ullong);
                        break;
#if OPTPARSE_FLOATING_POINT_SUPPORT
                    case DATA_TYPE_FLT:
                        ((void (*)(float)) opt->function)(conv_arg.t_flt);
                        break;
                    case DATA_TYPE_DBL:
                        ((void (*)(double)) opt->function)(conv_arg.t_dbl);
                        break;
                    case DATA_TYPE_LDBL:
                        ((void (*)(long double)) opt->function)(
                            conv_arg.t_ldbl);
                        break;
#endif
                    case DATA_TYPE_BOOL:
                        ((void (*)(_Bool)) opt->function)(conv_arg.t_bool);
                        break;
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
                    case DATA_TYPE_INT8:
                        ((void (*)(int8_t)) opt->function)(conv_arg.t_int8);
                        break;
                    case DATA_TYPE_UINT8:
                        ((void (*)(uint8_t)) opt->function)(conv_arg.t_uint8);
                        break;
                    case DATA_TYPE_INT16:
                        ((void (*)(int16_t)) opt->function)(conv_arg.t_int16);
                        break;
                    case DATA_TYPE_UINT16:
                        ((void (*)(uint16_t)) opt->function)(conv_arg.t_uint16);
                        break;
                    case DATA_TYPE_INT32:
                        ((void (*)(int32_t)) opt->function)(conv_arg.t_int32);
                        break;
                    case DATA_TYPE_UINT32:
                        ((void (*)(uint32_t)) opt->function)(conv_arg.t_uint32);
                        break;
                    case DATA_TYPE_INT64:
                        ((void (*)(int64_t)) opt->function)(conv_arg.t_int64);
                        break;
                    case DATA_TYPE_UINT64:
                        ((void (*)(uint64_t)) opt->function)(conv_arg.t_uint64);
                        break;
#endif
                }
                break;
#if OPTPARSE_LIST_SUPPORT
            case FUNCTION_TYPE_OARG_ARRAY:
                {
                    char **array = NULL;
                    size_t size = strtoarr(oarg, (void *) &array,
                        opt->arg_delim, DATA_TYPE_STR);
                    ((void (*)(size_t, char **)) opt->function)(size, array);
                    if (array) {
                        free(array);
                    }
                }
                break;
            case FUNCTION_TYPE_TARG_ARRAY:
                type_targ_array:
                switch (opt->arg_data_type) {
                    case DATA_TYPE_STR:
                        ((void (*)(size_t, char **)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_CHAR:
                        ((void (*)(size_t, char *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_SCHAR:
                        ((void (*)(size_t, signed char *)) opt->function)(
                            list_size, list_array);
                        break;
                    case DATA_TYPE_UCHAR:
                        ((void (*)(size_t, unsigned char *)) opt->function)(
                            list_size, list_array);
                        break;
                    case DATA_TYPE_SHRT:
                        ((void (*)(size_t, short *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_USHRT:
                        ((void (*)(size_t, unsigned short *)) opt->function)(
                            list_size, list_array);
                        break;
                    case DATA_TYPE_INT:
                        ((void (*)(size_t, int *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_UINT:
                        ((void (*)(size_t, unsigned int *)) opt->function)(
                            list_size, list_array);
                        break;
                    case DATA_TYPE_LONG:
                        ((void (*)(size_t, long *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_ULONG:
                        ((void (*)(size_t, unsigned long *)) opt->function)(
                            list_size, list_array);
                        break;
                    case DATA_TYPE_LLONG:
                        ((void (*)(size_t, long long *)) opt->function)(
                            list_size, list_array);
                        break;
                    case DATA_TYPE_ULLONG:
                        ((void (*)(size_t, unsigned long long *)) opt->function)
                            (list_size, list_array);
                        break;
#if OPTPARSE_FLOATING_POINT_SUPPORT
                    case DATA_TYPE_FLT:
                        ((void (*)(size_t, float *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_DBL:
                        ((void (*)(size_t, double *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_LDBL:
                        ((void (*)(size_t, long double *)) opt->function)(
                            list_size, list_array);
                        break;
#endif
                    case DATA_TYPE_BOOL:
                        ((void (*)(size_t, _Bool *)) opt->function)(list_size,
                            list_array);
                        break;
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
                    case DATA_TYPE_INT8:
                        ((void (*)(size_t, int8_t *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_UINT8:
                        ((void (*)(size_t, uint8_t *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_INT16:
                        ((void (*)(size_t, int16_t *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_UINT16:
                        ((void (*)(size_t, uint16_t *)) opt->function)(
                            list_size, list_array);
                        break;
                    case DATA_TYPE_INT32:
                        ((void (*)(size_t, int32_t *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_UINT32:
                        ((void (*)(size_t, uint32_t *)) opt->function)(
                            list_size, list_array);
                        break;
                    case DATA_TYPE_INT64:
                        ((void (*)(size_t, int64_t *)) opt->function)(list_size,
                            list_array);
                        break;
                    case DATA_TYPE_UINT64:
                        ((void (*)(size_t, uint64_t *)) opt->function)(
                            list_size, list_array);
                        break;
#endif
                }
                break;
#endif
            case FUNCTION_TYPE_VOID:
                type_void:
                ((void (*)(void)) opt->function)();
                break;
        }
    }

#if OPTPARSE_LIST_SUPPORT
    // List-related clean-up.
    if (opt->arg_delim && !opt->arg_storage) {
        free(list_array);
    }
    if (oarg != arg) {
        free(oarg);
    }
#endif
}

#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
// Prints a mutually exclusive option's name ("-o, --option") to a buffer.
static void bprint_option_name(char *buffer, struct optparse_opt *opt)
{
    if (opt->short_name) {
        bprintf(buffer, "-%c", opt->short_name);
#if OPTPARSE_LONG_OPTIONS
        if (opt->long_name) {
            bprintf(buffer, ", ");
        }
#endif
    }

#if OPTPARSE_LONG_OPTIONS
    if (opt->long_name) {
        bprintf(buffer, "--%s", opt->long_name);
    }
#endif
}

// Checks an option for mutual exclusivity violations and quits on error.
static void check_mutual_exclusivity(struct optparse_opt *opt)
{
    static struct optparse_opt *exclusive_opts[OPTPARSE_MUTUALLY_EXCLUSIVE_GROUPS_MAX];

    if (opt->group > 0 && opt->group
            < OPTPARSE_MUTUALLY_EXCLUSIVE_GROUPS_MAX) {
        if (exclusive_opts[opt->group]) {
            char buffer1[OPTPARSE_PRINT_BUFFER_SIZE];
            buffer1[0] = '\0';
            char buffer2[OPTPARSE_PRINT_BUFFER_SIZE];
            buffer2[0] = '\0';
            bprint_option_name(buffer1, exclusive_opts[opt->group]);
            bprint_option_name(buffer2, opt);
            optparse_error("Options %s and %s are mutually exclusive.\n",
                buffer1, buffer2);
        } else {
            exclusive_opts[opt->group] = opt;
        }
    }
}
#endif

#if OPTPARSE_LONG_OPTIONS
// Identifies and executes a single known long option.
static void execute_long_option(char *long_name, struct optparse_opt options[])
{
    if (options == NULL) {
        goto unknown_option;
    }

#if OPTPARSE_ATTACHED_OPTION_ARGUMENTS
    char *arg = strchr(long_name, '=');
    if (arg) {
        *arg++ = '\0';
    }
#else
    char *arg = NULL;
#endif

    struct optparse_opt *opt = options;
    while (opt->short_name != (char) END_OF_OPTIONS) {
        if (opt->long_name && strcmp(long_name, opt->long_name) == 0) {
#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
            check_mutual_exclusivity(opt);
#endif
            if (arg) {
                if (!opt->arg_name) {
                    optparse_error("Unwanted option-argument: \"%s\"\n", arg);
                }
            } else if (opt->arg_name && opt->arg_name[0] != '[') {
                arg = args[++args_index];
                if (arg == NULL) {
                    optparse_error("Option \"--%s\" requires an argument.\n",
                        long_name);
                }
            }

            execute_option(opt, arg);
            return;
        }

        opt++;
    }

    unknown_option:
    optparse_error("Unknown option: \"--%s\"\n", long_name);
}
#endif

// Identifies and executes a group of known short options.
// option_group must not be NULL.
static void execute_short_option(char *option_group,
    struct optparse_opt options[])
{
    char *c = option_group + 1;

    if (options == NULL) {
        goto unknown_option;
    }

    while (*c != '\0') {
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
#if OPTPARSE_ATTACHED_OPTION_ARGUMENTS
                    if (!opt->arg_name) {
                        arg = NULL;
                    }
#else
                    if (opt->arg_name) {
                        optparse_error("Option -%c (in sequence \"%s\")"
                            " requires an argument.\n", *c, option_group);
                    } else {
                        arg = NULL;
                    }
#endif
                } else if (opt->arg_name && opt->arg_name[0] != '[') {
                    arg = args[++args_index];
                    if (arg == NULL) {
                        optparse_error(
                            "Option -%c requires an argument.\n", *c);
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

        unknown_option:
        if (option_group[1] != '\0' && option_group[2] != '\0') {
            optparse_error("Unknown option: \"-%c\" (in sequence \"%s\")\n", *c,
                option_group);
        } else {
            optparse_error("Unknown option: \"%s\"\n", option_group);
        }

        next:
        c++;
    }
}

// Parses a command's command line options.
// After parsing, only operands remain in argv.
static void parse(int *argc, char ***argv, struct optparse_cmd *cmd)
{
    args = *argv;
    args_index = 1;
    *argc = 1; // To keep argv[0].
#if OPTPARSE_SUBCOMMANDS
    active_cmd = cmd;
#endif

    int ignore_options = 0;
    while (args[args_index] != NULL) {
        if (!ignore_options && args[args_index][0] == '-') { // Option
            if (args[args_index][1] == '-') {
                if (args[args_index][2] == '\0') { // Stand-alone option "--"
                    ignore_options = 1;
#if OPTPARSE_LONG_OPTIONS
                } else { // Long option
                    execute_long_option(args[args_index] + 2, cmd->options);
#endif
                }
            } else { // Short option
                execute_short_option(args[args_index], cmd->options);
            }
        } else { // Operand or subcommand
#if OPTPARSE_SUBCOMMANDS
            if (cmd->subcommands) {
                struct optparse_cmd *subcmd = cmd->subcommands;
                while (subcmd->name != END_OF_SUBCOMMANDS) {
                    if (strcmp(args[args_index], subcmd->name) == 0) {
                        // Remove previous arguments, including the subcommand,
                        // from argv (args will be set in the next iteration).
                        do {
                            (*argv)[(*argc)++] = args[++args_index];
                        } while (args[args_index]);
                        (*argv)[*argc] = NULL;

                        // Continue parsing with the subcommand.
                        parse(argc, argv, subcmd);

                        return;
                    }
                    subcmd++;
                }

                optparse_error("Unknown command: \"%s\"\n", args[args_index]);
            } else
#endif
                // Treat argument as an operand, adding it to the new argv.
                (*argv)[(*argc)++] = args[args_index];
        }

        if (args[args_index] != NULL) { // Can be NULL due to optparse_shift().
            args_index++;
        }
    }

    (*argv)[*argc] = NULL;

    // Run command's function on remaining operands.
    if (cmd->function) {
        args_index = 0;
        cmd->function(*argc, *argv);
    }

}

/// Private "help screen" functions --------------------------------------------

// Prints a string using automatic word-wrapping.
// stream: the stream the string will be printed to
// str: the string to be printed
// first_line_indent: the known column at which printing starts
// indent: the indentation width (starting from line 2)
static void blockprint(FILE *stream, char *str, int first_line_indent,
    int indent, int end)
{
#if OPTPARSE_HELP_WORD_WRAP
    if (str == NULL || str[0] == '\0') {
        fputc('\n', stream);
        return;
    }

    int first_line_printed = 0;
    int width = end - indent;

    while (1) {
        // Indentation
        if (first_line_printed) {
            fprintf(stream, "%*s", indent, "");
        } else {
            if (first_line_indent > end) {
                // Indentation exceeds block width
                // Intentionally break into new line
                width = 0;
            } else {
                // Indentation does not exceed block width
                width = end - first_line_indent;
            }
        }

        int n = 0; // Number of characters to be printed on the current line

        while (n <= width) {
            // Print early when encountering a newline character.
            if (str[n] == '\n') {
                fprintf(stream, "%.*s", ++n, str);
                str += n;
                goto next;
            }
            // Print and finish if string is shorter than width.
            if (str[n] == '\0') {
                fprintf(stream, "%s\n", str);
                return;
            }
            n++;
        } // If this loop survives, word-wrapping will happen.
        n--;

        // First make sure not to truncate the last word...
        while (n > 0 && str[n] != ' ') {
            n--;
        }

        // ...then make sure not to print any trailing spaces.
        while (n > 0 && str[n - 1] == ' ') {
            n--;
        }

        // Truncate if the string is too long.
        if (n == 0) {
            n = width;
        }

        fprintf(stream, "%.*s\n", n, str);
        str += n;

        // Remove word-separating leading space before printing the next line.
        if (str[0] == ' ') {
            str++;
        }

        next:
        if (first_line_printed == 0) {
            first_line_printed = 1;
            width = end - indent;
        }
    }
#else
    fprintf(stream, "%s\n", str);
#endif
}


#if OPTPARSE_SUBCOMMANDS
// Makes a command's parent command known to all of the command's subcommands.
// Returns the maximum depth of nested commands.
static int initialize_subcommand_parents(struct optparse_cmd *cmd, int depth)
{
    if (cmd->subcommands) {
        int max_depth = 0;
        struct optparse_cmd *subcmd = cmd->subcommands;
        while (subcmd->name != END_OF_SUBCOMMANDS) {
            subcmd->_parent = cmd;
            int ret = initialize_subcommand_parents(subcmd, depth + 1);
            if (ret > max_depth) {
                max_depth = ret;
            }
            subcmd++;
        }
        return max_depth;
    } else {
        return depth;
    }
}
#endif

#if OPTPARSE_SUBCOMMANDS
// Fills an array with the names of a command's parents, including the root
// command, and the command itself, in the order in which they appear in the
// command tree.
static void build_cmd_array(struct optparse_cmd *cmd,
    char *array[subcmd_depth + 1])
{
    char *temp_array[subcmd_depth + 1];

    int i = 0;
    do {
        temp_array[i] = cmd->name;
        cmd = cmd->_parent;
        i++;
    } while (cmd);

    // Reverse
    int size = i;
    i = 0;
    while (i < size) {
        array[i] = temp_array[size - 1 - i];
        i++;
    }
    array[i] = NULL;
}
#endif

#if OPTPARSE_HELP_USAGE_STYLE == 1 && OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
// Prints all of a specified group's mutually exlusive options to a buffer.
// Assumes there are at least 2 group members.
static void bprint_exclusive_option_group(char *buffer,
    struct optparse_opt *opt, int *printed_groups)
{
    int group_index = opt->group;

    // Don't print groups that have already been printed.
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
    printed_groups[group_index] = 1; // Mark group as printed.
}
#endif

// Prints a command's usage.
static void print_usage(FILE *stream, struct optparse_cmd *cmd)
{
#if OPTPARSE_SUBCOMMANDS
    static int parents_initialized;
    if (parents_initialized == 0) {
        subcmd_depth = initialize_subcommand_parents(optparse_main_cmd, 1);
        parents_initialized = 1;
    }
#endif

#if OPTPARSE_HELP_LETTER_CASE == 0
    fprintf(stream, "Usage:");
#elif OPTPARSE_HELP_LETTER_CASE == 1
    fprintf(stream, "usage:");
#elif OPTPARSE_HELP_LETTER_CASE == 2
    fprintf(stream, "USAGE:");
#endif

    char buffer[OPTPARSE_PRINT_BUFFER_SIZE];
    buffer[0] = '\0';

    // If a custom usage string is provided, print it and return.
    if (cmd->usage) {
        bprintf(buffer, " %s\n", cmd->usage);
        goto print;
    }

    // Print command name(s).
#if OPTPARSE_SUBCOMMANDS
    {
        char *cmd_array[subcmd_depth + 1];
        build_cmd_array(cmd, cmd_array);
        for (int i = 0; cmd_array[i]; i++) {
            bprintf(buffer, " %s", cmd_array[i]);
        }
    }
#else
    bprintf(buffer, " %s", optparse_main_cmd->name);
#endif

    // Print command's options.
    if (cmd->options) {
#if OPTPARSE_HELP_USAGE_STYLE == 1
#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
        int printed_groups[OPTPARSE_MUTUALLY_EXCLUSIVE_GROUPS_MAX] = { 0 };
#endif

        struct optparse_opt *opt = cmd->options;
        while (opt->short_name != (char) END_OF_OPTIONS) {
#if OPTPARSE_HIDDEN_OPTIONS
            // Don't print options marked as "hidden".
            if (opt->hidden) {
                opt++;
                continue;
            }
#endif

#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
            if (opt->group) {
                bprint_exclusive_option_group(buffer, opt, printed_groups);
            } else
#endif
            {
                bprintf(buffer, " [");
                bprint_option_usage(buffer, opt);
                bprintf(buffer, "]");
            }
            opt++;
        }
#else
        bprintf(buffer, " [" OPTPARSE_HELP_USAGE_OPTIONS_STRING "]");
#endif
    }

    // Print command's operands.
    if (cmd->operands) {
        bprintf(buffer, " %s", cmd->operands);
    }

    print:
    blockprint(stream, buffer, 7, 7, OPTPARSE_HELP_MAX_LINE_WIDTH);
}

// Prints a set of options (names, arguments, descriptions).
// Returns the calculated divider width.
static void print_options(FILE *stream, struct optparse_opt options[])
{
    struct optparse_opt *opt = options;
    int divider_width = 0;

    // Determine divider width -------------------------------------------------
    // (see section "Print" below to know where the numbers come from)
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

        // Snap divider to options.
        if (len > divider_width && len <= OPTPARSE_HELP_MAX_DIVIDER_WIDTH) {
            divider_width = len;
        }

        if (opt->arg_name) {
#if OPTPARSE_ATTACHED_OPTION_ARGUMENTS
            if (opt->arg_name[0] == '[') {
#if OPTPARSE_LONG_OPTIONS
                if (opt->long_name) {
                    len += 1 + strlen(opt->arg_name);
                } else
#endif
                len += strlen(opt->arg_name);
            } else
#endif
            len += 1 + strlen(opt->arg_name);
        }

        // Snap divider to arguments.
        if (len > divider_width) {
            divider_width = len;
        }
        opt++;
    }

    if (divider_width > OPTPARSE_HELP_MAX_DIVIDER_WIDTH) {
        divider_width = OPTPARSE_HELP_MAX_DIVIDER_WIDTH;
    }

    // Print options -----------------------------------------------------------
    opt = options;
    while (opt->short_name != (char) END_OF_OPTIONS) {
#if OPTPARSE_HIDDEN_OPTIONS
        if (opt->hidden) {
            opt++;
            continue;
        }
#endif

        int len = 0;

        len += fprintf(stream, "%*c", OPTPARSE_HELP_INDENTATION_WIDTH, ' ');

        // Print option's short name.
        if (opt->short_name) {
            len += fprintf(stream, "-%c", opt->short_name);
#if OPTPARSE_LONG_OPTIONS
            if (opt->long_name) {
                len += fprintf(stream, ", ");
            }
        } else if (OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS) {
            len += fprintf(stream, "    ");
#endif
        }

#if OPTPARSE_LONG_OPTIONS
        // Print option's long name.
        if (opt->long_name) {
            len += fprintf(stream, "--%s", opt->long_name);
        }
#endif

        // Print option's arguments.
        if (opt->arg_name) {
#if OPTPARSE_ATTACHED_OPTION_ARGUMENTS
            if (opt->arg_name[0] == '[') {
#if OPTPARSE_LONG_OPTIONS
                if (opt->long_name) {
                    len += fprintf(stream, "[=%s", opt->arg_name + 1);
                } else
#endif
                len += fprintf(stream, "%s", opt->arg_name);
            } else
#endif
            len += fprintf(stream, " %s", opt->arg_name);
        }

        len += fprintf(stream, "%*c", OPTPARSE_HELP_INDENTATION_WIDTH, ' ');

        // Adjust spacing before printing option's description.
        if (len < divider_width) {
            len += fprintf(stream, "%*c", divider_width - len, ' ');
        }

        // Print option's description.
        if (opt->description) {
            if (len > divider_width) {
#if OPTPARSE_HELP_FLOATING_DESCRIPTIONS
                blockprint(stream, opt->description, len, divider_width,
                    OPTPARSE_HELP_MAX_LINE_WIDTH);
#else
                fprintf(stream, "\n%*c", divider_width, ' ');
                blockprint(stream, opt->description, divider_width,
                    divider_width, OPTPARSE_HELP_MAX_LINE_WIDTH);
#endif
            } else {
                blockprint(stream, opt->description, divider_width,
                    divider_width, OPTPARSE_HELP_MAX_LINE_WIDTH);
            }
        } else {
            fprintf(stream, "\n");
        }

        opt++;
    }
}

#if OPTPARSE_SUBCOMMANDS
// Prints a list of a command's subcommands.
static void print_subcommands(FILE *stream, struct optparse_cmd subcommands[])
{
    struct optparse_cmd *subcmd;
    int divider_width = 0;

    // Determine subcommand list's divider width.
    subcmd = subcommands;
    while (subcmd->name != END_OF_SUBCOMMANDS) {
        int len = strlen(subcmd->name);
        if (subcmd->operands) {
            len += strlen(subcmd->operands) + 1;
        }
        if (len > divider_width) {
            divider_width = len;
        }
        subcmd++;
    }
    divider_width += 2 * OPTPARSE_HELP_INDENTATION_WIDTH;
    if (divider_width > OPTPARSE_HELP_MAX_DIVIDER_WIDTH) {
        divider_width = OPTPARSE_HELP_MAX_DIVIDER_WIDTH;
    }

    // Print list of subcommands.
    subcmd = subcommands;
    while (subcmd->name != END_OF_SUBCOMMANDS) {
        char buffer[OPTPARSE_PRINT_BUFFER_SIZE];
        buffer[0] = '\0';
        int n = bprintf(buffer, "%*c%s%s%s%*c",
            OPTPARSE_HELP_INDENTATION_WIDTH, ' ',
            subcmd->name,
            subcmd->operands ? " " : "",
            subcmd->operands ? subcmd->operands : "",
            OPTPARSE_HELP_INDENTATION_WIDTH, ' ');
        if (n < divider_width) {
            bprintf(buffer, "%*c", divider_width - n, ' ');
        }
        fprintf(stream, "%s", buffer);

        if (subcmd->about) {
            if (n > divider_width) {
#if OPTPARSE_HELP_FLOATING_DESCRIPTIONS
                blockprint(stream, subcmd->about, n, divider_width,
                    OPTPARSE_HELP_MAX_LINE_WIDTH);
#else
                fprintf(stream, "\n%*c", divider_width, ' ');
                blockprint(stream, subcmd->about, divider_width, divider_width,
                    OPTPARSE_HELP_MAX_LINE_WIDTH);
#endif
            } else {
                blockprint(stream, subcmd->about, divider_width, divider_width,
                    OPTPARSE_HELP_MAX_LINE_WIDTH);
            }
        } else {
            fprintf(stream, "\n");
        }

        subcmd++;
    }
}
#endif

// Prints a command's complete help information: about, usage, description,
// options, subcommands.
// cmd_chain: a NULL-terminated array that contains a valid command chain
static void print_help(FILE *stream, struct optparse_cmd *cmd, int exit_status, bool noExit)
{
    if (stream != stderr && cmd->about) {
        blockprint(stream, cmd->about, 0, 0, OPTPARSE_HELP_MAX_LINE_WIDTH);
    }

    // Print command's usage.
    print_usage(stream, cmd);

    // Print command's description.
    if (cmd->description) {
        fprintf(stream, "\n");
        blockprint(stream, cmd->description, 0, 0,
            OPTPARSE_HELP_MAX_LINE_WIDTH);
    }

    // Print command's options.
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
    // Print list of subcommands.
    if (cmd->subcommands) {
#if OPTPARSE_HELP_LETTER_CASE == 0
        fprintf(stream, "\nCommands:\n");
#elif OPTPARSE_HELP_LETTER_CASE == 1
        fprintf(stream, "\ncommands:\n");
#elif OPTPARSE_HELP_LETTER_CASE == 2
        fprintf(stream, "\nCOMMANDS:\n");
#endif
        print_subcommands(stream, cmd->subcommands);
    }
#endif

    if (!noExit)
        exit(exit_status);
}

#if OPTPARSE_SUBCOMMANDS
// Parses a command chain and returns the subcommmand the chain leads to.
// Errors out if the chain is invalid.
static struct optparse_cmd *read_cmd_chain(struct optparse_cmd *cmd,
    char **argv)
{
    if (*argv && cmd->subcommands) {
        struct optparse_cmd *subcmd = cmd->subcommands;
        while (subcmd->name != END_OF_SUBCOMMANDS) {
            if (strcmp(subcmd->name, *argv) == 0) {
                return read_cmd_chain(subcmd, ++argv);
            }
            subcmd++;
        }

        optparse_error("Unknown command: \"%s\"\n", *argv);
        return NULL; // To satisfy the compiler.
    } else {
        return cmd;
    }
}
#endif

#ifndef NDEBUG
// Recursively checks a command's option structure for impossible/faulty setups.
static void check_cmd(struct optparse_cmd *cmd)
{
    // The command's name is required.
    assert(cmd->name != NULL);

    if (cmd->options) {
        struct optparse_opt *opt = cmd->options;
        while (opt->short_name != (char) END_OF_OPTIONS) {
            // At least one of those is required.
#if OPTPARSE_LONG_OPTIONS
            assert(opt->short_name || opt->long_name);
#else
            assert(opt->short_name);
#endif

            // Make sure option-argument is named properly.
            assert((opt->arg_name && opt->arg_name[0] == '['
                && opt->arg_name[strlen(opt->arg_name) - 1] == ']')
                || (opt->arg_name && opt->arg_name[0] != '[')
                || !opt->arg_name);

#if OPTPARSE_LIST_SUPPORT
            // .arg_storage_size requires .arg_delim and .arg_storage.
            assert((opt->arg_storage_size && opt->arg_delim && opt->arg_storage)
                || !opt->arg_storage_size);

            // Splitting and then calling like a non-array type-converted value
            // existed would produce random values.
            assert((opt->arg_delim && opt->function_type != FUNCTION_TYPE_TARG)
                || !opt->arg_delim);

            // If the option-argument is not split, no array exists and array
            // functions must not be called.
            assert((!opt->arg_delim && opt->function_type
                != FUNCTION_TYPE_TARG_ARRAY && opt->function_type
                != FUNCTION_TYPE_OARG_ARRAY) || opt->arg_delim);
#endif

#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
            // Group values must not be larger than
            // OPTPARSE_MUTUALLY_EXCLUSIVE_GROUPS_MAX.
            assert((opt->group && opt->group < OPTPARSE_MUTUALLY_EXCLUSIVE_GROUPS_MAX)
                || !opt->group);
#endif

            opt++;
        }
    }

#if OPTPARSE_SUBCOMMANDS
    if (cmd->subcommands) {
        struct optparse_cmd *subcmd = cmd->subcommands;
        while (subcmd->name != END_OF_SUBCOMMANDS) {
            check_cmd(subcmd);
            subcmd++;
        }
    }
#endif
}
#endif

/// Public functions -----------------------------------------------------------

// Parses command line options as described in the provided command structure.
void optparse_parse(struct optparse_cmd *cmd, int *argc, char ***argv)
{
#ifndef NDEBUG
    check_cmd(cmd);
#endif

    help_stream = stdout;
    optparse_main_cmd = cmd;
    if (optparse_main_cmd) {
        parse(argc, argv, optparse_main_cmd);
    }
}

// Advances the parser index by 1 and returns the next command line argument.
char *optparse_shift(void)
{
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
char *optparse_unshift(void)
{
    if (args == NULL) {
        return NULL;
    }

    if (args_index > 0) {
        return args[--args_index];
    } else {
        return NULL;
    }
}

// Prints the currently active command's help information.
void optparse_print_help(bool noExit)
{
#if OPTPARSE_SUBCOMMANDS
    print_help(help_stream, active_cmd, EXIT_SUCCESS, noExit);
#else
    print_help(help_stream, optparse_main_cmd, EXIT_SUCCESS, noExit);
#endif
}

// Same as optparse_print_help, but prints to the specified stream. Exits with
// the provided exit status.
void optparse_fprint_help(FILE *stream, int exit_status, bool noExit)
{
#if OPTPARSE_SUBCOMMANDS
    print_help(stream, active_cmd, exit_status, noExit);
#else
    print_help(stream, optparse_main_cmd, exit_status, noExit);
#endif
}

// Prints the currently active command's usage information only.
void optparse_fprint_usage(FILE *stream)
{
#if OPTPARSE_SUBCOMMANDS
    print_usage(stream, active_cmd);
#else
    print_usage(stream, optparse_main_cmd);
#endif
}

#if OPTPARSE_SUBCOMMANDS
static inline void __optparse_print_help_subcmd(int argc, char **argv, bool noExit) {
    (void) argc; // To avoid compilers complaining about "unused parameter".
    argv++; // To ignore the program's file name
    if (*argv) {
        struct optparse_cmd *subcmd = read_cmd_chain(optparse_main_cmd, argv);
        print_help(stdout, subcmd, EXIT_SUCCESS, noExit);
    } else {
        print_help(stdout, optparse_main_cmd, EXIT_SUCCESS, noExit);
    }
}

// Prints a subcommand's help by parsing remaining operands. To be used as a
// command structure's .function member.
void optparse_print_help_subcmd(int argc, char **argv)
{
    __optparse_print_help_subcmd(argc, argv, false);
}

void optparse_print_help_subcmd_noexit(int argc, char **argv)
{
    __optparse_print_help_subcmd(argc, argv, true);
}
#endif

// Converts a string to a different data type.
// Return value:  0: success
//                1: string is not convertible
//               -1: converted data is out of range
// Example:
//     int i;
//     int retval = strtox("512", &i, DATA_TYPE_INT);
int strtox(char *str, void *x, enum optparse_data_type data_type)
{
    if (str == NULL) {
        return 1;
    }

    char *endptr = NULL;
    errno = 0;

    switch (data_type) {
        case DATA_TYPE_STR:
            *(char **) x = str;
            break;
        case DATA_TYPE_CHAR:
            if (strlen(str) > 1) {
                errno = ERANGE;
            }
            *(char *) x = str[0];
            break;
        case DATA_TYPE_SCHAR:
            if (strlen(str) > 1) {
                errno = ERANGE;
            }
            *(signed char *) x = str[0];
            break;
        case DATA_TYPE_UCHAR:
            if (strlen(str) > 1) {
                errno = ERANGE;
            }
            *(unsigned char *) x = str[0];
            break;
        case DATA_TYPE_SHRT:
            {
                long result = strtol(str, &endptr, 0);
                if (result < SHRT_MIN || result > SHRT_MAX) {
                    errno = ERANGE;
                }
                *(short *) x = (short) result;
            }
            break;
        case DATA_TYPE_USHRT:
            {
                unsigned long result = strtoul(str, &endptr, 0);
                if (result > USHRT_MAX) {
                    errno = ERANGE;
                }
                *(unsigned short *) x = (unsigned short) result;
            }
            break;
        case DATA_TYPE_INT:
            {
                long result = strtol(str, &endptr, 0);
                if (result < INT_MIN || result > INT_MAX) {
                    errno = ERANGE;
                }
                *(int *) x = (int) result;
            }
            break;
        case DATA_TYPE_UINT:
            {
                unsigned long result = strtoul(str, &endptr, 0);
                if (result > UINT_MAX) {
                    errno = ERANGE;
                }
                *(unsigned int *) x = (unsigned int) result;
            }
            break;
        case DATA_TYPE_LONG:
            *(long *) x = strtol(str, &endptr, 0);
            break;
        case DATA_TYPE_ULONG:
            *(unsigned long *) x = strtoul(str, &endptr, 0);
            break;
        case DATA_TYPE_LLONG:
            *(long long *) x = strtoll(str, &endptr, 0);
            break;
        case DATA_TYPE_ULLONG:
            *(unsigned long long *) x = strtoull(str, &endptr, 0);
            break;
#if OPTPARSE_FLOATING_POINT_SUPPORT
        case DATA_TYPE_FLT:
            {
                double result = strtod(str, &endptr);
                if (result < FLT_MIN || result > FLT_MAX) {
                    errno = ERANGE;
                }
                *(float *) x = (float) result;
            }
            break;
        case DATA_TYPE_DBL:
            *(double *) x = strtod(str, &endptr);
            break;
        case DATA_TYPE_LDBL:
            *(long double *) x = strtold(str, &endptr);
            break;
#endif
        case DATA_TYPE_BOOL:
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
                    int result = strtox(str, x, DATA_TYPE_INT);
                    if (result == 1 || result == 0) {
                        *(bool *) x = result;
                    } else {
                        endptr = str;
                    }
                }
            }
            break;
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
        case DATA_TYPE_INT8:
            {
                long result = strtol(str, &endptr, 0);
                if (result < INT8_MIN || result > INT8_MAX) {
                    errno = ERANGE;
                }
                *(int8_t *) x = (int8_t) result;
            }
            break;
        case DATA_TYPE_UINT8:
            {
                unsigned long result = strtoul(str, &endptr, 0);
                if (result > UINT8_MAX) {
                    errno = ERANGE;
                }
                *(uint8_t *) x = (uint8_t) result;
            }
            break;
        case DATA_TYPE_INT16:
            {
                long result = strtol(str, &endptr, 0);
                if (result < INT16_MIN || result > INT16_MAX) {
                    errno = ERANGE;
                }
                *(int16_t *) x = (int16_t) result;
            }
            break;
        case DATA_TYPE_UINT16:
            {
                unsigned long result = strtoul(str, &endptr, 0);
                if (result > UINT16_MAX) {
                    errno = ERANGE;
                }
                *(uint16_t *) x = (uint16_t) result;
            }
            break;
        case DATA_TYPE_INT32:
            {
                long result = strtol(str, &endptr, 0);
                if (result < INT32_MIN || result > INT32_MAX) {
                    errno = ERANGE;
                }
                *(int32_t *) x = (int32_t) result;
            }
            break;
        case DATA_TYPE_UINT32:
            {
                unsigned long result = strtoul(str, &endptr, 0);
                if (result > UINT32_MAX) {
                    errno = ERANGE;
                }
                *(uint32_t *) x = (uint32_t) result;
            }
            break;
        case DATA_TYPE_INT64:
            {
                long long result = strtoll(str, &endptr, 0);
                if (result < INT64_MIN || result > INT64_MAX) {
                    errno = ERANGE;
                }
                *(int64_t *) x = (int64_t) result;
            }
            break;
        case DATA_TYPE_UINT64:
            {
                unsigned long long result = strtoull(str, &endptr, 0);
                if (result > UINT64_MAX) {
                    errno = ERANGE;
                }
                *(uint64_t *) x = (uint64_t) result;
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
