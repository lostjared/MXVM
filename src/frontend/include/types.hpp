#ifndef __TYPES__H_HPP
#define __TYPES__H_HPP

#include<iostream>

namespace mxx {

    enum class Keywords {
        AND,
        ARRAY,
        BEGIN,
        CASE,
        CONST,
        DIV,
        DO,
        DOWNTO,
        ELSE,
        END,
        FILE,
        FOR,
        FUNCTION,
        GOTO,
        IF,
        IN,
        LABEL,
        MOD,
        NIL,
        NOT,
        OF,
        OR,
        PACKED,
        PROCEDURE,
        PROGRAM,
        RECORD,
        REPEAT,
        SET,
        THEN,
        TO,
        TYPE,
        UNTIL,
        VAR,
        WHILE,
        WITH
    };

    static const char *key_str[] = {
        "and",
        "array",
        "begin",
        "case",
        "const",
        "div",
        "do",
        "downto",
        "else",
        "end",
        "file",
        "for",
        "function",
        "goto",
        "if",
        "in",
        "label",
        "mod",
        "nil",
        "not",
        "of",
        "or",
        "packed",
        "procedure",
        "program",
        "record",
        "repeat",
        "set",
        "then",
        "to",
        "type",
        "until",
        "var",
        "while",
        "with"
    };



}




#endif