#!/usr/bin/env python3
"""Inject the Lab8 grading tests into a temporary copy of the student tree.

Usage: python3 tools/inject.py <temporary grading copy>

The grading target of the Makefile runs this script on a temporary copy of the
lab8 tree, never on the student's own sources. It makes three changes to the copy:

1. kern/process/proc.c: the body of user_main() is replaced by the test code of
   tools/test_lab8_user.h, which reads /canary through the file system and then
   starts sh.
2. disk0/canary: the binary file tools/canary.bin is copied into the disk0
   directory of the copy, to be packed into the SFS image by mksfs.
3. kern/fs/devs/dev_stdin.c: the console script of tools/test_lab8_stdin.h is
   placed at the start of dev_stdin_read(), so that sh receives the graded
   commands.

The script stops with an error if one of the files is missing or if the
injection has already been applied to the copy.
"""

import os
import sys


def die(message):
    """Print an error message with the [lab8 inject] prefix and exit with status 1."""
    print("[lab8 inject] ERROR: " + message)
    raise SystemExit(1)


if len(sys.argv) != 2:
    die("Usage: inject.py <temporary grading copy>")

root = os.path.abspath(sys.argv[1])
tools = os.path.dirname(os.path.abspath(__file__))

def read_text(path):
    """Return the whole content of a UTF-8 text file."""
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def write_text(path, text):
    """Replace the content of a UTF-8 text file with text."""
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def read_binary(path):
    """Return the whole content of a file as bytes."""
    with open(path, "rb") as f:
        return f.read()


def write_binary(path, data):
    """Replace the content of a file with the bytes in data."""
    with open(path, "wb") as f:
        f.write(data)


def function_body_span(text, anchor):
    """Return (body_start, body_end) for the function whose signature contains anchor.

    body_start is the index just after the opening brace of the function and
    body_end is the index of its matching closing brace. Braces inside strings,
    character literals and comments are skipped.
    """
    sig = text.find(anchor)
    if sig < 0:
        die("Cannot find the anchor '{}' in file".format(anchor))
    open_brace = text.find("{", sig + len(anchor))
    if open_brace < 0:
        die("Cannot find the body of '{}'".format(anchor))

    depth = 0
    i = open_brace
    n = len(text)
    while i < n:
        c = text[i]
        # line comment
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            i = text.find("\n", i)
            if i < 0:
                die("Unterminated line comment in '{}'".format(anchor))
            continue
        # block comment
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            end = text.find("*/", i + 2)
            if end < 0:
                die("Unterminated block comment in '{}'".format(anchor))
            i = end + 2
            continue
        # string or character literal
        if c == '"' or c == "'":
            quote = c
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    i += 1
                i += 1
            i += 1
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return open_brace + 1, i
        i += 1
    die("Unbalanced braces after the body of '{}'".format(anchor))


proc_path = os.path.join(root, "kern", "process", "proc.c")
stdin_path = os.path.join(root, "kern", "fs", "devs", "dev_stdin.c")
body_src = os.path.join(tools, "test_lab8_user.h")
stdin_src = os.path.join(tools, "test_lab8_stdin.h")
canary_src = os.path.join(tools, "canary.bin")
canary_dst = os.path.join(root, "disk0", "canary")

# Pre-condition: verify the skeleton is clean
proc_text = read_text(proc_path)
if "[Test 1] SFS Read" in proc_text:
    die("proc.c is already injected")
stdin_text = read_text(stdin_path)
if "lab8_script" in stdin_text:
    die("dev_stdin.c is already injected")

# Read test body and stdin body
if not os.path.exists(body_src):
    die("tools/test_lab8_user.h not found")
body = read_text(body_src).strip()

if not os.path.exists(stdin_src):
    die("tools/test_lab8_stdin.h not found")
stdin_body = read_text(stdin_src).strip()

# Replace the body of user_main()
start, end = function_body_span(proc_text, "user_main(void *arg)")
proc_text = proc_text[:start] + "\n" + body + "\n" + proc_text[end:]
write_text(proc_path, proc_text)

# Place the canary file
if not os.path.exists(canary_src):
    die("tools/canary.bin not found")
canary = read_binary(canary_src)
disk0 = os.path.join(root, "disk0")
if not os.path.isdir(disk0):
    os.makedirs(disk0)
write_binary(canary_dst, canary)

# Queue the graded commands on the console
stdin_sig = stdin_text.find("dev_stdin_read(char *buf, size_t len)")
if stdin_sig < 0:
    die("Cannot find dev_stdin_read anchor in dev_stdin.c")
open_brace = stdin_text.find("{", stdin_sig + len("dev_stdin_read(char *buf, size_t len)"))
if open_brace < 0:
    die("Cannot find the body of dev_stdin_read")
stdin_text = stdin_text[:open_brace + 1] + "\n" + stdin_body + "\n" + stdin_text[open_brace + 1:]
write_text(stdin_path, stdin_text)

print("lab8 injection complete")

