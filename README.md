A C99+ option parser.

# Features:
  - Supports both short and long options.
    - Short options are POSIX-compliant (-o ARG, -oARG, -asdfoARG).
    - Long options follow the GNU standard (--option ARG, --option=ARG).
  - The order of options and operands (non-options) does not matter.
  - Supports the "end of options" delimiter (--).
  - Can set integer flags (true, false, increment, decrement).
  - Can type-convert and store option-arguments.
  - Supports subcommands and nested subcommands.
  - Options and commands/subcommands can call functions ("callbacks") with or without arguments.
  - Mutually exclusive options.
  - A nicely-formatted, customizable help screen with word-wrapping.
  - Provides functions for easy manual parsing (e.g. to implement multiple option-arguments).
  - Provides function "strtox()" for manual type-conversion.
  - Features can be toggled to only compile necessary code.

The code is still considered work-in-progress: it may be rough around the edges and have bugs. If you like the ideas and want to help polish them or report bugs, please create an issue at https://github.com/hippie68/optparse99/issues.

# Index
- [Basic example](#basic-example)
- [Documentation](#documentation)
  - [Command structure](#command-structure)
  - [Option structure](#option-structure)
  - [Functions](#functions)
    - [Manual parsing](#manual-parsing)
    - [Manual type-converting](#manual-type-converting)
  - [Preprocessor directives](#preprocessor-directives)

# Basic example

```C
#include "optparse99.h"

#include <stdint.h> // For uint16_t
#include <stdio.h>
#include <stdlib.h> // For exit()

int verbose;
uint16_t bufsize = 4096;
char *filename;

void check_bufsize(uint16_t bufsize)
{
    if (bufsize < 4096) {
        fprintf(stderr, "Buffer size too low: %u\n", bufsize);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    // 1. Define the command tree and its options.
    struct optparse_cmd main_cmd = {
        .about = "Supertool v1.00 - A really handy tool.",
        .description = "This is an example program that uses a basic optparse99 setup. Variables like \"FILENAME\" serve no purpose other than to demonstrate option behavior. After parsing, the program will print remaining operands and variables' final state.",
        .name = "supertool",
        .operands = "OPERAND [OPERAND...]", // At least 1 operand is required.
        .options = (struct optparse_opt []) {
            // Call the built-in help screen function.
            {
                .short_name = 'h',
                .long_name = "help",
                .description = "Print help information and quit.",
                .function = optparse_print_help,
            },
            // Set a flag.
            {
                .long_name = "verbose",
                .description = "Increase verbosity.",
                .flag = &verbose,
            },
            // Use a user-provided option-argument to set a string.
            {
                .short_name = 'f',
                .long_name = "file",
                .description = "Set a file name.",
                .arg_name = "FILENAME",
                .arg_dest = &filename,
            },
            // Convert a user-provided option-argument to data type uint16_t and
            // check if the converted number is inside an allowed range.
            // (The check could also be done, perhaps together with other
            // checks, by employing main_cmd's .function, or after parsing.)
            {
                .short_name = 'b',
                .long_name = "bufsize",
                .description = "Change the file buffer size. Allowed range: 4096-65535 (default: 4096).",
                .arg_name = "BUFSIZE",
                .arg_data_type = DATA_TYPE_UINT16,
                .arg_dest = &bufsize,
                .function = (void (*)(void)) check_bufsize,
            },
            { END_OF_OPTIONS },
        },
    };

    // 2. After defining the command tree, parse the command line arguments.
    optparse_parse(&main_cmd, &argc, &argv);

    // At this point, optparse99 is done. Variables have been set, and argc and
    // argv have been altered to contain operands only.

    // Print remaining operands.
    if (argc > 1) {
        for (int i = 0; i <= argc; i++) {
            printf("argv[%d]: %s\n", i, argv[i] ? argv[i] : "NULL");
        }
    } else {
        fprintf(stderr, "At least one operand is required.\n");
        optparse_fprint_usage(stderr);
        exit(EXIT_FAILURE);
    }

    // Print state of variables.
    printf("verbose: %d\n", verbose);
    printf("filename: %s\n", filename ? filename : "NULL");
    printf("bufsize: %u\n", bufsize);
}
```

Help screen output:

```
$ supertool --help
Supertool v1.00 - A really handy tool.
Usage: supertool [OPTIONS] OPERAND [OPERAND...]

This is an example program that uses a basic optparse99 setup. Variables like
"FILENAME" serve no purpose other than to demonstrate option behavior. After
parsing, the program will print remaining operands and variables' final state.

Options:
  -h, --help             Print help information and quit.
      --verbose          Increase verbosity.
  -f, --file FILENAME    Set a file name.
  -b, --bufsize BUFSIZE  Change the file buffer size. Allowed range: 4096-65535
                         (default: 4096).
```

# Documentation

Optparse99 is based on two types of structures: commands and options. Each command can have a set of options and a set of subcommands, the latter which can be nested tree-like. The root of the tree is always the program's command itself.  
Command and option structures are to be thought of as sets of instructions. Most structure members are optional and, by making use of C99's designated initializers, do not need to be specified. Non-specified members are initialized with 0 (NULL), which for enumeration types is the default value.

Both options and subcommands are arrays that are constructed by compound literals and must end with the element { END_OF_OPTIONS } and { END_OF_SUBCOMMANDS }, respectively:

```C
struct optparse_cmd main_cmd = {
    .options = (struct optparse_opt []) {
        { ... },
        { END_OF_OPTIONS },
    },
    .subcommands = (struct optparse_cmd []) {
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
    struct optparse_cmd *_parent;
};
```

Structure member   | Description
------------------ | ------------------
`.name` (required) | The command line string users enter to run the command.
`.about`           | A short sentence, describing the command's purpose in a nutshell. May also contain information like version number, homepage, etc.
`.description`     | The command's detailed documentation.
`.operands`        | The command's operands (aka "positional arguments") as to be displayed in the help screen.
`.usage`           | Can be specified to override automatic usage generation, e.g. if operands depend on options.
`.function`        | Once the command's options have been parsed, the command will call the specified function, using the current state of argc and argv as function arguments.
`.options`         | Points to an array containing the command's options.
`.subcommands`     | Point to an array containing the command's subcommands.

Members starting with an underscore ("_") are for internal use only and should be ignored.

## Option structure

```C
struct optparse_opt {
    char short_name;
    char *long_name;
    char *arg_name;
    enum optparse_data_type arg_data_type;
    void *arg_dest;
    int *flag;
    enum optparse_flag_type flag_type;
    void (*function)(void);
    enum optparse_function_type function_type;
    int group;
    _Bool hidden;
    char *description;
};
```

Structure member          | Description
------------------------- | -------------------------
`.short_name` (required*) | The short option character.
`.long_name` (required*)  | The long option string (without leading "--").
`.arg_name`               | If specified, it means the option has one or more option-arguments. The string is displayed as-is in the help screen. If it begins with "\[", the option-argument is regarded as optional.
`.arg_data_type`          | If set, the parsed option-argument (char *) will be converted to a different data type.
`.arg_dest`               | The memory location the (type-converted) option-argument is saved to.
`.flag`                   | A pointer to an integer variable that is to be used as specified by .flag_type.
`.flag_type`              | Specifies what to do to with the flag variable's value.
`.function`               | Points to a function that is called as specified in .function_type. The pointer can be cast to void (*)(void) to suppress compiler warnings.
`.function_type`          | Tells .function which argument to use.
`.group`                  | Options that share the same group value are treated as mutually exclusive.
`.hidden`                 | If true, the option won't be displayed in the help screen.
`.description`            | The option's description, whether short or in-depth.
* At least one of them must be specified.

### Allowed values for .arg_data_type

Value                     | Conversion type
------------------------- | ------------------
`DATA_TYPE_STR` (default) | (no conversion)
`DATA_TYPE_CHAR`          | char
`DATA_TYPE_SCHAR`         | signed char
`DATA_TYPE_UCHAR`         | unsigned char
`DATA_TYPE_SHRT`          | short
`DATA_TYPE_USHRT`         | unsigned short
`DATA_TYPE_INT`           | int
`DATA_TYPE_UINT`          | unsigned int
`DATA_TYPE_LONG`          | long
`DATA_TYPE_ULONG`         | unsigned long
`DATA_TYPE_LLONG`         | long long
`DATA_TYPE_ULLONG`        | unsigned long long
`DATA_TYPE_FLT`           | float
`DATA_TYPE_DBL`           | double
`DATA_TYPE_LDBL`          | long double
`DATA_TYPE_BOOL`          | \_Bool
`DATA_TYPE_INT8`          | int8_t
`DATA_TYPE_UINT8`         | uint8_t
`DATA_TYPE_INT16`         | int16_t
`DATA_TYPE_UINT16`        | uint16_t
`DATA_TYPE_INT32`         | int32_t
`DATA_TYPE_UINT32`        | uint32_t
`DATA_TYPE_INT64`         | int64_t
`DATA_TYPE_UINT64`        | uint64_t

### Allowed values for .flag_type

Value                          | Result
------------------------------ | ------------------------------
`FLAG_TYPE_SET_TRUE` (default) | Set the variable to 1.
`FLAG_TYPE_SET_FALSE`          | Set the variable to 0.
`FLAG_TYPE_INCREMENT`          | Increase the variable's current value by 1.
`FLAG_TYPE_DECREMENT`          | Decrease the variable's current value by 1.

### Allowed values for .function_type

Value                          | Function declaration and internal call
------------------------------ | --------------------------------------
`FUNCTION_TYPE_AUTO` (default) | Automatically decide (default)
`FUNCTION_TYPE_TARG`           | TARG means "type-converted option-argument" (type as specified in .arg_data_type).<br>declaration: void f(DATA_TYPE);<br>call: f(TARG);
`FUNCTION_TYPE_OARG`           | OARG means "original option-argument".<br>declaration: void f(char *);<br>call: f(OARG);
`FUNCTION_TYPE_VOID`           | declaration: void f(void);<br>call: f();

How automatic decision works:
   - if .arg_name is set:
     - if .arg_data_type is set: FUNCTION_TYPE_TARG
     - else:                     FUNCTION_TYPE_OARG
   - else:                       FUNCTION_TYPE_VOID

Functions refered to by .function must be of return type void, and their declaration must match the one specified by .function_type.

## Functions

```C
void optparse_parse(optparse_cmd *cmd, int *argc, char ***argv);
```

optparse_parse() starts the parsing process. It must be called before calling any other optparse_ function. After parsing, the main() function's argc and argv will contain only operands.  
- *cmd: a pointer to the command tree's root command  
- *argc: a pointer to main()'s argc variable  
- ***argv: a pointer to main()'s argv variable

```C
void optparse_print_help(void);
```

Prints the currently active command's help information. It can be called manually or through an option's .function member - see the [quick example](#quick-example) above. Exits with exit status EXIT_SUCCESS.

```C
void optparse_fprint_help(FILE *stream, int exit_status);
```

Same as optparse_print_help(), but can only be called manually. It prints to the specified stream and exits with the provided exit status.

```C
void optparse_fprint_usage(FILE *stream);
```

Prints the currently active command's usage information only. 

```C
void optparse_print_help_subcmd(int argc, char **argv);
```

Prints a subcommand's help information by parsing remaining operands.
optparse_print_help_subcmd() must be called while the remaining command line arguments represent a valid subcommand chain, e.g. as a subcommand's function:

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

It is possible to manually parse arguments from inside an option's callback function (.function).
Inside that function, the following functions can be used:

```C
char *optparse_shift(void);
char *optparse_unshift(void);
```

optparse_shift() returns the next command line argument, while optparse_unshift() puts it back. Please note that optparse_unshift() is only guaranteed to undo the most recent shift.
Using this technique, you can parse multiple option-arguments while having full control over what exactly qualifies as a valid option-argument and when to stop.

The callback function must have the single parameter char *, and .function_type must be FUNCTION_TYPE_OARG. The function's argument will be the first command line argument, so it must be checked prior to using optparse_shift().
E.g. to make an option eat and print all remaining command line arguments:

```C
void print_arg(char *arg) {
    while (arg != NULL) {
        printf("option-argument: %s\n", arg);
        arg = optparse_shift();
    }
}
```

Or, in general:

```C
void print_arg(char *arg) {
    if (arg == NULL) {
        ... // Code that is run if a required argument is missing.
        return;
    }

    do {
        ... // Code that is run for each argument.
        arg = optparse_shift();
    } while (arg != NULL);
}

```

Note: when implementing multiple option-arguments, the first option-argument must not be optional unless OPTPARSE_ATTACHED_OPTION_ARGUMENTS (see [Preprocessor directives](#preprocessor-directives)) is set to false.

### Manual type-converting

The function used internally for type-converting option-arguments is strtox():

```C
int strtox(char *str, void *x, enum optparse_data_type data_type);
```

It can also be used manually, to convert a string to any of the basic C99 data types.
For example, to convert a string (in this case a string literal, "512") to int:

```C
int i;
int retval = strtox("512", &i, DATA_TYPE_INT);
```

The function's return value depends on the conversion outcome:

Return value | Meaning
------------ | ------------
0            | Success
1            | The string is not convertible.
-1           | The converted data is out of range.

## Preprocessor directives

The following macros can be defined to disable features and to customize the help screen:

Macro                                 | Default value | Description
------------------------------------- | ------------- | ----------------------------
`OPTPARSE_LONG_OPTIONS`               | 1 (boolean)   | Enables/disables long options.
`OPTPARSE_SUBCOMMANDS`                | 1 (boolean)   | Enables/disables subcommands.
`OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS` | 1 (boolean)   | Enables/disables mutually exclusive options.
`OPTPARSE_HIDDEN_OPTIONS`             | 1 (boolean)   | Enables/disables hidden options.
`OPTPARSE_ATTACHED_OPTION_ARGUMENTS`  | 1 (boolean)   | Enables/disables attached option-arguments (-oarg, --option=arg). Note: if disabled, optional option-arguments can only be detected during manual parsing.
`OPTPARSE_FLOATING_POINT_SUPPORT`     | 1 (boolean)   | Enables/disables floating point support.
`OPTPARSE_C99_INTEGER_TYPES_SUPPORT`  | 1 (boolean)   | Enables/disables C99 integer types support.
`OPTPARSE_HELP_INDENTATION_WIDTH`     | 2             | The help screen's indentation width, in characters.
`OPTPARSE_HELP_MAX_DIVIDER_WIDTH`     | 32            | Maximum distance between the help screen's left edge and option descriptions.
`OPTPARSE_HELP_MAX_LINE_WIDTH`        | 80            | Maximum line width for word wrapping.
`OPTPARSE_HELP_USAGE_STYLE`           | 1             | Style used for automatic usage generation; 0: short, 1: verbose.
`OPTPARSE_HELP_USAGE_OPTIONS_STRING`  | "OPTIONS"     | Placeholder string to be displayed if OPTPARSE_HELP_USAGE_STYLE is 0.
`OPTPARSE_HELP_LETTER_CASE`           | 0             | The help screen's letter case; 0: capitalized, 1: lower, 2: upper.
`OPTPARSE_HELP_WORD_WRAP`             | 1 (boolean)   | Enables/disables word wrap for lines longer than OPTPARSE_HELP_MAX_LINE_WIDTH.
`OPTPARSE_HELP_FLOATING_DESCRIPTIONS` | 1 (boolean)   | Defines how a description is to be printed if an option is longer than OPTPARSE_HELP_MAX_DIVIDER_WIDTH; 0: print description on a separate line, 1: print description on the same line, after a single indentation.
`OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS` | 1 (boolean) | Makes long options stay in a separate column even if there's no short option.
`OPTPARSE_PRINT_HELP_ON_ERROR`        | 1 (boolean)   | Prints the currently active command's help screen if there's a parsing error.

By disabling a feature, related code will not be compiled and structure members that are related to that feature will no longer be recognized.

When building releases, please define NDEBUG to remove assert()-related code.
