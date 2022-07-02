A C99+ option parser.

# Features:
  - Supports both short and long options.
    - Short options are POSIX-compliant (-o ARG, -oARG, -asdfoARG).
    - Long options follow the GNU standard (--option ARG, --option=ARG).
  - The order of options and operands does not matter.
  - Supports the "end of options" delimiter (--).
  - Can set integer flags (true, false, increment, decrement).
  - Can type-convert and store option-arguments.
  - Supports subcommands and nested subcommands.
  - Options and commands/subcommands can call functions ("callbacks").
  - Mutually exclusive options.
  - A nicely-formatted, customizable help screen with word-wrapping.
  - Provides functions for easy manual parsing (e.g. to implement multiple option-arguments).
  - Provides function "strtox()" for manual type-conversion.
  - Features can be toggled to only compile necessary code.

This is a first version that might be rough around the edges or have bugs. If you want to help polish it or report bugs, please create an issue at https://github.com/hippie68/optparse99/issues.

# Index
- [Quick example](#quick-example)
- [Documentation](#documentation)
  - [Command structure](#command-structure)
  - [Option structure](#option-structure)
  - [Functions](#functions)
    - [Manual parsing](#manual-parsing)
    - [Manual type-converting](#manual-type-converting)
  - [Preprocessor directives](#preprocessor-directives)
- [Notes](#notes)

# Quick example

```C
#include "optparse99.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    struct optparse_cmd main_cmd = {
        .about = "Supertool v1.00 - A really handy tool.",
        .description = "This is an example program with a basic optparse99 setup. After parsing, it will print the remaining command line arguments.",
        .name = "supertool",
        .operands = "ARGUMENT [ARGUMENT...]",
        .options = (struct optparse_opt []) {
            {
                .short_name = 'h',
                .long_name = "help",
                .description = "Print help information and quit.",
                .function = optparse_print_help,
            },
            { END_OF_OPTIONS },
        },
    };

    optparse_parse(&main_cmd, &argc, &argv);

    // Print remaining operands
    if (argc) {
        for (int i = 0; i <= argc; i++) {
            printf("argv[%d]: %s\n", i, argv[i]);
        }
    } else {
        optparse_print_help();
    }
}
```

Help screen output:

```
$ supertool --help
Supertool v1.00 - A really handy tool.
Usage: supertool [-h] ARGUMENT [ARGUMENT...]

This is an example program with a basic optparse99 setup. After parsing, it will
print the remaining command line arguments.

Options:
  -h, --help  Print help information and quit.
```

# Documentation

Optparse99 is based on two types of structures: commands and options. Each command can have a set of options and a set of subcommands, the latter which can be nested tree-like. The root of the tree is always the program's command itself.
Command and option structures are to be thought of as sets of instructions. Most structure members are optional and, by making use of C99's designated initializers, do not need to be specified. Non-specified members are initialized with 0 (NULL), which for enumeration types is the default value.

Both options and subcommands are arrays that are constructed by compound literals and must end with the element { END_OF_OPTIONS } and { END_OF_SUBCOMMANDS }, respectively:

```C
struct optparse_cmd main_cmd = {
    .options = (struct optparse_opt []) = {
        { ... },
        { END_OF_OPTIONS },
    },
    .subcommands = (struct optparse_cmd []) = {
        { ... },
        { END_OF_SUBCOMMANDS },
    },
};
```

## Command structure

```C
struct optparse_cmd {
    char *name;
    char *about;
    char *description;
    char *operands;
    char *usage;
    void (*function)(int, char **);
    struct optparse_opt *options;
    struct optparse_cmd *subcommands;
};
```

Structure member | Description
---------------- | ----------------
.name (required) | The command line string users enter to run the command.
.about           | A short sentence, describing the command's purpose in a nutshell. May also contain information like version number, homepage, etc.
.description     | The command's detailed documentation.
.operands        | The command's operands (aka "positional arguments") as to be displayed in the help screen.
.usage           | Can be specified to override automatic usage generation, e.g. if operands depend on options.
.function        | Once the command's options have been parsed, the command will call the specified function, using the current state of argc and argv as function arguments.
.options         | Points to an array containing the command's options.
.subcommands     | Point to an array containing the command's subcommands.

## Option structure

```C
struct optparse_opt {
    char short_name;
    char *long_name;
    char *arg;
    void *arg_dest;
    enum optparse_arg_type arg_type;
    int *flag;
    enum optparse_flag_action flag_action;
    void (*function)(void);
    enum optparse_function_arg function_arg;
    int group;
    _Bool hidden;
    char *description;
};
```

Structure member        | Description
----------------------- | -----------------------
.short_name (required*) | The short option character.
.long_name (required*)  | The long option string (without leading "--").
.arg                    | If specified, it means the option has one or more option-arguments. The string is displayed as-is in the help screen. If it begins with "\[", the argument is regarded as optional.
.arg_dest               | The memory location the (type-converted) option-argument is saved to.
.arg_type               | If set, the parsed (single) option-argument will be converted from string to a different data type.
.flag                   | A pointer to an integer variable that is to be used as specified by .flag_action.
.flag_action            | Specifies what to do to with the flag variable's value.
.function               | Points to a function that is called with the argument specified in .function_arg.
.function_arg           | Tells .function which argument to use.
.group                  | Options that share the same group value are treated as mutually exclusive.
.hidden                 | If true, the option won't be displayed in the help screen.
.description            | The option's description, whether short or in-depth.
* At least one of them must be specified.

### Allowed values for .arg_type

Value                  | Conversion type
---------------------- | ------------------
ARG_TYPE_STR (default) | (no conversion)
ARG_TYPE_CHAR          | char
ARG_TYPE_SCHAR         | signed char
ARG_TYPE_UCHAR         | unsigned char
ARG_TYPE_SHRT          | short
ARG_TYPE_USHRT         | unsigned short
ARG_TYPE_INT           | int
ARG_TYPE_UINT          | unsigned int
ARG_TYPE_LONG          | long
ARG_TYPE_ULONG         | unsigned long
ARG_TYPE_LLONG         | long long
ARG_TYPE_ULLONG        | unsigned long long
ARG_TYPE_FLT           | float
ARG_TYPE_DBL           | double
ARG_TYPE_LDBL          | long double
ARG_TYPE_BOOL          | \_Bool
ARG_TYPE_INT8          | int8_t
ARG_TYPE_UINT8         | uint8_t
ARG_TYPE_INT16         | int16_t
ARG_TYPE_UINT16        | uint16_t
ARG_TYPE_INT32         | int32_t
ARG_TYPE_UINT32        | uint32_t
ARG_TYPE_INT64         | int64_t
ARG_TYPE_UINT64        | uint64_t

### Allowed values for .flag_action

Value                          | Result
------------------------------ | ------------------------------
FLAG_ACTION_SET_TRUE (default) | Set the variable to 1.
FLAG_ACTION_SET_FALSE          | Set the variable to 0.
FLAG_ACTION_INCREMENT          | Increase the variable's current value by 1.
FLAG_ACTION_DECREMENT          | Decrease the variable's current value by 1.

### Allowed values for .function_arg

Value                             | Used function argument
--------------------------------- | ---------------------------------
FUNCTION_ARG_AUTO (default)       | Automatically decide (default)
FUNCTION_ARG_VOID                 | void
FUNCTION_ARG_OPTION_ARG           | The unchanged option-argument (char \*)
FUNCTION_ARG_CONVERTED_OPTION_ARG | The type-converted option-argument (type as specified in .arg_type)

How automatic decision works:
   - if .arg is not set: FUNCTION_ARG_VOID
   - if .arg is set:
     - if .arg_type is ARG_TYPE_STR: FUNCTION_ARG_OPTION_ARG
     - if .arg_type is not ARG_TYPE_STR: FUNCTION_ARG_CONVERTED_OPTION_ARG

## Functions

```C
void optparse_parse(optparse_cmd *cmd, int *argc, char ***argv);
```

optparse_parse() starts the parsing process. It must be called before calling any other optparse_ function. After parsing, the main() function's argc and argv will contain only operands.
\*cmd: a pointer to the command tree's root command
\*argc: a pointer to main()'s argc variable
\*argv: a pointer to main()'s argv variable

```C
void optparse_print_help(void);
```

Prints the root command's help information. It is thought to be invoked through an option's .function member - see the [quick example](#quick-example) above.

void optparse_print_help_subcmd(void);
```

optparse_print_help_subcmd() is the same as optparse_print_help(), but for subcommands. It must be called while the remaining arguments represent chained subcommand names, e.g. as a subcommand's function:

```C
    ...
    .subcommands = (struct optparse_cmd []) {
        {
            .description = "Print a subcommand's help information and quit.",
            .name = "help",
            .operands = "[COMMAND...]",
            .function = optparse_print_help_subcmd,
        },
        ...
```

### Manual parsing

It is possible to manually parse arguments from inside a callback function (.function).
Inside that function, the following functions can be used:

```C
char *optparse_shift(void);
char *optparse_unshift(void);
```

optparse_shift() returns the next command line argument, while optparse_unshift() puts it back. Please note that optparse_unshift() is only guaranteed to undo the most recent shift.
Using this technique, you can parse multiple option-arguments while having full control over what exactly qualifies as a valid option-argument and when to stop.

Note: If part of an option structure, the callback function should have the parameter char *, and .function_arg should be FUNCTION_ARG_OPTION_ARG. This is required to catch an option-argument the user may have entered via -oARG or --option=ARG. The function's argument will be the first command line argument, so it must be checked prior to using optparse_shift().

### Manual type-converting

The function used internally for type-converting option-arguments is strtox():

```C
int strtox(char *str, void *x, enum strtox_conversion_type conversion_type);
```

It can also be used manually, to convert a string to any of the basic C99 data types.

For example, to convert a string (in this case a string literal, "512") to int:

```C
int i;
int result = strtox("512", &i, STRTOI);
```

The function's return value depends on the conversion outcome:

Return value | Meaning
------------ | ------------
0            | Success
1            | The string is not convertible.
-1           | The string is too long.

Allowed values for parameter conversion_type:

Value     | Conversion type
--------- | ------------------
STRTOC    | char
STRTOSC   | signed char
STRTOUC   | unsigned char
STRTOS    | short
STRTOUS   | unsigned short
STRTOI    | int
STRTOUI   | unsigned int
STRTOL    | long
STRTOUL   | unsigned long
STRTOLL   | long long
STRTOULL  | unsigned long long
STRTOF    | float
STRTOD    | double
STRTOLD   | long double
STRTOB    | \_Bool
STRTOI8   | int8_t
STRTOUI8  | uint8_t
STRTOI16  | int16_t
STRTOUI16 | uint16_t
STRTOI32  | int32_t
STRTOUI32 | uint32_t
STRTOI64  | int64_t
STRTOUI64 | uint64_t

## Preprocessor directives

The following macros can be defined to disable features and to customize the help screen:

Macro                               | Default value | Description
----------------------------------- | ------------- | --------------------------
OPTPARSE_LONG_OPTIONS               | 1 (boolean)   | Enables/disables long options.
OPTPARSE_SUBCOMMANDS                | 1 (boolean)   | Enables/disables subcommands.
OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS | 1 (boolean)   | Enables/disables mutually exclusive options.
OPTPARSE_HIDDEN_OPTIONS             | 1 (boolean)   | Enables/disables hidden options.
OPTPARSE_FLOATING_POINT_SUPPORT     | 1 (boolean)   | Enables/disables floating point support.
OPTPARSE_C99_INTEGER_TYPES_SUPPORT  | 1 (boolean)   | Enables/disables C99 integer types support.
OPTPARSE_HELP_INDENTATION_WIDTH     | 2             | The help screen's indentation width, in characters.
OPTPARSE_HELP_MAX_DIVIDER_WIDTH     | 32            | Maximum distance between the help screen's left edge and option descriptions.
OPTPARSE_HELP_MAX_LINE_WIDTH        | 80            | Maximum line width for word wrapping.
OPTPARSE_HELP_USAGE_STYLE           | 1             | Style used for automatic usage generation; 0: short, 1: verbose.
OPTPARSE_HELP_USAGE_OPTIONS_STRING  | "OPTIONS"     | Placeholder string to be displayed if OPTPARSE_HELP_USAGE_STYLE is 0.
OPTPARSE_HELP_LETTER_CASE           | 0             | The help screen's letter case; 0: capitalized, 1: lower, 2: upper.
OPTPARSE_HELP_WORD_WRAP             | 1 (boolean)   | Enables/disables word wrap for lines longer than OPTPARSE_HELP_MAX_LINE_WIDTH.
OPTPARSE_HELP_FLOATING_DESCRIPTIONS | 1 (boolean)   | Defines how a description is to be printed if an option is longer than OPTPARSE_HELP_MAX_DIVIDER_WIDTH; 0: print description on a separate line, 1: print description on the same line, after a single indentation.
OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS | 1 (boolean) | Makes long options stay in a separate column even if there's no short option.

By disabling a feature, related code will not be compiled and structure members that are related to that feature will no longer be recognized.

# Notes

- To suppress compiler warnings, the function in an option's .function assignment can be prefixed with "(void (*)(void))".
- When implementing multiple option-arguments, the first option-argument should not be optional.
