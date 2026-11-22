#!/bin/sh
#
# Lab7 autograder.
#
# Usage: tools/grade.sh
#
# Flow: `make grade` copies the tree to a temporary directory and runs tools/inject.py on the copy,
# which makes kern/sync/check_sync.c call the tests in tools/test_lab7.c first. This script then
# builds that copy with -DDEBUG_GRADE, runs QEMU for at most 40 seconds with the serial port saved
# in a file, and looks for one result line per test in that file. Each test is worth 50 points, and
# the script exits with status 1 if the total is not 100.
#
#   [Test 1] Condition Variable: PASS        cond_wait / cond_signal (kern/sync/monitor.c)
#   [Test 2] Philosopher Monitor: PASS       phi_*_condvar          (kern/sync/check_sync.c)

set -u

make_cmd=make
if gmake --version >/dev/null 2>&1; then
    make_cmd=gmake
fi

output=.qemu.out
score=0

echo "Building Lab7 tests..."
if ! "$make_cmd" --quiet --no-print-directory clean; then
    echo "  - build: FAIL (clean)"
    echo "Total Score: 0/100"
    exit 1
fi

if ! "$make_cmd" --quiet --no-print-directory "DEFS+=-DDEBUG_GRADE"; then
    echo "  - build: FAIL"
    echo "Total Score: 0/100"
    exit 1
fi

# QEMU 8.2 (OpenSBI v1.3, fw_dynamic) must be given the kernel with -kernel. The old
# "-device loader,file=bin/ucore.img,addr=0x80200000" leaves "Domain0 Next Address" at 0, so the
# kernel would never start.
echo "Running Lab7 tests..."
rm -f "$output"
timeout 40s qemu-system-riscv64 \
    -machine virt -nographic -bios default \
    -kernel bin/ucore.img \
    -serial file:"$output" -monitor none -no-reboot >/dev/null 2>&1 || true

if grep -F "[Test 1] Condition Variable: PASS" "$output" >/dev/null 2>&1; then
    echo "  - condition variable: PASS (50/50)"
    score=$((score + 50))
else
    echo "  - condition variable: FAIL (0/50)"
fi

if grep -F "[Test 2] Philosopher Monitor: PASS" "$output" >/dev/null 2>&1; then
    echo "  - philosopher monitor: PASS (50/50)"
    score=$((score + 50))
else
    echo "  - philosopher monitor: FAIL (0/50)"
fi

echo "Total Score: $score/100"
[ "$score" -eq 100 ]
