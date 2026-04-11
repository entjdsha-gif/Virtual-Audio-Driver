from ghidra.program.model.scalar import Scalar


def print_block(title):
    print("\n=== {} ===".format(title))


listing = currentProgram.getListing()
fm = currentProgram.getFunctionManager()
image_base = currentProgram.getImageBase().getOffset()

targets = {
    7168: [],
    48000: [],
    96000: [],
}

for insn in listing.getInstructions(True):
    for obj in insn.getOpObjects(0):
        if isinstance(obj, Scalar):
            value = obj.getUnsignedValue()
            if value in targets:
                targets[value].append(insn.getAddress())
    for obj in insn.getOpObjects(1):
        if isinstance(obj, Scalar):
            value = obj.getUnsignedValue()
            if value in targets:
                targets[value].append(insn.getAddress())
    for obj in insn.getOpObjects(2):
        if isinstance(obj, Scalar):
            value = obj.getUnsignedValue()
            if value in targets:
                targets[value].append(insn.getAddress())

for value, addrs in targets.items():
    print_block("Scalar {}".format(value))
    seen = set()
    for addr in addrs:
        if addr in seen:
            continue
        seen.add(addr)
        func = fm.getFunctionContaining(addr)
        if func:
            print("0x{:x} RVA=0x{:x} in {}".format(
                addr.getOffset(),
                addr.getOffset() - image_base,
                func.getName(),
            ))
        else:
            print("0x{:x} RVA=0x{:x} (no function)".format(
                addr.getOffset(),
                addr.getOffset() - image_base,
            ))
        insn = listing.getInstructionAt(addr)
        if insn:
            print("  {}".format(insn))

target_rvas = [0x163d9]

for rva in target_rvas:
    addr = toAddr(image_base + rva)
    print_block("Disasm around RVA 0x{:x}".format(rva))
    start = addr.subtract(0x20)
    end = addr.add(0x40)
    insn = listing.getInstructionAt(start)
    if insn is None:
        insn = listing.getInstructionAfter(start)
    while insn and insn.getAddress().compareTo(end) <= 0:
        func = fm.getFunctionContaining(insn.getAddress())
        if func:
            prefix = "{}: ".format(func.getName())
        else:
            prefix = ""
        print("{}0x{:x} {}".format(prefix, insn.getAddress().getOffset(), insn))
        insn = insn.getNext()
