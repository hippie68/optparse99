#optparse99 cmake list

cmake_minimum_required(VERSION 3.28, FATAL_ERROR)

project(optparse99 LANGUAGES C)

option(OPT_OPTPARSE_LONG_OPTIONS "Enables/disables long options." ON)
option(OPT_OPTPARSE_SUBCOMMANDS "Enables/disables subcommands." ON)
option(OPT_OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS "Enables/disables mutually exclusive options." ON)
option(OPT_OPTPARSE_HIDDEN_OPTIONS "Enables/disables hidden options." ON)
option(OPT_OPTPARSE_ATTACHED_OPTION_ARGUMENTS "Enables/disables attached option-arguments (-oarg, --option=arg). Note: if disabled, optional option-arguments can only be detected during manual parsing." ON)
option(OPT_OPTPARSE_LIST_SUPPORT "Enables/disables support for option-arguments in list form." ON)
option(OPT_OPTPARSE_FLOATING_POINT_SUPPORT "Enables/disables floating point support." ON)
option(OPT_OPTPARSE_C99_INTEGER_TYPES_SUPPORT "Enables/disables C99 integer types support." ON)
set(OPT_OPTPARSE_HELP_INDENTATION_WIDTH "2" CACHE STRING "The help screen's indentation width, in characters.")
set(OPT_OPTPARSE_HELP_MAX_DIVIDER_WIDTH "32" CACHE STRING "Maximum distance between the help screen's left edge and option descriptions.")
set(OPT_OPTPARSE_HELP_MAX_LINE_WIDTH "80" CACHE STRING "Maximum line width for word wrapping.")
set(OPT_OPTPARSE_HELP_USAGE_STYLE "0" CACHE STRING "Style used for automatic usage generation; 0: short, 1: verbose.")
set(OPT_OPTPARSE_HELP_USAGE_OPTIONS_STRING "OPTIONS" CACHE STRING "Placeholder string to be displayed if OPTPARSE_HELP_USAGE_STYLE is 0.")
set(OPT_OPTPARSE_HELP_LETTER_CASE "0" CACHE STRING "The help screen's letter case; 0: capitalized, 1: lower, 2: upper.")
option(OPT_OPTPARSE_HELP_WORD_WRAP "Enables/disables word wrap for lines longer than OPTPARSE_HELP_MAX_LINE_WIDTH." ON)
option(OPT_OPTPARSE_HELP_FLOATING_DESCRIPTIONS "Defines how a description is to be printed if an option is longer than OPTPARSE_HELP_MAX_DIVIDER_WIDTH; 0: print description on a separate line, 1: print description on the same line, after a single indentation." ON)
option(OPT_OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS "Makes long options stay in a separate column even if there's no short option." ON)
option(OPT_OPTPARSE_PRINT_HELP_ON_ERROR "Prints the currently active command's help screen if there's a parsing error." ON)
set(OPT_OPTPARSE_MUTUALLY_EXCLUSIVE_GROUPS_MAX "8" CACHE STRING "The maximum amount of groups for mutually exclusive options.")
set(OPT_OPTPARSE_PRINT_BUFFER_SIZE "1024" CACHE STRING "The size of the buffer used for printing functionality of optparse99 such as printing help and usage.")

option(OPTPARSE99_STATIC "Build static library." ON)
if(OPTPARSE99_STATIC)
    set(OPTPARSE99_BUILD_TYPE STATIC)
    set(OPTPARSE99_LINK_TYPE ARCHIVE)
else()
    set(OPTPARSE99_BUILD_TYPE SHARED)
    set(OPTPARSE99_LINK_TYPE LIBRARY)
endif()

add_library(optparse99 ${OPTPARSE99_BUILD_TYPE} optparse99.h optparse99.c)

target_include_directories(optparse99 PUBLIC ${PROJECT_SOURCE_DIR})

target_compile_features(optparse99
    PRIVATE
        c_std_99
        c_function_prototypes)

set_target_properties(optparse99
    PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON)

target_compile_definitions(optparse99
    PUBLIC
        OPTPARSE_LONG_OPTIONS=$<IF:$<BOOL:${OPT_OPTPARSE_LONG_OPTIONS}>,true,false>
        OPTPARSE_SUBCOMMANDS=$<IF:$<BOOL:${OPT_OPTPARSE_SUBCOMMANDS}>,true,false>
        OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS=$<IF:$<BOOL:${OPT_OPTPARSE_MUTUALLY_EXCLUSIVE_OPTIONS}>,true,false>
        OPTPARSE_HIDDEN_OPTIONS=$<IF:$<BOOL:${OPT_OPTPARSE_HIDDEN_OPTIONS}>,true,false>
        OPTPARSE_ATTACHED_OPTION_ARGUMENTS=$<IF:$<BOOL:${OPT_OPTPARSE_ATTACHED_OPTION_ARGUMENTS}>,true,false>
        OPTPARSE_LIST_SUPPORT=$<IF:$<BOOL:${OPT_OPTPARSE_LIST_SUPPORT}>,true,false>
        OPTPARSE_FLOATING_POINT_SUPPORT=$<IF:$<BOOL:${OPT_OPTPARSE_FLOATING_POINT_SUPPORT}>,true,false>
        OPTPARSE_C99_INTEGER_TYPES_SUPPORT=$<IF:$<BOOL:${OPT_OPTPARSE_C99_INTEGER_TYPES_SUPPORT}>,true,false>
        OPTPARSE_HELP_INDENTATION_WIDTH=${OPT_OPTPARSE_HELP_INDENTATION_WIDTH}
        OPTPARSE_HELP_MAX_DIVIDER_WIDTH=${OPT_OPTPARSE_HELP_MAX_DIVIDER_WIDTH}
        OPTPARSE_HELP_MAX_LINE_WIDTH=${OPT_OPTPARSE_HELP_MAX_LINE_WIDTH}
        OPTPARSE_HELP_USAGE_STYLE=${OPT_OPTPARSE_HELP_USAGE_STYLE}
        OPTPARSE_HELP_USAGE_OPTIONS_STRING="${OPT_OPTPARSE_HELP_USAGE_OPTIONS_STRING}"
        OPTPARSE_HELP_LETTER_CASE=${OPT_OPTPARSE_HELP_LETTER_CASE}
        OPTPARSE_HELP_WORD_WRAP=$<IF:$<BOOL:${OPT_OPTPARSE_HELP_WORD_WRAP}>,true,false>
        OPTPARSE_HELP_FLOATING_DESCRIPTIONS=$<IF:$<BOOL:${OPT_OPTPARSE_HELP_FLOATING_DESCRIPTIONS}>,true,false>
        OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS=$<IF:$<BOOL:${OPT_OPTPARSE_HELP_UNIQUE_COLUMN_FOR_LONG_OPTIONS}>,true,false>
        OPTPARSE_PRINT_HELP_ON_ERROR=$<IF:$<BOOL:${OPT_OPTPARSE_PRINT_HELP_ON_ERROR}>,true,false>
        OPTPARSE_MUTUALLY_EXCLUSIVE_GROUPS_MAX=${OPT_OPTPARSE_MUTUALLY_EXCLUSIVE_GROUPS_MAX}
        OPTPARSE_PRINT_BUFFER_SIZE=${OPT_OPTPARSE_PRINT_BUFFER_SIZE})

install(TARGETS optparse99
    ${OPTPARSE99_LINK_TYPE}
    PUBLIC_HEADER)
