#!/usr/bin/env python3
"""malloc_arena_report.py:

Report how much of the libc malloc arena the firmware uses at boot, and how
much is left, without flashing a board.

The arena is invisible to the linker map: Zephyr's common libc malloc hands
F Prime every byte of SRAM between the end of static RAM (`_end`) and the top
of SRAM, and F Prime fills it at boot with component queues, buffer manager
pools and sequencer buffers. A change that grows static RAM, a queue, or a
buffer pool can run the arena dry, and that only shows up as a boot assert on
hardware.

This script runs the real zephyr.elf boot path, `main()` up to the rate group
loop, in a Cortex-M emulator (Unicorn). Kernel calls (`z_impl_k_*`) return 0
and peripheral registers read as zero, but every F Prime allocation runs the
target's own code against the target's own sys_heap, so the sizes and the
heap overhead are the ones the board sees. It also stops on any FW_ASSERT
during setup, which catches boot-time overflows such as a full command
dispatch table.

What it cannot see: allocations made after boot (none are expected, the
runtime should take buffers from buffer managers) and code paths that depend
on real hardware answers during setup.

Usage:
    python3 scripts/malloc_arena_report.py [path/to/zephyr.elf] [--min-headroom BYTES]

Exit status:
    0 - setup completed and free arena >= --min-headroom
    1 - free arena below --min-headroom, or an allocation failed
    2 - FW_ASSERT during setup
    3 - emulation did not reach the rate group loop (crash or hang)
"""

import argparse
import bisect
import sys
from dataclasses import dataclass
from pathlib import Path

from elftools.elf.elffile import ELFFile
from unicorn import (
    UC_ARCH_ARM,
    UC_HOOK_CODE,
    UC_HOOK_MEM_UNMAPPED,
    UC_MODE_MCLASS,
    UC_MODE_THUMB,
    Uc,
    UcError,
)
from unicorn.arm_const import (
    UC_ARM_REG_LR,
    UC_ARM_REG_PC,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_SP,
)

DEFAULT_ELF = Path("build-fprime-automatic-zephyr/zephyr/zephyr.elf")

# libc entry points into the arena, and the register holding the requested size.
ARENA_ENTRY_POINTS = {
    "malloc": UC_ARM_REG_R0,
    "aligned_alloc": UC_ARM_REG_R1,
    "memalign": UC_ARM_REG_R1,
    "realloc": UC_ARM_REG_R1,
}
# SYS_INIT entries that set up the arena or redirect allocations away from it.
# Driver inits are skipped: they would only talk to absent hardware.
SOFTWARE_SYS_INITS = ("__init_malloc_prepare", "__init__mbedtls_init")
BOOT_DONE = "_ZN19ReferenceDeployment15startRateGroupsEv"
SW_ASSERT_PREFIX = "_ZN2Fw8SwAssertEPKc"

# Address-space windows mapped as plain RAM. Peripheral windows read as zero.
MEMORY = [
    (0x10000000, 0x01000000),  # XIP flash
    (0x20000000, 0x00090000),  # SRAM
    (0x40000000, 0x00200000),  # APB peripherals
    (0x50000000, 0x00200000),  # AHB peripherals
    (0xD0000000, 0x00100000),  # SIO
    (0xE0000000, 0x00100000),  # PPB
    (0x0F000000, 0x00001000),  # return trampoline
]
RETURN_ADDR = 0x0F000100
# Kernel calls return 0 except these, which must look successful to Os::Task.
# Threads never run, so the fake stack is never touched.
FAKE_STACK = 0x0F000800
KERNEL_RETURNS = {
    "z_impl_k_thread_stack_alloc": lambda r0: FAKE_STACK,
    "z_impl_k_thread_create": lambda r0: r0,
}
INSTRUCTION_BUDGET = 200_000_000
CHUNK_UNIT = 8


def demangle(name: str) -> str:
    """Nested-name prefix of an Itanium symbol (`_ZN7CdhCore7cmdDispE` -> `CdhCore::cmdDisp`)."""
    if not name.startswith("_ZN"):
        return name
    parts, i = [], 3
    while i < len(name) and name[i].isdigit():
        j = i
        while name[j].isdigit():
            j += 1
        n = int(name[i:j])
        parts.append(name[j : j + n])
        i = j + n
    return "::".join(parts) if parts else name


@dataclass
class Allocation:
    """One arena allocation made during setup, attributed to the global object that asked for it."""

    owner: str
    site: str
    requested: int
    heap_bytes: int = 0
    ptr: int = 0
    freed: bool = False


@dataclass
class Report:
    """Arena size, use and free space after setup, with every allocation."""

    sram_bytes: int
    static_bytes: int
    arena_bytes: int
    used_bytes: int
    free_bytes: int
    largest_free: int
    allocations: list[Allocation]


class BootFailure(Exception):
    """Setup did not complete; `code` is the script's exit status."""

    def __init__(self, code: int, message: str):
        super().__init__(message)
        self.code = code


class Firmware:
    """Symbols and loadable sections of zephyr.elf."""

    def __init__(self, elf_path: Path):
        with elf_path.open("rb") as f:
            elf = ELFFile(f)
            symtab = elf.get_section_by_name(".symtab")
            self.syms: dict[str, int] = {}
            funcs, objs = [], []
            for s in symtab.iter_symbols():
                if not s.name:
                    continue
                self.syms.setdefault(s.name, s["st_value"])
                kind = s["st_info"]["type"]
                if kind == "STT_FUNC" and s["st_value"]:
                    funcs.append((s["st_value"] & ~1, s.name))
                elif (
                    kind == "STT_OBJECT"
                    and s["st_size"] >= 16
                    and 0x20000000 <= s["st_value"] < 0x20090000
                ):
                    objs.append((s["st_value"], s["st_size"], s.name))
            self.sections = [
                (
                    sec["sh_addr"],
                    sec["sh_type"] == "SHT_NOBITS",
                    sec["sh_size"],
                    sec.data() if sec["sh_type"] != "SHT_NOBITS" else b"",
                )
                for sec in elf.iter_sections()
                if sec["sh_flags"] & 2 and sec["sh_size"]
            ]
        self.funcs = sorted(funcs)
        self.func_addrs = [a for a, _ in self.funcs]
        self.objs = sorted(objs)
        self.obj_addrs = [a for a, _, _ in self.objs]

    def addr(self, name: str) -> int:
        return self.syms[name] & ~1

    def function_at(self, pc: int) -> int:
        """Start address of the function containing `pc`."""
        return self.funcs[bisect.bisect_right(self.func_addrs, pc) - 1][0]

    def describe(self, pc: int) -> str:
        """Readable name of the function containing `pc`."""
        i = bisect.bisect_right(self.func_addrs, pc) - 1
        return demangle(self.funcs[i][1]) if i >= 0 else hex(pc)

    def object_containing(self, ptr: int) -> str | None:
        """Name of the global object (not an allocator) that `ptr` points into."""
        i = bisect.bisect_right(self.obj_addrs, ptr) - 1
        if i >= 0 and self.objs[i][0] <= ptr < self.objs[i][0] + self.objs[i][1]:
            name = self.objs[i][2]
            return None if "allocator" in name.lower() else demangle(name)
        return None


class Emulator:
    """One emulated boot. `entry_hooks` lists functions whose (this, return address) to record."""

    def __init__(self, fw: Firmware, entry_hooks: set[int]):
        self.fw = fw
        self.uc = uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        for base, size in MEMORY:
            uc.mem_map(base, size)
        for addr, nobits, size, data in fw.sections:
            uc.mem_write(addr, b"\0" * size if nobits else data)

        self.stack_top = fw.syms["__kernel_ram_end"] - 16
        self.entries: dict[int, tuple[int, int]] = {}
        self.allocations: list[Allocation] = []
        self.unresolved: set[int] = set()
        self.pending_return: int | None = None
        self.return_hooks: set[int] = set()
        self.failure: BootFailure | None = None
        self.boot_done = False

        for name, value in fw.syms.items():
            if name.startswith("z_impl_k_") and value & 1:
                self._hook(
                    value & ~1,
                    self._stub_kernel_call(KERNEL_RETURNS.get(name, lambda r0: 0)),
                )
            elif name.startswith(SW_ASSERT_PREFIX):
                self._hook(value & ~1, self._on_assert)
        if "free" in fw.syms:
            self._hook(fw.addr("free"), self._on_free)
        for name, size_reg in ARENA_ENTRY_POINTS.items():
            if name in fw.syms:
                self._hook(fw.addr(name), self._on_allocate, size_reg)
        self._hook(fw.addr(BOOT_DONE), self._on_boot_done)
        for f in entry_hooks:
            self._hook(f, self._on_entry)
        uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._on_unmapped)

    def _hook(self, addr: int, fn, user=None) -> None:
        self.uc.hook_add(UC_HOOK_CODE, fn, user_data=user, begin=addr, end=addr)

    def _reg(self, r: int) -> int:
        return self.uc.reg_read(r)

    def _stub_kernel_call(self, result):
        """Hook that returns `result(r0)` from a kernel call without running it."""

        def hook(uc, address, size, user) -> None:
            uc.reg_write(UC_ARM_REG_R0, result(self._reg(UC_ARM_REG_R0)))
            uc.reg_write(UC_ARM_REG_PC, self._reg(UC_ARM_REG_LR))

        return hook

    def _on_entry(self, uc, address, size, user) -> None:
        self.entries[address] = (
            self._reg(UC_ARM_REG_R0),
            self._reg(UC_ARM_REG_LR) & ~1,
        )

    def _owner(self, caller_pc: int) -> tuple[str, str]:
        """Walk recorded entries up the call chain to the first call whose `this` lies inside a named global object.

        Returns that object and the function called on it.
        """
        pc = caller_pc
        for _ in range(16):
            f = self.fw.function_at(pc)
            if f not in self.entries:
                self.unresolved.add(f)
                return "?", "?"
            this, ret = self.entries[f]
            owner = self.fw.object_containing(this)
            if owner:
                return owner, self.fw.describe(f)
            pc = ret
        return "?", "?"

    def _on_allocate(self, uc, address, size, user) -> None:
        """Record the request and arrange to read the returned pointer."""
        if self.pending_return is not None:
            return  # realloc or memalign re-entering the arena for the same request
        requested = self._reg(user)
        caller = (self._reg(UC_ARM_REG_LR) & ~1) - 2
        self.allocations.append(Allocation(*self._owner(caller), requested))
        ret = self._reg(UC_ARM_REG_LR) & ~1
        self.pending_return = ret
        if ret not in self.return_hooks:
            self.return_hooks.add(ret)
            self._hook(ret, self._on_allocate_return)

    def _on_allocate_return(self, uc, address, size, user) -> None:
        if address != self.pending_return:
            return
        self.pending_return = None
        alloc = self.allocations[-1]
        ptr = self._reg(UC_ARM_REG_R0)
        if ptr == 0:
            self.failure = BootFailure(
                1,
                f"allocation of {alloc.requested} bytes for {alloc.owner} returned NULL",
            )
            uc.emu_stop()
            return
        alloc.ptr = ptr
        alloc.heap_bytes = self._chunk_bytes_of(ptr)

    def _on_free(self, uc, address, size, user) -> None:
        ptr = self._reg(UC_ARM_REG_R0)
        for alloc in reversed(self.allocations):
            if alloc.ptr == ptr and not alloc.freed:
                alloc.freed = True
                return

    def _on_assert(self, uc, address, size, user) -> None:
        """Stop on FW_ASSERT and name the file and caller."""
        file = (
            bytes(uc.mem_read(self._reg(UC_ARM_REG_R0), 128))
            .split(b"\0")[0]
            .decode(errors="replace")
        )
        caller = self.fw.describe((self._reg(UC_ARM_REG_LR) & ~1) - 2)
        self.failure = BootFailure(
            2, f"FW_ASSERT in {file or '?'} (called from {caller}) during setup"
        )
        uc.emu_stop()

    def _on_boot_done(self, uc, address, size, user) -> None:
        self.boot_done = True
        uc.emu_stop()

    def _on_unmapped(self, uc, access, addr, size, value, user) -> bool:
        self.failure = BootFailure(
            3,
            f"access to unmapped {addr:#x} in {self.fw.describe(self._reg(UC_ARM_REG_PC))}",
        )
        return False

    def call(self, addr: int) -> None:
        """Run the function at `addr` to completion on a fresh stack."""
        uc = self.uc
        uc.reg_write(UC_ARM_REG_SP, self.stack_top)
        uc.reg_write(UC_ARM_REG_LR, RETURN_ADDR | 1)
        try:
            uc.emu_start(addr | 1, RETURN_ADDR, count=INSTRUCTION_BUDGET)
        except UcError as e:
            if not self.failure:
                self.failure = BootFailure(
                    3, f"{e} in {self.fw.describe(self._reg(UC_ARM_REG_PC))}"
                )
        if self.failure:
            raise self.failure

    def boot(self) -> None:
        """Run the software SYS_INITs, static constructors, then main() until the rate group loop."""
        fw = self.fw
        for entry in SOFTWARE_SYS_INITS:
            if entry in fw.syms:
                self.call(
                    int.from_bytes(self.uc.mem_read(fw.syms[entry], 4), "little") & ~1
                )
        for p in range(
            fw.syms["__zephyr_init_array_start"], fw.syms["__zephyr_init_array_end"], 4
        ):
            self.call(int.from_bytes(self.uc.mem_read(p, 4), "little") & ~1)
        self.call(fw.addr("main"))
        if not self.boot_done:
            raise BootFailure(
                3,
                f"main() returned or ran out of budget in {fw.describe(self._reg(UC_ARM_REG_PC))}",
            )

    # sys_heap layout (lib/heap/heap.h): chunk headers are pairs of u16 (small
    # heap) or u32 (big heap) fields; SIZE_AND_USED holds size << 1 | used.
    def _heap(self) -> tuple[int, int, bool]:
        sys_heap = self.fw.syms["z_malloc_heap"]
        z_heap = int.from_bytes(self.uc.mem_read(sys_heap, 4), "little")
        end_chunk = int.from_bytes(self.uc.mem_read(z_heap + 8, 4), "little")
        return z_heap, end_chunk, end_chunk > 0x7FFF

    def _size_and_used(self, z_heap: int, chunk: int, big: bool) -> int:
        width = 4 if big else 2
        return int.from_bytes(
            self.uc.mem_read(z_heap + chunk * CHUNK_UNIT + width, width), "little"
        )

    def _chunk_bytes_of(self, ptr: int) -> int:
        z_heap, _, big = self._heap()
        header = 8 if big else 4
        chunk = (ptr - header - z_heap) // CHUNK_UNIT
        return (self._size_and_used(z_heap, chunk, big) >> 1) * CHUNK_UNIT

    def heap_usage(self) -> tuple[int, int, int, int]:
        """Walk every chunk: arena bytes, used bytes, free bytes, largest free block."""
        z_heap, end_chunk, big = self._heap()
        used = free = largest = 0
        chunk = 0
        while chunk < end_chunk:
            field = self._size_and_used(z_heap, chunk, big)
            nbytes = (field >> 1) * CHUNK_UNIT
            if field & 1:
                used += nbytes
            else:
                free += nbytes
                largest = max(largest, nbytes)
            chunk += field >> 1
        return (end_chunk + 1) * CHUNK_UNIT, used, free, largest


def measure(elf_path: Path) -> Report:
    """Emulate setup, re-running until every allocation can be attributed."""
    fw = Firmware(elf_path)
    hooks: set[int] = set()
    while True:
        emu = Emulator(fw, hooks)
        emu.boot()
        if not emu.unresolved - hooks:
            break
        hooks |= emu.unresolved
    arena, used, free, largest = emu.heap_usage()
    sram = fw.syms["__kernel_ram_end"] - fw.syms["_image_ram_start"]
    static = fw.syms["_end"] - fw.syms["_image_ram_start"]
    return Report(sram, static, arena, used, free, largest, emu.allocations)


def print_report(r: Report) -> None:
    """Print live allocations grouped by owner, then the arena totals."""
    live = [a for a in r.allocations if not a.freed]
    by_owner: dict[str, list[Allocation]] = {}
    for a in live:
        by_owner.setdefault(a.owner, []).append(a)
    print(f"{'owner':<44} {'site':<46} {'allocs':>6} {'requested':>10} {'heap':>8}")
    for owner, allocs in sorted(
        by_owner.items(), key=lambda kv: -sum(a.heap_bytes for a in kv[1])
    ):
        sites = sorted({a.site for a in allocs})
        print(
            f"{owner:<44} {', '.join(sites):<46.46} {len(allocs):>6} "
            f"{sum(a.requested for a in allocs):>10} {sum(a.heap_bytes for a in allocs):>8}"
        )
    requested = sum(a.requested for a in live)
    print()
    print(f"SRAM                {r.sram_bytes:>8} bytes")
    print(f"static RAM (_end)   {r.static_bytes:>8} bytes")
    print(f"malloc arena        {r.arena_bytes:>8} bytes")
    print(
        f"boot allocations    {len(live):>8} live ({requested} bytes requested), "
        f"{len(r.allocations) - len(live)} more allocated and freed during setup"
    )
    print(
        f"arena used at boot  {r.used_bytes:>8} bytes (allocations incl. chunk headers and padding, "
        f"plus {r.used_bytes - sum(a.heap_bytes for a in live)} bytes of sys_heap bookkeeping)"
    )
    print(
        f"arena free at boot  {r.free_bytes:>8} bytes (largest block {r.largest_free})"
    )


def main(argv: list[str]) -> int:
    """Command-line entry point; returns the exit status."""
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[1],
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("elf", nargs="?", type=Path, default=DEFAULT_ELF)
    parser.add_argument(
        "--min-headroom",
        type=int,
        default=0,
        help="fail if the free arena after boot is below this many bytes",
    )
    args = parser.parse_args(argv[1:])

    if not args.elf.is_file():
        print(f"ERROR: {args.elf} not found. Run 'make build' first.", file=sys.stderr)
        return 1
    try:
        report = measure(args.elf)
    except BootFailure as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return e.code
    print_report(report)
    if report.free_bytes < args.min_headroom:
        print(
            f"ERROR: {report.free_bytes} bytes of malloc arena left after boot, "
            f"below the {args.min_headroom}-byte minimum.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
