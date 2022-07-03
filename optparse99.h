// A C99+ option parser.
// Copyright (c) 2022 hippie68 (https://github.com/hippie68/optparse99)

#ifndef OPTPARSE99_H
#define OPTPARSE99_H

#include <stdbool.h>
#include <stddef.h>

/// Customizable preprocessor directives ---------------------------------------

#define OPTPARSE_LONG_OPTIONS true
#define OPTPARSE_SUBCOMMANDS true
#define OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS true
#define OPTPARSE_HIDDEN_OPTIONS true
#define OPTPARSE_FLOATING_POINT_SUPPORT true
#define OPTPARSE_C99_INTEGER_TYPES_SUPPORT true

// Indentation width, in characters
//#define OPTPARSE_HELP_INDENTATION_WIDTH 2

// Maximum distance between the screen's left edge and option descriptions
//#define OPTPARSE_HELP_MAX_DIVIDER_WIDTH 32

// Defines how a description is printed if preceeding text is longer than
// OPTPARSE_HELP_MAX_DIVIDER_WIDTH
// true: print description on the same line, after a single indentation
// false: print description on a separate line
//#define OPTPARSE_HELP_FLOATING_DESCRIPTIONS true

// Maximum line width for word wrapping
//#define OPTPARSE_HELP_MAX_LINE_WIDTH 80

// Enables word wrap for lines longer than OPTPARSE_HELP_MAX_LINE_WIDTH
//#define OPTPARSE_HELP_WORD_WRAP true

// Style used for automatic usage generation
//#define OPTPARSE_HELP_USAGE_STYLE 1 // 0: short, 1: verbose

// Placeholder string to be displayed if OPTPARSE_HELP_USAGE_STYLE is 0
//#define OPTPARSE_HELP_USAGE_OPTIONS_STRING "OPTIONS"

// The help screen's letter case
//#define OPTPARSE_HELP_LETTER_CASE 0 // 0: capitalized, 1: lower, 2: upper

// Makes long options stay in a separate column even if there's no short option
//#define OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS true

// Prints the currently active command's help screen if there's a parsing error
//#define OPTPARSE_PRINT_HELP_ON_ERROR true

/// Option structure -----------------------------------------------------------

#define END_OF_OPTIONS -1 // Marks the end of an option array

// Used for .arg_type; describes to which C data type the option's argument
// string (retreived from argv) has to be converted.
enum optparse_arg_type {
    ARG_TYPE_STR,    // char *; no conversion takes place (default)
    ARG_TYPE_CHAR,   // char
    ARG_TYPE_SCHAR,  // signed char
    ARG_TYPE_UCHAR,  // unsigned char
    ARG_TYPE_SHRT,   // short
    ARG_TYPE_USHRT,  // unsigned short
    ARG_TYPE_INT,    // int
    ARG_TYPE_UINT,   // unsigned int
    ARG_TYPE_LONG,   // long
    ARG_TYPE_ULONG,  // unsigned long
    ARG_TYPE_LLONG,  // long long
    ARG_TYPE_ULLONG, // unsigned long long
#if OPTPARSE_FLOATING_POINT_SUPPORT
    ARG_TYPE_FLT,    // float
    ARG_TYPE_DBL,    // double
    ARG_TYPE_LDBL,   // long double
#endif
    ARG_TYPE_BOOL,   // _Bool
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
    ARG_TYPE_INT8,   // int8_t
    ARG_TYPE_UINT8,  // uint8_t
    ARG_TYPE_INT16,  // int16_t
    ARG_TYPE_UINT16, // uint16_t
    ARG_TYPE_INT32,  // int32_t
    ARG_TYPE_UINT32, // uint32_t
    ARG_TYPE_INT64,  // int64_t
    ARG_TYPE_UINT64, // uint64_t
#endif
};

// Used for .flag_action; describes what to do with the integer value .flag
// points to.
enum optparse_flag_action {
    FLAG_ACTION_SET_TRUE,  // set to 1 (default)
    FLAG_ACTION_SET_FALSE, // set to 0
    FLAG_ACTION_INCREMENT, // increase by 1
    FLAG_ACTION_DECREMENT, // decrease by 1
};

// Used for .function_arg; describes which argument the function pointed
// to by .function has to receive.
// How automatic decision works:
//   - if .arg is not set: FUNCTION_ARG_VOID
//   - if .arg is set:
//     - if .arg_type is ARG_TYPE_STR: FUNCTION_ARG_OPTION_ARG
//     - if .arg_type is not ARG_TYPE_STR: FUNCTION_ARG_CONVERTED_OPTION_ARG
enum optparse_function_arg {
    FUNCTION_ARG_AUTO,                 // Automatically decide (default)
    FUNCTION_ARG_VOID,                 // void
    FUNCTION_ARG_OPTION_ARG,           // The unchanged option argument (char *)
    FUNCTION_ARG_CONVERTED_OPTION_ARG, // The type-converted option argument
};

struct optparse_opt {
    char short_name; // The short option character
#if OPTPARSE_LONG_OPTIONS
    char *long_name; // The long option string, without leading "--"
#endif
    char *arg;
    void *arg_dest;
    enum optparse_arg_type arg_type;
    int *flag;
    enum optparse_flag_action flag_action;
    void (*function)(void);
    enum optparse_function_arg function_arg;
#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
    int group;
#endif
#if OPTPARSE_HIDDEN_OPTIONS
    _Bool hidden;
#endif
    char *description;
};

/// Command structure ----------------------------------------------------------

#define END_OF_SUBCOMMANDS NULL // Marks the end of a subcommand array

struct optparse_cmd {
    char *about;
    char *description;
    char *name;
    char *operands;
    char *usage; // Used to override automatic usage generation
    void (*function)(int, char **); // Called after parsing options/subcommands
    struct optparse_opt *options;
#if OPTPARSE_SUBCOMMANDS
    struct optparse_cmd *subcommands;
#endif
};

/// Functions ------------------------------------------------------------------

// Parses command line options as specified in the command tree *cmd.
// Modifies argc and argv to only contain non-option arguments.
void optparse_parse(struct optparse_cmd *cmd, int *argc, char ***argv);

// Prints the currently active command's usage information and lists available
// options and their descriptions. Exits with exit status EXIT_SUCCESS.
void optparse_print_help(void);

// Same as optparse_print_help, but prints to stderr. Exits with EXIT_FAILURE.
void optparse_print_help_stderr(void);

#if OPTPARSE_SUBCOMMANDS
// Prints a subcommand's help by parsing remaining operands. To be used as a
// command structure's .function member.
void optparse_print_help_subcmd(int argc, char **argv);
#endif

// Advances the parser's internal index and returns the next command line
// argument. If there are no more arguments left or optparse_parse() is not
// running, the return value is NULL.
char *optparse_shift(void);

// Undoes the previously called optparse_shift() and returns the previous
// command line argument. Must be called in the same function and is only
// guaranteed to undo the most recent shift.
char *optparse_unshift(void);

/// Function "strtox" ----------------------------------------------------------

// The function strtox() converts a string to different data type. It can be
// used to manually convert option-arguments that have been retreived by
// optparse_shift().

enum strtox_conversion_type {
    STRTOC,    // char
    STRTOSC,   // signed char
    STRTOUC,   // unsigned char
    STRTOS,    // short
    STRTOUS,   // unsigned short
    STRTOI,    // int
    STRTOUI,   // unsigned int
    STRTOL,    // long
    STRTOUL,   // unsigned long
    STRTOLL,   // long long
    STRTOULL,  // unsigned long long
#if OPTPARSE_FLOATING_POINT_SUPPORT
    STRTOF,    // float
    STRTOD,    // double
    STRTOLD,   // long double
#endif
    STRTOB,    // _Bool
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
    STRTOI8,   // int8_t
    STRTOUI8,  // uint8_t
    STRTOI16,  // int16_t
    STRTOUI16, // uint16_t
    STRTOI32,  // int32_t
    STRTOUI32, // uint32_t
    STRTOI64,  // int64_t
    STRTOUI64, // uint64_t
#endif
};

// Converts a string to a different type.
// Return value:  0: success
//                1: string is not convertible
//               -1: string is too long
// Example:
//     int i;
//     strtox("512", &i, STRTOI);
int strtox(char *str, void *x, enum strtox_conversion_type conversion_type);

#endif
