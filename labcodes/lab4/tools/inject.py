#!/usr/bin/env python3
"""Inject Lab4 grading tests into a temporary student-tree copy.

Tests are inserted at function boundaries:
  * pmm_init: function tail
  * proc_init: function tail
  * init_main: function entry

The injector does not depend on implementation-specific statements such as
"ret = 0", comments, labels, or a particular final call in a function.

Usage: python3 tools/inject.py <temporary grading copy>
This script is run by the "grade" target of the Makefile on a copy of the student tree
(never on the working tree). It makes these changes in that copy:
  1. copies tools/test_lab4.c and test_lab4.h into kern/test/;
  2. adds "kern/test" to KINCLUDE and KSRCDIR in the Makefile so the tests are built;
  3. adds an #include of test_lab4.h to kern/mm/pmm.c and kern/process/proc.c;
  4. inserts test_lab4_pmm_basic() before the end of pmm_init();
     test_lab4_process_create() before the end of proc_init();
     test_lab4_schedule() at the start of init_main().
It stops with an error if a function or include anchor is missing or appears more than once, or if
the tree already contains test hooks (duplicate injection) or a leftover --wrap option.
"""

import os
import re
import shutil
import sys


def die(message):
    """Print an error prefixed with "[lab4 inject] ERROR:" and exit with status 1.

    Args:
        message: Text describing the problem.
    """
    print("[lab4 inject] ERROR: " + message)
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
    """Return (declaration_start, opening_brace, closing_brace_end).

    The brace scan ignores comments, strings, and character literals, so the
    matching brace remains correct in functions containing nested blocks.
    """
    pattern = re.compile(
        r"(?ms)^[ \t]*"
        r"(?:static[ \t]+)?"
        r"[A-Za-z_][A-Za-z0-9_ \t\r\n\*]*?"
        r"\b" + re.escape(name) +
        r"[ \t\r\n]*\([^;{}]*\)[ \t\r\n]*\{"
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
    """Add "#include <header>" on the line after the existing "#include <anchor>" line.

    Args:
        path: File to modify.
        header: Header name to add, e.g. "test_lab4.h".
        anchor: Existing header name to search for, e.g. "pmm.h". It must occur exactly once.
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
    """Return @p code as a block of statements indented by four spaces, with blank lines around it.

    Args:
        code: One or more C statements, one per line.
    """
    return "\n" + "\n".join("    " + line for line in code.splitlines()) + "\n"


def insert_at_function_start(path, name, code):
    """Insert @p code as the first statements of function @p name.

    Args:
        path: C source file to modify.
        name: Function name; it must be defined exactly once in the file.
        code: C statements to insert.
    """
    text = read_text(path)
    _, body_start, _ = find_function(text, name)
    insertion = indent_code(code)
    write_text(path, text[:body_start + 1] + insertion + text[body_start + 1:])


def insert_at_function_end(path, name, code):
    """Insert @p code just before the closing brace of function @p name.

    Args:
        path: C source file to modify.
        name: Function name; it must be defined exactly once in the file.
        code: C statements to insert; they run after the student's code, before the function returns.
    """
    text = read_text(path)
    _, _, body_end = find_function(text, name)
    insertion = indent_code(code)
    write_text(path, text[:body_end - 1] + insertion + text[body_end - 1:])


# Step 1: refuse to run on a tree that already has the test hooks or a --wrap setup.
makefile = os.path.join(root, "Makefile")
make = read_text(makefile)
if "kern/test" in make or "test_lab4.h" in make:
    die("The skeleton already contains test hooks; duplicate injection is not allowed")
if "--wrap=" in make:
    die("Detected leftover --wrap configuration; the grading skeleton must use direct injection")

# Step 2: copy the test sources into kern/test/ of the grading copy.
testdir = os.path.join(root, "kern", "test")
os.makedirs(testdir, exist_ok=True)
for name in ("test_lab4.c", "test_lab4.h"):
    src = os.path.join(tools, name)
    if not os.path.exists(src):
        die("tools/ not found".format(name))
    shutil.copyfile(src, os.path.join(testdir, name))

# Step 3: add kern/test to the include path and to the source directories of the Makefile.
for variable in ("KINCLUDE", "KSRCDIR"):
    match = re.search(
        r"(?m)^" + variable + r"[ \t]*\+=.*(?:\n[ \t]+[^\n]*)*",
        make,
    )
    if match is None:
        die("Makefile variable not found: " + variable)
    make = make[:match.end()] + "\n" + variable + "\t+= kern/test" + make[match.end():]
write_text(makefile, make)

# Step 4: include the test header and call each test from the student's functions.
pmm = os.path.join(root, "kern", "mm", "pmm.c")
proc = os.path.join(root, "kern", "process", "proc.c")
insert_include(pmm, "test_lab4.h", "pmm.h")
insert_include(proc, "test_lab4.h", "proc.h")
insert_at_function_end(pmm, "pmm_init", "test_lab4_pmm_basic();")
insert_at_function_end(proc, "proc_init", "test_lab4_process_create();")
insert_at_function_start(proc, "init_main", "test_lab4_schedule();")

print("lab4 injection complete")