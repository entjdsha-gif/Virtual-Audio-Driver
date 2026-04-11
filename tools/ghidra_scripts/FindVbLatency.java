// Find scalar uses of latency/sample-rate constants in the VB-Cable driver.

import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.scalar.Scalar;

public class FindVbLatency extends GhidraScript {

    @Override
    public void run() throws Exception {
        Listing listing = currentProgram.getListing();
        Memory memory = currentProgram.getMemory();
        long imageBase = currentProgram.getImageBase().getOffset();

        long[] values = new long[] { 7168, 48000, 96000 };
        Map<Long, Set<Address>> hits = new LinkedHashMap<>();
        for (long v : values) {
            hits.put(v, new LinkedHashSet<>());
        }

        Instruction insn = listing.getInstructions(true).next();
        while (insn != null) {
            for (int opIndex = 0; opIndex < insn.getNumOperands(); opIndex++) {
                Object[] objs = insn.getOpObjects(opIndex);
                for (Object obj : objs) {
                    if (obj instanceof Scalar) {
                        long value = ((Scalar) obj).getUnsignedValue();
                        if (hits.containsKey(value)) {
                            hits.get(value).add(insn.getAddress());
                        }
                    }
                }
            }
            insn = insn.getNext();
        }

        for (long value : values) {
            println("");
            println("=== Scalar " + value + " ===");
            for (Address addr : hits.get(value)) {
                Function fn = getFunctionContaining(addr);
                if (fn != null) {
                    println(String.format("0x%x RVA=0x%x in %s", addr.getOffset(),
                        addr.getOffset() - imageBase, fn.getName()));
                }
                else {
                    println(String.format("0x%x RVA=0x%x (no function)", addr.getOffset(),
                        addr.getOffset() - imageBase));
                }
                Instruction at = listing.getInstructionAt(addr);
                if (at != null) {
                    println("  " + at.toString());
                }
            }
        }

        long[] targetRvas = new long[] { 0x163d9L };
        for (long rva : targetRvas) {
            println("");
            println(String.format("=== Disasm around RVA 0x%x ===", rva));
            Address start = toAddr(imageBase + rva - 0x20);
            Address end = toAddr(imageBase + rva + 0x40);
            Instruction cur = listing.getInstructionContaining(start);
            if (cur == null) {
                cur = listing.getInstructionAfter(start);
            }
            while (cur != null && cur.getAddress().compareTo(end) <= 0) {
                println(String.format("0x%x %s", cur.getAddress().getOffset(), cur.toString()));
                cur = cur.getNext();
            }
        }

        Function f = getFunctionContaining(toAddr(imageBase + 0x163d8L));
        if (f != null) {
            println("");
            println("=== Decompile around 7168 function ===");
            DecompInterface ifc = new DecompInterface();
            ifc.openProgram(currentProgram);
            DecompileResults res = ifc.decompileFunction(f, 60, monitor);
            if (res != null && res.decompileCompleted()) {
                String c = res.getDecompiledFunction().getC();
                String[] lines = c.split("\\r?\\n");
                for (int i = 0; i < Math.min(lines.length, 220); i++) {
                    println(lines[i]);
                }
            }
            else {
                println("Decompiler did not complete for " + f.getName());
            }
            ifc.dispose();
        }

        long[] dataAddrs = new long[] {
            0x14001b290L, 0x14001b2b0L, 0x14001b310L, 0x14001b390L,
            0x14001b3d0L, 0x14001b400L, 0x14001b430L
        };
        for (long off : dataAddrs) {
            println("");
            println(String.format("=== Data at 0x%x ===", off));
            Address a = toAddr(off);
            byte[] buf = new byte[128];
            int n = memory.getBytes(a, buf);
            StringBuilder hex = new StringBuilder();
            for (int i = 0; i < Math.min(n, 48); i++) {
                hex.append(String.format("%02x ", buf[i] & 0xff));
            }
            println(hex.toString().trim());
            StringBuilder ascii = new StringBuilder();
            for (int i = 0; i < n; i++) {
                int b = buf[i] & 0xff;
                if (b == 0) break;
                if (b >= 32 && b <= 126) ascii.append((char)b);
                else ascii.append('.');
            }
            println("ASCII: " + ascii.toString());
            StringBuilder utf16 = new StringBuilder();
            for (int i = 0; i + 1 < n; i += 2) {
                int ch = (buf[i] & 0xff) | ((buf[i + 1] & 0xff) << 8);
                if (ch == 0) break;
                if (ch >= 32 && ch <= 126) utf16.append((char) ch);
                else utf16.append('.');
            }
            println("UTF16: " + utf16.toString());

            Reference[] refs = getReferencesTo(a);
            for (Reference ref : refs) {
                Function rf = getFunctionContaining(ref.getFromAddress());
                if (rf != null) {
                    println(String.format("XREF 0x%x RVA=0x%x in %s",
                        ref.getFromAddress().getOffset(),
                        ref.getFromAddress().getOffset() - imageBase,
                        rf.getName()));
                }
                else {
                    println(String.format("XREF 0x%x RVA=0x%x",
                        ref.getFromAddress().getOffset(),
                        ref.getFromAddress().getOffset() - imageBase));
                }
            }
        }
    }
}
