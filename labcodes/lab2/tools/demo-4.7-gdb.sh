#!/usr/bin/env bash
# One-shot demo of lab2 section 4.7: host GDB watches QEMU's Sv39 walk while
# guest GDB reads the boot page table. Uses the --enable-debug QEMU build
# (not installed) so get_physical_address has a symbol and source lines.
set -euo pipefail

LAB_DIR="$(cd "$(dirname "$0")/.." && pwd)"
QEMU_SRC="/home/sidneylu0822/NKU_OS_Labwork/qemu-8.2.7"
QEMU="${QEMU_SRC}/build-debug/qemu-system-riscv64"
GDB_GUEST="${GDB_GUEST:-riscv64-unknown-elf-gdb}"
GDB_HOST="${GDB_HOST:-gdb}"
PORT="${PORT:-1234}"
LOG_DIR="${TMPDIR:-/tmp}/lab2-4.7-gdb"
mkdir -p "$LOG_DIR"
HOST_LOG="${LOG_DIR}/qemu-gdb.log"
GUEST_LOG="${LOG_DIR}/kernel-gdb.log"

if [[ ! -x "$QEMU" ]]; then
    echo "debug QEMU not found: $QEMU" >&2
    exit 1
fi
if ! command -v "$GDB_GUEST" >/dev/null 2>&1; then
    if [[ -x /home/sidneylu0822/riscv-elf-toolchains/bin/riscv64-unknown-elf-gdb ]]; then
        GDB_GUEST=/home/sidneylu0822/riscv-elf-toolchains/bin/riscv64-unknown-elf-gdb
    else
        echo "riscv64-unknown-elf-gdb not in PATH" >&2
        exit 1
    fi
fi

cd "$LAB_DIR"
make -s

VA="$(riscv64-unknown-elf-nm bin/kernel | awk '$3 == "boot_page_table_sv39" { print "0x"$1; exit }')"
if [[ -z "$VA" ]]; then
    echo "symbol boot_page_table_sv39 not found in bin/kernel" >&2
    exit 1
fi
# Sv39 1 GiB leaf at the root: PA = 0x80000000 + (VA & 0x3fffffff)
PA="$(python3 - "$VA" <<'PY'
import sys
va = int(sys.argv[1], 16)
print(f"0x{0x80000000 + (va & 0x3fffffff):x}")
PY
)"
VPN2=$(( (VA >> 30) & 0x1ff ))
VPN1=$(( (VA >> 21) & 0x1ff ))
VPN0=$(( (VA >> 12) & 0x1ff ))
OFF=$(( VA & 0xfff ))

echo "boot_page_table_sv39 VA=${VA}"
echo "  VPN[2]=${VPN2} VPN[1]=${VPN1} VPN[0]=${VPN0} offset=${OFF}"
echo "  expected PA of that virtual page base: ${PA}"

if ss -ltn | awk '{print $4}' | grep -q ":${PORT}$"; then
    echo "port ${PORT} is already in use" >&2
    exit 1
fi

qemu_pid=""
host_gdb_pid=""
cleanup() {
    if [[ -n "$host_gdb_pid" ]]; then
        kill "$host_gdb_pid" 2>/dev/null || true
        wait "$host_gdb_pid" 2>/dev/null || true
    fi
    if [[ -n "$qemu_pid" ]]; then
        kill "$qemu_pid" 2>/dev/null || true
        wait "$qemu_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT

HOST_CMDS="$(mktemp)"
GUEST_CMDS="$(mktemp)"
trap 'rm -f "$HOST_CMDS" "$GUEST_CMDS"; cleanup' EXIT

# Launch QEMU under host GDB. ptrace_scope on this machine rejects gdb -p,
# but a debugger may trace a process it started itself.
cat >"$HOST_CMDS" <<EOF
set pagination off
set confirm off
set height 0
set width 0
set logging file ${HOST_LOG}
set logging overwrite on
set logging enabled on
handle SIGPIPE nostop noprint pass
directory ${QEMU_SRC}/target/riscv
break cpu_helper.c:1153 if (addr & ~0xfff) == ${VA}
commands
  silent
  printf "\\n[qemu] get_physical_address VA=0x%lx\\n", addr
  printf "  satp = 0x%lx\\n", env->satp
  printf "  PTE  = 0x%lx\\n", pte
  printf "  PA   = 0x%lx\\n", *physical
  continue
end
run -machine virt -nographic -bios default -kernel bin/ucore.img -s -S
EOF

"$GDB_HOST" -q -x "$HOST_CMDS" "$QEMU" >"${LOG_DIR}/4.7-host-gdb.stdout" 2>&1 &
host_gdb_pid=$!
for _ in $(seq 1 100); do
    if ss -ltn | awk '{print $4}' | grep -q ":${PORT}$"; then
        break
    fi
    if ! kill -0 "$host_gdb_pid" 2>/dev/null; then
        echo "host gdb/QEMU exited early; see ${LOG_DIR}/4.7-host-gdb.stdout" >&2
        cat "${LOG_DIR}/4.7-host-gdb.stdout" >&2 || true
        exit 1
    fi
    sleep 0.1
done
if ! ss -ltn | awk '{print $4}' | grep -q ":${PORT}$"; then
    echo "QEMU gdb stub did not open port ${PORT}" >&2
    cat "${LOG_DIR}/4.7-host-gdb.stdout" >&2 || true
    exit 1
fi

cat >"$GUEST_CMDS" <<EOF
set pagination off
set confirm off
set logging file ${GUEST_LOG}
set logging overwrite on
set logging enabled on
file bin/kernel
set arch riscv:rv64
target remote localhost:${PORT}
break kern_init
continue
printf "\\n[kernel] stopped at kern_init, paging is already on\\n"
printf "boot page table VA ${VA}\\n"
printf "first PTE and the VPN[2]=${VPN2} entry:\\n"
x/gx ${VA}
x/gx ${VA}+${VPN2}*8
printf "\\n[kernel] pc after reaching kern_init\\n"
info registers pc
quit
EOF

# monitor may not exist on this stub; don't fail the whole script on it.
# The x/gx commands are what enter get_physical_address.
set +e
"$GDB_GUEST" -q -batch -x "$GUEST_CMDS" >"${LOG_DIR}/4.7-guest-gdb.stdout" 2>&1
guest_rc=$?
set -e

echo
echo "===== guest GDB ====="
cat "${LOG_DIR}/4.7-guest-gdb.stdout"
echo
echo "===== host GDB (QEMU page walk) ====="
if [[ -f "$HOST_LOG" ]]; then
    cat "$HOST_LOG"
else
    echo "(no host log yet)"
    cat "${LOG_DIR}/4.7-host-gdb.stdout" || true
fi

if [[ "$guest_rc" -ne 0 ]]; then
    echo "guest gdb exited ${guest_rc}" >&2
    exit "$guest_rc"
fi
