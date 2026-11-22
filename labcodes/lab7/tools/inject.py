#!/usr/bin/env python3
"""Inject Lab7 grading tests into a temporary student-tree copy.

Tests are inserted at function boundaries:
  * check_sync: function entry

The injector operates only on the temporary grading copy; the student's
original sources are never modified.

Usage: python3 tools/inject.py <temporary grading copy>
This script is run by the "grade" target of the Makefile on a copy of the student tree
(never on the working tree). It makes these changes in that copy:
  1. copies tools/test_lab7.c and test_lab7.h into kern/test/;
  2. adds "kern/test" to KINCLUDE and KSRCDIR in the Makefile so the tests are built;
  3. adds an #include of test_lab7.h to kern/sync/check_sync.c (after assert.h);
  4. inserts test_lab7_condvar() and test_lab7_philosopher(...) at the start of check_sync(),
     passing it the student's phi_test_condvar, phi_take_forks_condvar and phi_put_forks_condvar.
It stops with an error if a function or include anchor is missing or appears more than once, or if
the tree already contains test hooks (duplicate injection) or a kern/test directory.
"""

import os
import re
import shutil
import sys


def die(message):
    """Print an error prefixed with "[lab7 inject] ERROR:" and exit with status 1.

    Args:
        message: Text describing the problem.
    """
    print("[lab7 inject] ERROR: " + message)
    raise SystemExit(1)


# The script takes exactly one argument: the root of the temporary grading copy.
if len(sys.argv) != 2:
    die("Usage: inject.py <temporary grading copy>")

# root: absolute path of the grading copy to modify; tools: directory that holds the test sources.
root = os.path.abspath(sys.argv[1])
tools = os.path.dirname(os.path.abspath(__file__))


def read_text(path):
    """Return the whole content of a UTF-8 text file."""
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def write_text(path, text):
    """Overwrite a UTF-8 text file with @p text."""
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def find_function(text, name):
    """Locate the definition of function @p name in the C source @p text.

    The brace scan ignores comments, strings, and character literals, so the
    matching brace remains correct in functions containing nested blocks.

    Args:
        text: Full C source text to search.
        name: Name of the function to find; it must have exactly one definition.

    Returns:
        A tuple (declaration_start, opening_brace, closing_brace_end) of character
        offsets in text. closing_brace_end is the offset just after the closing brace.

    Exits with an error if the function is missing, defined more than once, or
    its closing brace cannot be found.
    """
    pattern = re.compile(
        r"(?ms)^[ \t]*"
        r"(?:static[ \t]+)?"
        r"[A-Za-z_][A-Za-z0-9_ \t\n\*]*?"
        r"\b" + re.escape(name) +
        r"[ \t\n]*\([^;{}]*\)[ \t\n]*\{"
    )
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        die("Function location is missing or ambiguous: " + name)

    match = matches[0]
    body_start = text.find("{", match.start(), match.end())
    depth = 0
    in_line_comment = False
    in_block_comment = False
    in_string = False
    in_char = False
    escaped = False

    for i in range(body_start, len(text)):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
        elif in_block_comment:
            if ch == "*" and nxt == "/":
                in_block_comment = False
        elif in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
        elif in_char:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == "'":
                in_char = False
        else:
            if ch == "/" and nxt == "/":
                in_line_comment = True
            elif ch == "/" and nxt == "*":
                in_block_comment = True
            elif ch == '"':
                in_string = True
            elif ch == "'":
                in_char = True
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return match.start(), body_start, i + 1

    die("Function is missing its matching closing brace: " + name)


def insert_include(path, header, anchor):
    """Add "#include <header>" on the line after the single "#include" of @p anchor.

    Args:
        path: C source file to modify in place.
        header: Header name to add, without the angle brackets.
        anchor: Header name whose #include line is the insertion point.

    Exits with an error if the anchor include is missing or appears more than once.
    """
    text = read_text(path)
    pattern = re.compile(
        r"(?m)^(?P<line>[ \t]*#include[ \t]*[<\"]"
        + re.escape(anchor)
        + r"[>\"][ \t]*)$"
    )
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        die("Include anchor is missing or ambiguous: " + anchor)
    match = matches[0]
    replacement = match.group("line") + "\n#include <" + header + ">"
    write_text(path, text[:match.start()] + replacement + text[match.end():])


def indent_code(code):
    """Indent each line of @p code by four spaces and surround it with newlines.

    Args:
        code: One or more lines of C code.

    Returns:
        The indented block, ready to insert inside a function body.
    """
    return "\n" + "\n".join("    " + line for line in code.splitlines()) + "\n"


def insert_at_function_start(path, name, code):
    """Insert @p code as the first statements of function @p name.

    Args:
        path: C source file to modify in place.
        name: Name of the function that receives the code.
        code: C statements to insert, one per line.
    """
    text = read_text(path)
    _, body_start, _ = find_function(text, name)
    insertion = indent_code(code)
    write_text(path, text[:body_start + 1] + insertion + text[body_start + 1:])


# Pre-conditions: the grading copy must be a clean student tree without test hooks.
makefile = os.path.join(root, "Makefile")
check_sync_c = os.path.join(root, "kern", "sync", "check_sync.c")
testdir = os.path.join(root, "kern", "test")

make_text = read_text(makefile)
check_text = read_text(check_sync_c)
for marker in ("test_lab7.h", "test_lab7_condvar", "test_lab7_philosopher"):
    if marker in make_text or marker in check_text:
        die("The skeleton already contains injected Lab 7 test markers")
if os.path.exists(testdir):
    die("The skeleton already contains kern/test; it must be clean")

# Copy test sources into kern/test/
os.makedirs(testdir)
for filename in ("test_lab7.c", "test_lab7.h"):
    src = os.path.join(tools, filename)
    if not os.path.exists(src):
        die("tools/{} not found".format(filename))
    shutil.copyfile(src, os.path.join(testdir, filename))

# Add kern/test to Makefile
for variable in ("KINCLUDE", "KSRCDIR"):
    match = re.search(
        r"(?m)^" + variable + r"[ \t]+\+=.*(?:\n[ \t]+[^\n]*)*",
        make_text,
    )
    if match is None:
        die("Makefile variable not found: " + variable)
    make_text = make_text[:match.end()] + "\n" + variable + "\t+= kern/test" + make_text[match.end():]
write_text(makefile, make_text)

# Add #include <test_lab7.h> to check_sync.c
insert_include(check_sync_c, "test_lab7.h", "assert.h")

# Inject tests at the start of check_sync()
insert_at_function_start(
    check_sync_c,
    "check_sync",
    "test_lab7_condvar();\n"
    "test_lab7_philosopher(phi_test_condvar, phi_take_forks_condvar, phi_put_forks_condvar);"
)

print("lab7 injection complete")
