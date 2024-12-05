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

#ifndef OPTPARSE99_H
#define OPTPARSE99_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/// Customizable preprocessor directives ---------------------------------------

#ifndef OPTPARSE_LONG_OPTIONS
#define OPTPARSE_LONG_OPTIONS true
#endif

#ifndef OPTPARSE_SUBCOMMANDS
#define OPTPARSE_SUBCOMMANDS true
#endif

#ifndef OPTPARSE_ATTACHED_OPTION_ARGUMENTS
#define OPTPARSE_ATTACHED_OPTION_ARGUMENTS true
#endif

#ifndef OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
#define OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS true
#endif

#ifndef OPTPARSE_HIDDEN_OPTIONS
#define OPTPARSE_HIDDEN_OPTIONS true
#endif

#ifndef OPTPARSE_LIST_SUPPORT
#define OPTPARSE_LIST_SUPPORT true
#endif

#ifndef OPTPARSE_FLOATING_POINT_SUPPORT
#define OPTPARSE_FLOATING_POINT_SUPPORT true
#endif

#ifndef OPTPARSE_C99_INTEGER_TYPES_SUPPORT
#define OPTPARSE_C99_INTEGER_TYPES_SUPPORT true
#endif

// Indentation width, in characters.
// Default value: 2
#ifndef OPTPARSE_HELP_INDENTATION_WIDTH
#define OPTPARSE_HELP_INDENTATION_WIDTH 2
#endif

// Maximum distance between the screen's left edge and option descriptions.
// Default value: 32
#ifndef OPTPARSE_HELP_MAX_DIVIDER_WIDTH
#define OPTPARSE_HELP_MAX_DIVIDER_WIDTH 32
#endif

// Defines how a description is printed if preceeding text is longer than
// OPTPARSE_HELP_MAX_DIVIDER_WIDTH.
// true: print description on the same line, after a single indentation
// false: print description on a separate line
// Default value: true
#ifndef OPTPARSE_HELP_FLOATING_DESCRIPTIONS
#define OPTPARSE_HELP_FLOATING_DESCRIPTIONS true
#endif

// Maximum line width for word wrapping.
// Default value: 80
#ifndef OPTPARSE_HELP_MAX_LINE_WIDTH
#define OPTPARSE_HELP_MAX_LINE_WIDTH 80
#endif

// Enables word wrap for lines longer than OPTPARSE_HELP_MAX_LINE_WIDTH.
// Default value: true
#ifndef OPTPARSE_HELP_WORD_WRAP
#define OPTPARSE_HELP_WORD_WRAP true
#endif

// Style used for automatic usage generation.
// Default value: 0
#ifndef OPTPARSE_HELP_USAGE_STYLE
#define OPTPARSE_HELP_USAGE_STYLE 0 // 0: short, 1: verbose
#endif

// Placeholder string to be displayed if OPTPARSE_HELP_USAGE_STYLE is 0.
// Default value: "OPTIONS"
#ifndef OPTPARSE_HELP_USAGE_OPTIONS_STRING
#define OPTPARSE_HELP_USAGE_OPTIONS_STRING "OPTIONS"
#endif

// The help screen's letter case.
// Default value: 0
#ifndef OPTPARSE_HELP_LETTER_CASE
#define OPTPARSE_HELP_LETTER_CASE 0 // 0: capitalized, 1: lower, 2: upper
#endif

// Makes long options stay in a separate column even if there's no short option.
// Default value: true
#ifndef OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS
#define OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS true
#endif

// Prints the currently active command's help screen if there's a parsing error.
// Default value: true
#ifndef OPTPARSE_PRINT_HELP_ON_ERROR
#define OPTPARSE_PRINT_HELP_ON_ERROR true
#endif

// The maximum amount of groups for mutually exclusive options.
// Default value: 8
#ifndef OPTPARSE_MUTUALLY_EXCLUSIVE_GROUPS_MAX
#define OPTPARSE_MUTUALLY_EXCLUSIVE_GROUPS_MAX 8
#endif

// The size of the buffer used for printing functionality such as printing help
// and usage.
// Default value: 1024
#ifndef OPTPARSE_PRINT_BUFFER_SIZE
#define OPTPARSE_PRINT_BUFFER_SIZE 1024
#endif

/// Option structure -----------------------------------------------------------

#define END_OF_OPTIONS -1 // Marks the end of an option array.

// Represents C99 data types.
// Used for both .arg_type and function strtox().
enum optparse_data_type {
    DATA_TYPE_STR,    // char *; no conversion takes place (default)
    DATA_TYPE_CHAR,   // char
    DATA_TYPE_SCHAR,  // signed char
    DATA_TYPE_UCHAR,  // unsigned char
    DATA_TYPE_SHRT,   // short
    DATA_TYPE_USHRT,  // unsigned short
    DATA_TYPE_INT,    // int
    DATA_TYPE_UINT,   // unsigned int
    DATA_TYPE_LONG,   // long
    DATA_TYPE_ULONG,  // unsigned long
    DATA_TYPE_LLONG,  // long long
    DATA_TYPE_ULLONG, // unsigned long long
#if OPTPARSE_FLOATING_POINT_SUPPORT
    DATA_TYPE_FLT,    // float
    DATA_TYPE_DBL,    // double
    DATA_TYPE_LDBL,   // long double
#endif
    DATA_TYPE_BOOL,   // _Bool
#if OPTPARSE_C99_INTEGER_TYPES_SUPPORT
    DATA_TYPE_INT8,   // int8_t
    DATA_TYPE_UINT8,  // uint8_t
    DATA_TYPE_INT16,  // int16_t
    DATA_TYPE_UINT16, // uint16_t
    DATA_TYPE_INT32,  // int32_t
    DATA_TYPE_UINT32, // uint32_t
    DATA_TYPE_INT64,  // int64_t
    DATA_TYPE_UINT64, // uint64_t
#endif
};

// Specifies what to do with the integer variable .flag points to.
enum optparse_flag_type {
    FLAG_TYPE_SET_TRUE,  // Set to 1 (default)
    FLAG_TYPE_SET_FALSE, // Set to 0
    FLAG_TYPE_INCREMENT, // Increase by 1
    FLAG_TYPE_DECREMENT, // Decrease by 1
};

// Specifies how the function pointed to by .function is expected to be declared
// and, internally, going to be called.
enum optparse_function_type {
    FUNCTION_TYPE_AUTO,       // Automatically decide (default):
                              // .arg_name is set?
                              //   .arg_delim is set? FUNCTION_TYPE_TARG_ARRAY
                              //   else               FUNCTION_TYPE_TARG
                              // else                 FUNCTION_TYPE_VOID
    FUNCTION_TYPE_TARG,       // TARG means "type-converted option-argument".
                              // declaration: void f(DATA_TYPE);
                              // call:        f(TARG);
                              // (DATA_TYPE is set according to .arg_data_type.)
    FUNCTION_TYPE_OARG,       // OARG means "original option-argument".
                              // declaration: void f(char *);
                              // call:        f(OARG);
#if OPTPARSE_LIST_SUPPORT
    FUNCTION_TYPE_TARG_ARRAY, // declaration: void f(size_t, DATA_TYPE *);
                              // call:        f(TARG_ARRAY_SIZE, TARG_ARRAY);
    FUNCTION_TYPE_OARG_ARRAY, // declaration: void f(size_t, char **);
                              // call:        f(OARG_ARRAY_SIZE, OARG_ARRAY);
#endif
    FUNCTION_TYPE_VOID,       // declaration: void f(void);
                              // call:        f();
};

struct optparse_opt {
    char short_name;          // The short option character.
#if OPTPARSE_LONG_OPTIONS
    char *long_name;          // The long option string, without leading "--".
                              // At least .short_name or .long_name must be set.
#endif
    char *arg_name;           // If set, it means the option has one or more
                              // option-arguments. The string is displayed as-is
                              // in the help screen. If it begins with "[", the
                              // option-argument is regarded as optional.
    enum optparse_data_type arg_data_type;
                              // If set, the parsed option-argument will be
                              // converted to a different data type.
#if OPTPARSE_LIST_SUPPORT
    char *arg_delim;          // If set, the option-argument will be treated as
                              // a list whose items are separated by any of this
                              // string's characters.
#endif
    void *arg_storage;        // The memory location the (type-converted)
                              // option-argument is saved to. Its data type must
                              // match the one defined in .arg_data_type. If
                              // .arg_delim is set, it must be a pointer (which
                              // after parsing will point to dynamically
                              // allocated memory).
#if OPTPARSE_LIST_SUPPORT
    size_t *arg_storage_size; // The memory location the number of list items
                              // stored in *arg_storage is saved to.
#endif
    int *flag;                // A pointer to an integer variable that is to be
                              // used as specified by .flag_type.
    enum optparse_flag_type flag_type;
    void (*function)(void);   // Points to a function that is called as
                              // specified in .function_type. In the struct's
                              // initializer, the pointer can be cast to a void
                              // function pointer to avoid compiler warnings:
                              // .function = (void (*)(void)) function_name;
    enum optparse_function_type function_type;
#if OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS
    int group;                // Options that share the same group value are
                              // treated as mutually exclusive.
#endif
#if OPTPARSE_HIDDEN_OPTIONS
    _Bool hidden;             // If true, the option won't be displayed in the
                              // help screen.
#endif
    char *description;        // A string that will appear as the option's
                              // documentation in the help screen.
};

/// Command structure ----------------------------------------------------------

#define END_OF_SUBCOMMANDS NULL // Marks the end of a subcommand array.

struct optparse_cmd {
    char *name;        // The command line string users enter to run the
                       // command. (required)
    char *about;       // A short sentence, describing the command's purpose in
                       // a nutshell. May also contain information like version
                       // number, homepage, etc.
    char *description; // The command's detailed documentation.
    char *operands;    // The command's operands (aka "positional arguments") as
                       // to be displayed in the help screen.
    char *usage;       // Used to override automatic usage generation.
    void (*function)(int, char **);
                       // Called after parsing options/subcommands.
    struct optparse_opt *options;
                       // Points to an array containing the command's options.
#if OPTPARSE_SUBCOMMANDS
    struct optparse_cmd *subcommands;
                       // Points to an array containing the command's
                       // subcommands.
    struct optparse_cmd *_parent;
                       // Used internally to keep track of nested subcommands.
#endif
};

/// Functions ------------------------------------------------------------------

// Parses command line options as specified in the command tree *cmd.
// Modifies argc and argv to only contain non-option arguments.
void optparse_parse(struct optparse_cmd *cmd, int *argc, char ***argv);

// Prints the currently active command's full help information, listing
// available options and their descriptions. It can be called manuall or through
// an option's function member. Exits with exit status EXIT_SUCCESS.
void optparse_print_help(bool noExit);

// Same as optparse_print_help, but can only be called manually. It prints to
// the specified stream and exits with the provided exit status.
void optparse_fprint_help(FILE *stream, int exit_status, bool noExit);

// Prints the currently active command's usage information only.
void optparse_fprint_usage(FILE *stream);

#if OPTPARSE_SUBCOMMANDS
// Prints a subcommand's help by parsing remaining operands. To be used as a
// command structure's .function member.
void optparse_print_help_subcmd(int argc, char **argv);

void optparse_print_help_subcmd_noexit(int argc, char **argv);
#endif

// Advances the parser's internal index and returns the next command line
// argument. If there are no more arguments left or optparse_parse() is not
// running, the return value is NULL.
char *optparse_shift(void);

// Undoes the previously called optparse_shift() and returns the previous
// command line argument. Must be called in the same function and is only
// guaranteed to undo the most recent shift.
char *optparse_unshift(void);

// Converts a string to different data type. Can, for example, be used to
// manually convert option-arguments retreived by optparse_shift().
// Return value:  0: success
//                1: string is not convertible
//               -1: converted data is out of range
// Example:
//     int i;
//     int retval = strtox("512", &i, DATA_TYPE_INT);
int strtox(char *str, void *x, enum optparse_data_type data_type);
#endif
