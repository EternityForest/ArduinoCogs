#!/usr/bin/env python3
#
# Preprocessor that makes asserts easier to debug.
#
# Example:
# ./scripts/prettyasserts.py -p LFS_ASSERT lfs.c -o lfs.a.c
#
# Copyright (c) 2022, The littlefs authors.
# Copyright (c) 2020, Arm Limited. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

import re
import sys

# NOTE the use of macros here helps keep a consistent stack depth which
# tools may rely on.
#
# If compilation errors are noisy consider using -ftrack-macro-expansion=0.
#

LIMIT = 16

CMP = {
    "==": "eq",
    "!=": "ne",
    "<=": "le",
    ">=": "ge",
    "<": "lt",
    ">": "gt",
}

LEXEMES = {
    "ws": [r"(?:\s|\n|#.*?\n|//.*?\n|/\*.*?\*/)+"],
    "assert": ["assert"],
    "arrow": ["=>"],
    "string": [r'"(?:\\.|[^"])*"', r"'(?:\\.|[^'])\'"],
    "paren": [r"\(", r"\)"],
    "cmp": CMP.keys(),
    "logic": [r"\&\&", r"\|\|"],
    "sep": [":", ";", r"\{", r"\}", ","],
    "op": ["->"],  # specifically ops that conflict with cmp
}


def openio(path, mode="r", buffering=-1):
    # allow '-' for stdin/stdout
    if path == "-":
        if mode == "r":
            return os.fdopen(os.dup(sys.stdin.fileno()), mode, buffering)
        else:
            return os.fdopen(os.dup(sys.stdout.fileno()), mode, buffering)
    else:
        return open(path, mode, buffering)


def write_header(f, limit=LIMIT):
    f.writeln("// Generated by %s:" % sys.argv[0])
    f.writeln("//")
    f.writeln("// %s" % " ".join(sys.argv))
    f.writeln("//")
    f.writeln()

    f.writeln("#include <stdbool.h>")
    f.writeln("#include <stdint.h>")
    f.writeln("#include <inttypes.h>")
    f.writeln("#include <stdio.h>")
    f.writeln("#include <string.h>")
    f.writeln("#include <signal.h>")
    # give source a chance to define feature macros
    f.writeln("#undef _FEATURES_H")
    f.writeln()

    # write print macros
    f.writeln("__attribute__((unused))")
    f.writeln("static void __pretty_assert_print_bool(")
    f.writeln("        const void *v, size_t size) {")
    f.writeln("    (void)size;")
    f.writeln('    printf("%s", *(const bool*)v ? "true" : "false");')
    f.writeln("}")
    f.writeln()
    f.writeln("__attribute__((unused))")
    f.writeln("static void __pretty_assert_print_int(")
    f.writeln("        const void *v, size_t size) {")
    f.writeln("    (void)size;")
    f.writeln('    printf("%"PRIiMAX, *(const intmax_t*)v);')
    f.writeln("}")
    f.writeln()
    f.writeln("__attribute__((unused))")
    f.writeln("static void __pretty_assert_print_mem(")
    f.writeln("        const void *v, size_t size) {")
    f.writeln("    const uint8_t *v_ = v;")
    f.writeln('    printf("\\"");')
    f.writeln("    for (size_t i = 0; i < size && i < %d; i++) {" % limit)
    f.writeln("        if (v_[i] >= ' ' && v_[i] <= '~') {")
    f.writeln('            printf("%c", v_[i]);')
    f.writeln("        } else {")
    f.writeln('            printf("\\\\x%02x", v_[i]);')
    f.writeln("        }")
    f.writeln("    }")
    f.writeln("    if (size > %d) {" % limit)
    f.writeln('        printf("...");')
    f.writeln("    }")
    f.writeln('    printf("\\"");')
    f.writeln("}")
    f.writeln()
    f.writeln("__attribute__((unused))")
    f.writeln("static void __pretty_assert_print_str(")
    f.writeln("        const void *v, size_t size) {")
    f.writeln("    __pretty_assert_print_mem(v, size);")
    f.writeln("}")
    f.writeln()
    f.writeln("__attribute__((unused, noinline))")
    f.writeln("static void __pretty_assert_fail(")
    f.writeln("        const char *file, int line,")
    f.writeln("        void (*type_print_cb)(const void*, size_t),")
    f.writeln("        const char *cmp,")
    f.writeln("        const void *lh, size_t lsize,")
    f.writeln("        const void *rh, size_t rsize) {")
    f.writeln('    printf("%s:%d:assert: assert failed with ", file, line);')
    f.writeln("    type_print_cb(lh, lsize);")
    f.writeln('    printf(", expected %s ", cmp);')
    f.writeln("    type_print_cb(rh, rsize);")
    f.writeln('    printf("\\n");')
    f.writeln("    fflush(NULL);")
    f.writeln("    raise(SIGABRT);")
    f.writeln("}")
    f.writeln()

    # write assert macros
    for op, cmp in sorted(CMP.items()):
        f.writeln("#define __PRETTY_ASSERT_BOOL_%s(lh, rh) do { \\" % cmp.upper())
        f.writeln("    bool _lh = !!(lh); \\")
        f.writeln("    bool _rh = !!(rh); \\")
        f.writeln("    if (!(_lh %s _rh)) { \\" % op)
        f.writeln("        __pretty_assert_fail( \\")
        f.writeln("                __FILE__, __LINE__, \\")
        f.writeln('                __pretty_assert_print_bool, "%s", \\' % cmp)
        f.writeln("                &_lh, 0, \\")
        f.writeln("                &_rh, 0); \\")
        f.writeln("    } \\")
        f.writeln("} while (0)")
    for op, cmp in sorted(CMP.items()):
        f.writeln("#define __PRETTY_ASSERT_INT_%s(lh, rh) do { \\" % cmp.upper())
        f.writeln("    __typeof__(lh) _lh = lh; \\")
        f.writeln("    __typeof__(lh) _rh = rh; \\")
        f.writeln("    if (!(_lh %s _rh)) { \\" % op)
        f.writeln("        __pretty_assert_fail( \\")
        f.writeln("                __FILE__, __LINE__, \\")
        f.writeln('                __pretty_assert_print_int, "%s", \\' % cmp)
        f.writeln("                &(intmax_t){_lh}, 0, \\")
        f.writeln("                &(intmax_t){_rh}, 0); \\")
        f.writeln("    } \\")
        f.writeln("} while (0)")
    for op, cmp in sorted(CMP.items()):
        f.writeln("#define __PRETTY_ASSERT_MEM_%s(lh, rh, size) do { \\" % cmp.upper())
        f.writeln("    const void *_lh = lh; \\")
        f.writeln("    const void *_rh = rh; \\")
        f.writeln("    if (!(memcmp(_lh, _rh, size) %s 0)) { \\" % op)
        f.writeln("        __pretty_assert_fail( \\")
        f.writeln("                __FILE__, __LINE__, \\")
        f.writeln('                __pretty_assert_print_mem, "%s", \\' % cmp)
        f.writeln("                _lh, size, \\")
        f.writeln("                _rh, size); \\")
        f.writeln("    } \\")
        f.writeln("} while (0)")
    for op, cmp in sorted(CMP.items()):
        f.writeln("#define __PRETTY_ASSERT_STR_%s(lh, rh) do { \\" % cmp.upper())
        f.writeln("    const char *_lh = lh; \\")
        f.writeln("    const char *_rh = rh; \\")
        f.writeln("    if (!(strcmp(_lh, _rh) %s 0)) { \\" % op)
        f.writeln("        __pretty_assert_fail( \\")
        f.writeln("                __FILE__, __LINE__, \\")
        f.writeln('                __pretty_assert_print_str, "%s", \\' % cmp)
        f.writeln("                _lh, strlen(_lh), \\")
        f.writeln("                _rh, strlen(_rh)); \\")
        f.writeln("    } \\")
        f.writeln("} while (0)")
    f.writeln()
    f.writeln()


def mkassert(type, cmp, lh, rh, size=None):
    if size is not None:
        return "__PRETTY_ASSERT_%s_%s(%s, %s, %s)" % (
            type.upper(),
            cmp.upper(),
            lh,
            rh,
            size,
        )
    else:
        return "__PRETTY_ASSERT_%s_%s(%s, %s)" % (type.upper(), cmp.upper(), lh, rh)


# simple recursive descent parser
class ParseFailure(Exception):
    def __init__(self, expected, found):
        self.expected = expected
        self.found = found

    def __str__(self):
        return "expected %r, found %s..." % (self.expected, repr(self.found)[:70])


class Parser:
    def __init__(self, in_f, lexemes=LEXEMES):
        p = "|".join("(?P<%s>%s)" % (n, "|".join(l)) for n, l in lexemes.items())
        p = re.compile(p, re.DOTALL)
        data = in_f.read()
        tokens = []
        line = 1
        col = 0
        while True:
            m = p.search(data)
            if m:
                if m.start() > 0:
                    tokens.append((None, data[: m.start()], line, col))
                tokens.append((m.lastgroup, m.group(), line, col))
                data = data[m.end() :]
            else:
                tokens.append((None, data, line, col))
                break
        self.tokens = tokens
        self.off = 0

    def lookahead(self, *pattern):
        if self.off < len(self.tokens):
            token = self.tokens[self.off]
            if token[0] in pattern or token[1] in pattern:
                self.m = token[1]
                return self.m
        self.m = None
        return self.m

    def accept(self, *patterns):
        m = self.lookahead(*patterns)
        if m is not None:
            self.off += 1
        return m

    def expect(self, *patterns):
        m = self.accept(*patterns)
        if not m:
            raise ParseFailure(patterns, self.tokens[self.off :])
        return m

    def push(self):
        return self.off

    def pop(self, state):
        self.off = state


def p_assert(p):
    state = p.push()

    # assert(memcmp(a,b,size) cmp 0)?
    try:
        p.expect("assert")
        p.accept("ws")
        p.expect("(")
        p.accept("ws")
        p.expect("memcmp")
        p.accept("ws")
        p.expect("(")
        p.accept("ws")
        lh = p_expr(p)
        p.accept("ws")
        p.expect(",")
        p.accept("ws")
        rh = p_expr(p)
        p.accept("ws")
        p.expect(",")
        p.accept("ws")
        size = p_expr(p)
        p.accept("ws")
        p.expect(")")
        p.accept("ws")
        cmp = p.expect("cmp")
        p.accept("ws")
        p.expect("0")
        p.accept("ws")
        p.expect(")")
        return mkassert("mem", CMP[cmp], lh, rh, size)
    except ParseFailure:
        p.pop(state)

    # assert(strcmp(a,b) cmp 0)?
    try:
        p.expect("assert")
        p.accept("ws")
        p.expect("(")
        p.accept("ws")
        p.expect("strcmp")
        p.accept("ws")
        p.expect("(")
        p.accept("ws")
        lh = p_expr(p)
        p.accept("ws")
        p.expect(",")
        p.accept("ws")
        rh = p_expr(p)
        p.accept("ws")
        p.expect(")")
        p.accept("ws")
        cmp = p.expect("cmp")
        p.accept("ws")
        p.expect("0")
        p.accept("ws")
        p.expect(")")
        return mkassert("str", CMP[cmp], lh, rh)
    except ParseFailure:
        p.pop(state)

    # assert(a cmp b)?
    try:
        p.expect("assert")
        p.accept("ws")
        p.expect("(")
        p.accept("ws")
        lh = p_expr(p)
        p.accept("ws")
        cmp = p.expect("cmp")
        p.accept("ws")
        rh = p_expr(p)
        p.accept("ws")
        p.expect(")")
        return mkassert("int", CMP[cmp], lh, rh)
    except ParseFailure:
        p.pop(state)

    # assert(a)?
    p.expect("assert")
    p.accept("ws")
    p.expect("(")
    p.accept("ws")
    lh = p_exprs(p)
    p.accept("ws")
    p.expect(")")
    return mkassert("bool", "eq", lh, "true")


def p_expr(p):
    res = []
    while True:
        if p.accept("("):
            res.append(p.m)
            while True:
                res.append(p_exprs(p))
                if p.accept("sep"):
                    res.append(p.m)
                else:
                    break
            res.append(p.expect(")"))
        elif p.lookahead("assert"):
            state = p.push()
            try:
                res.append(p_assert(p))
            except ParseFailure:
                p.pop(state)
                res.append(p.expect("assert"))
        elif p.accept("string", "op", "ws", None):
            res.append(p.m)
        else:
            return "".join(res)


def p_exprs(p):
    res = []
    while True:
        res.append(p_expr(p))
        if p.accept("cmp", "logic", ","):
            res.append(p.m)
        else:
            return "".join(res)


def p_stmt(p):
    ws = p.accept("ws") or ""

    # memcmp(lh,rh,size) => 0?
    if p.lookahead("memcmp"):
        state = p.push()
        try:
            p.expect("memcmp")
            p.accept("ws")
            p.expect("(")
            p.accept("ws")
            lh = p_expr(p)
            p.accept("ws")
            p.expect(",")
            p.accept("ws")
            rh = p_expr(p)
            p.accept("ws")
            p.expect(",")
            p.accept("ws")
            size = p_expr(p)
            p.accept("ws")
            p.expect(")")
            p.accept("ws")
            p.expect("=>")
            p.accept("ws")
            p.expect("0")
            p.accept("ws")
            return ws + mkassert("mem", "eq", lh, rh, size)
        except ParseFailure:
            p.pop(state)

    # strcmp(lh,rh) => 0?
    if p.lookahead("strcmp"):
        state = p.push()
        try:
            p.expect("strcmp")
            p.accept("ws")
            p.expect("(")
            p.accept("ws")
            lh = p_expr(p)
            p.accept("ws")
            p.expect(",")
            p.accept("ws")
            rh = p_expr(p)
            p.accept("ws")
            p.expect(")")
            p.accept("ws")
            p.expect("=>")
            p.accept("ws")
            p.expect("0")
            p.accept("ws")
            return ws + mkassert("str", "eq", lh, rh)
        except ParseFailure:
            p.pop(state)

    # lh => rh?
    lh = p_exprs(p)
    if p.accept("=>"):
        rh = p_exprs(p)
        return ws + mkassert("int", "eq", lh, rh)
    else:
        return ws + lh


def main(input=None, output=None, pattern=[], limit=LIMIT):
    with openio(input or "-", "r") as in_f:
        # create parser
        lexemes = LEXEMES.copy()
        lexemes["assert"] += pattern
        p = Parser(in_f, lexemes)

        with openio(output or "-", "w") as f:

            def writeln(s=""):
                f.write(s)
                f.write("\n")

            f.writeln = writeln

            # write extra verbose asserts
            write_header(f, limit=limit)
            if input is not None:
                f.writeln('#line %d "%s"' % (1, input))

            # parse and write out stmt at a time
            try:
                while True:
                    f.write(p_stmt(p))
                    if p.accept("sep"):
                        f.write(p.m)
                    else:
                        break
            except ParseFailure as e:
                print("warning: %s" % e)

            for i in range(p.off, len(p.tokens)):
                f.write(p.tokens[i][1])


if __name__ == "__main__":
    import argparse
    import sys

    parser = argparse.ArgumentParser(
        description="Preprocessor that makes asserts easier to debug.",
        allow_abbrev=False,
    )
    parser.add_argument("input", help="Input C file.")
    parser.add_argument("-o", "--output", required=True, help="Output C file.")
    parser.add_argument(
        "-p",
        "--pattern",
        action="append",
        help="Regex patterns to search for starting an assert statement. This"
        ' implicitly includes "assert" and "=>".',
    )
    parser.add_argument(
        "-l",
        "--limit",
        type=lambda x: int(x, 0),
        default=LIMIT,
        help="Maximum number of characters to display in strcmp and memcmp. "
        "Defaults to %r." % LIMIT,
    )
    sys.exit(
        main(
            **{
                k: v
                for k, v in vars(parser.parse_intermixed_args()).items()
                if v is not None
            }
        )
    )
