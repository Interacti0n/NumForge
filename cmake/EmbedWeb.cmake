if(NOT DEFINED NUMFORGE_WEB_SOURCE_DIR OR NOT DEFINED NUMFORGE_WEB_OUTPUT OR
   NOT DEFINED NUMFORGE_WEB_VERSION)
    message(FATAL_ERROR "EmbedWeb.cmake needs NUMFORGE_WEB_SOURCE_DIR, NUMFORGE_WEB_OUTPUT and NUMFORGE_WEB_VERSION")
endif()

get_filename_component(numforge_web_output_dir "${NUMFORGE_WEB_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${numforge_web_output_dir}")
set(numforge_web_generated
    "#ifndef NUMFORGE_GENERATED_WEB_PAGE_H\n"
    "#define NUMFORGE_GENERATED_WEB_PAGE_H\n\n"
    "#include <stddef.h>\n\n"
    "/* Generated from web/ sources at build time. Edit those files, not this header. */\n\n")
string(JOIN "" numforge_web_generated ${numforge_web_generated})

macro(numforge_embed_web_file symbol filename)
    file(READ "${NUMFORGE_WEB_SOURCE_DIR}/${filename}" contents)
    string(REPLACE "@NUMFORGE_VERSION@" "${NUMFORGE_WEB_VERSION}" contents "${contents}")
    string(HEX "${contents}" contents)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${contents}")
    string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],){16})" "\\1\n" bytes "${bytes}")
    string(APPEND numforge_web_generated "static const unsigned char ${symbol}[] = {\n${bytes}0x00\n};\n\n")
endmacro()

numforge_embed_web_file(NUMFORGE_CALCULATOR_SK_HTML calculator.sk.html)
numforge_embed_web_file(NUMFORGE_CALCULATOR_EN_HTML calculator.en.html)
numforge_embed_web_file(NUMFORGE_API_SK_HTML api.sk.html)
numforge_embed_web_file(NUMFORGE_API_EN_HTML api.en.html)
numforge_embed_web_file(NUMFORGE_UPCOMING_SK_HTML upcoming.sk.html)
numforge_embed_web_file(NUMFORGE_UPCOMING_EN_HTML upcoming.en.html)
numforge_embed_web_file(NUMFORGE_CALCULATOR_CSS calculator.css)
numforge_embed_web_file(NUMFORGE_API_CSS api.css)
numforge_embed_web_file(NUMFORGE_CHROME_CSS chrome.css)
numforge_embed_web_file(NUMFORGE_NAVIGATION_JS navigation.js)
numforge_embed_web_file(NUMFORGE_CALCULATOR_JS calculator.js)
numforge_embed_web_file(NUMFORGE_UNITS_SK_HTML units.sk.html)
numforge_embed_web_file(NUMFORGE_UNITS_EN_HTML units.en.html)
numforge_embed_web_file(NUMFORGE_UNITS_CSS units.css)
numforge_embed_web_file(NUMFORGE_UNITS_JS units.js)
numforge_embed_web_file(NUMFORGE_LICENSE_TEXT ../LICENSE)

# CMake strings cannot hold NUL bytes, so read PNG assets directly as hex.
macro(numforge_embed_web_binary symbol filename)
    file(READ "${NUMFORGE_WEB_SOURCE_DIR}/${filename}" contents HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${contents}")
    string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],){16})" "\\1\n" bytes "${bytes}")
    string(APPEND numforge_web_generated
        "static const unsigned char ${symbol}[] = {\n${bytes}0x00\n};\n\n")
endmacro()

numforge_embed_web_binary(NUMFORGE_LOGO_PNG logo.png)
numforge_embed_web_binary(NUMFORGE_WORDMARK_PNG wordmark.png)

string(APPEND numforge_web_generated
    "static const char *const NUMFORGE_UNITS_PAGE[] = {(const char *)NUMFORGE_UNITS_SK_HTML, NULL};\n"
    "static const char *const NUMFORGE_UNITS_PAGE_EN[] = {(const char *)NUMFORGE_UNITS_EN_HTML, NULL};\n"
    "static const char *const NUMFORGE_WEB_PAGE[] = {(const char *)NUMFORGE_CALCULATOR_SK_HTML, NULL};\n"
    "static const char *const NUMFORGE_WEB_PAGE_EN[] = {(const char *)NUMFORGE_CALCULATOR_EN_HTML, NULL};\n"
    "static const char *const NUMFORGE_API_PAGE[] = {(const char *)NUMFORGE_API_SK_HTML, NULL};\n"
    "static const char *const NUMFORGE_API_PAGE_EN[] = {(const char *)NUMFORGE_API_EN_HTML, NULL};\n"
    "static const char *const NUMFORGE_UPCOMING_PAGE[] = {(const char *)NUMFORGE_UPCOMING_SK_HTML, NULL};\n"
    "static const char *const NUMFORGE_UPCOMING_PAGE_EN[] = {(const char *)NUMFORGE_UPCOMING_EN_HTML, NULL};\n\n"
    "#endif\n")

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef numforge_web_suffix)
set(numforge_web_temporary "${NUMFORGE_WEB_OUTPUT}.${numforge_web_suffix}.tmp")
file(WRITE "${numforge_web_temporary}" "${numforge_web_generated}")
file(RENAME "${numforge_web_temporary}" "${NUMFORGE_WEB_OUTPUT}")
