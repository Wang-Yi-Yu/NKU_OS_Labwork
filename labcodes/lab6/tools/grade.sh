#!/bin/sh
#
# Lab6 autograder.
#
# Usage: tools/grade.sh [-v]
#   -v  verbose: show the build and QEMU output on the terminal, and stop at the first failed check.
#
# Flow: build the kernel (with the tests from tools/inject.py), run QEMU until the kernel reaches
# readline() (kernel monitor), then check the serial log saved in $qemu_out against a list of
# expected lines for each user program. A check passes only if every expected line is found.
# The score is the sum over checks, and the script exits with status 1 if it is not full marks.
# Lines with a leading "!" must NOT appear, and lines after "-" are regular expressions.

verbose=false
if [ "x$1" = "x-v" ]; then
    verbose=true
    out=/dev/stdout
    err=/dev/stderr
else
    out=/dev/null
    err=/dev/null
fi

## make & makeopts
if gmake --version > /dev/null 2>&1; then
    make=gmake;
else
    make=make;
fi

makeopts="--quiet --no-print-directory -j"

make_print() {
    echo `$make $makeopts print-$1`
}

## command tools
awk='awk'
bc='bc'
date='date'
grep='grep'
rm='rm -f'
sed='sed'

## symbol table
sym_table='obj/kernel.sym'

## gdb & gdbopts
gdb="$(make_print GDB)"
gdbport='1234'

gdb_in="$(make_print GRADE_GDB_IN)"

## qemu & qemuopts
qemu="$(make_print qemu)"

qemu_out="$(make_print GRADE_QEMU_OUT)"

if $qemu -nographic -help | grep -q '^-gdb'; then
    qemugdb="-gdb tcp::$gdbport"
else
    qemugdb="-s -p $gdbport"
fi

## default variables
default_timeout=30
default_pts=5

pts=5
part=0
part_pos=0
total=0
total_pos=0

## default functions
# update_score: add the current part to the running totals and reset the part.
update_score() {
    total=`expr $total + $part`
    total_pos=`expr $total_pos + $part_pos`
    part=0
    part_pos=0
}

get_time() {
    echo `$date +%s.%N 2> /dev/null`
}

show_part() {
    echo "Part $1 Score: $part/$part_pos"
    echo
    update_score
}

# show_final: print the total score and exit with status 1 if it is not full marks.
show_final() {
    update_score
    echo "Total Score: $total/$total_pos"
    if [ $total -lt $total_pos ]; then
        exit 1
    fi
}

show_time() {
    t1=$(get_time)
    time=`echo "scale=1; ($t1-$t0)/1" | $sed 's/.N/.0/g' | $bc 2> /dev/null`
    echo "(${time}s)"
}

show_build_tag() {
    echo "$1:" | $awk '{printf "%-24s ", $0}'
}

show_check_tag() {
    echo "$1:" | $awk '{printf "  -%-40s  ", $0}'
}

show_msg() {
    echo $1
    shift
    if [ $# -gt 0 ]; then
        echo -e "$@" | awk '{printf "   %s\n", $0}'
        echo
    fi
}

# pass: record a passed check, worth $pts points.
pass() {
    show_msg OK "$@"
    part=`expr $part + $pts`
    part_pos=`expr $part_pos + $pts`
}

# fail: record a failed check (the points are added to the maximum score, not to the earned score).
fail() {
    show_msg WRONG "$@"
    part_pos=`expr $part_pos + $pts`
}

# run_qemu: start QEMU in the background and, if a break function is set, wait for it in gdb.
run_qemu() {
    qemuextra=
    if [ "$brkfun" ]; then
        qemuextra="-S $qemugdb"
    fi

    if [ -z "$timeout" ] || [ $timeout -le 0 ]; then
        timeout=$default_timeout;
    fi

    t0=$(get_time)
    (
        ulimit -t $timeout
        exec $qemu -nographic $qemuopts -serial file:$qemu_out -monitor null -no-reboot $qemuextra
    ) > $out 2> $err &
    pid=$!

    sleep 1

    if [ -n "$brkfun" ]; then
        brkaddr=`$grep " $brkfun\$" $sym_table | $sed -e's/ .*$//g'`
        brkaddr_phys=`echo $brkaddr | sed "s/^c0/00/g"`
        (
            echo "target remote localhost:$gdbport"
            echo "break *0x$brkaddr"
            if [ "$brkaddr" != "$brkaddr_phys" ]; then
                echo "break *0x$brkaddr_phys"
            fi
            echo "continue"
        ) > $gdb_in

        $gdb -batch -nx -x $gdb_in > /dev/null 2>&1

        kill $pid > /dev/null 2>&1
    fi
}

# build_run: build the kernel (with optional DEFS), run it under QEMU and save the log.
build_run() {
    show_build_tag "$1"
    shift

    if $verbose; then
        echo "$make $@ ..."
    fi
    $make $makeopts $@ 'DEFS+=-DDEBUG_GRADE' > $out 2> $err

    if [ $? -ne 0 ]; then
        echo $make $@ failed
        exit 1
    fi

    run_qemu

    show_time

    cp $qemu_out .`echo $tag | tr '[:upper:]' '[:lower:]' | sed 's/ /_/g'`.log
}

check_result() {
    show_check_tag "$1"
    shift

    if [ ! -s $qemu_out ]; then
        sleep 4
    fi

    if [ ! -s $qemu_out ]; then
        fail > /dev/null
        echo 'no $qemu_out'
    else
        check=$1
        shift
        $check "$@"
    fi
}

# check_regexps: check each expected line in $qemu_out; '!' negates the next pattern and '-'
# makes it a regular expression (grep -E) instead of a fixed string (grep -F).
check_regexps() {
    okay=yes
    not=0
    reg=0
    error=
    for i do
        if [ "x$i" = "x!" ]; then
            not=1
        elif [ "x$i" = "x-" ]; then
            reg=1
        else
            if [ $reg -ne 0 ]; then
                $grep '-E' "^$i\$" $qemu_out > /dev/null
            else
                $grep '-F' "$i" $qemu_out > /dev/null
            fi
            found=$(($? == 0))
            if [ $found -eq $not ]; then
                if [ $found -eq 0 ]; then
                    msg="!! error: missing '$i'"
                else
                    msg="!! error: got unexpected line '$i'"
                fi
                okay=no
                if [ -z "$error" ]; then
                    error="$msg"
                else
                    error="$error\n$msg"
                fi
            fi
            not=0
            reg=0
        fi
    done
    if [ "$okay" = "yes" ]; then
        pass
    else
        fail "$error"
        if $verbose; then
            exit 1
        fi
    fi
}

# run_test: build one user program (-prog NAME), run it and check the output with -check.
run_test() {
    tag=
    prog=
    check=check_regexps
    while true; do
        select=
        case $1 in
            -tag|-prog)
                select=`expr substr $1 2 ${#1}`
                eval $select='$2'
                ;;
        esac
        if [ -z "$select" ]; then
            break
        fi
        shift
        shift
    done
    defs=
    while expr "x$1" : "x-D.*" > /dev/null; do
        defs="DEFS+='$1' $defs"
        shift
    done
    if [ "x$1" = "x-check" ]; then
        check=$2
        shift
        shift
    fi

    if [ -z "$prog" ]; then
        $make $makeopts touch > /dev/null 2>&1
        args="$defs"
    else
        if [ -z "$tag" ]; then
            tag="$prog"
        fi
        args="build-$prog $defs"
    fi

    build_run "$tag" "$args"

    check_result 'check result' "$check" "$@"
}

# quick_run: build and run the kernel without checking the output.
quick_run() {
    tag="$1"
    shift
    defs=
    while expr "x$1" : "x-D.*" > /dev/null; do
        defs="DEFS+='$1' $defs"
        shift
    done

    $make $makeopts touch > /dev/null 2>&1
    build_run "$tag" "$defs"
}

# quick_check: check the current $qemu_out against the given expected lines.
quick_check() {
    tag="$1"
    shift
    check_result "$tag" check_regexps "$@"
}

## kernel image
osimg=$(make_print ucoreimg)

## swap image
swapimg=$(make_print swapimg)

## set default qemu-options
qemuopts="-machine virt -nographic -bios default -kernel bin/ucore.img"

## set break-function, default is readline
brkfun=readline

# default_check: check the lines every test must print: the injected [Test RR Scheduler] and
# [Test Stride Scheduler] PASS lines (printed at the end of sched_init()), the program's own
# expected lines, the memory-management check messages and the timer setup line.
default_check() {
    pts=10
    quick_check 'RR Scheduler'                                  \
    '[Test RR Scheduler] PASS'

    pts=10
    quick_check 'Stride Scheduler'                              \
    '[Test Stride Scheduler] PASS'

    pts=30
    check_regexps "$@"

    pts=10
    quick_check 'check output'                                  \
    'memory management: default_pmm_manager'                    \
    'check_alloc_page() succeeded!'                             \
    'check_pgdir() succeeded!'                                  \
    'check_boot_pgdir() succeeded!'                             \
    'kmalloc_init() succeeded!'                                 \
    'check_vma_struct() succeeded!'                             \
    'check_vmm() succeeded.'                                    \
    '++ setup timer interrupts'
}

## check now!! priority runs five children with priorities 1..5 under the scheduler that
## sched_init() selected; the expected lines only check that the run completes.
run_test -prog 'priority'      -check default_check             \
        'kernel_execve: pid = 2, name = "priority".'            \
        'main: fork ok,now need to wait pids.'                  \
        'set priority to 5'                                     \
        'set priority to 4'                                     \
        'set priority to 3'                                     \
        'set priority to 2'                                     \
        'set priority to 1'                                     \
        'all user-mode processes have quit.'                    \
        'init check memory pass.'                               \
    ! - 'user panic at .*'


## print final-score
show_final
