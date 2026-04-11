// Ghidra headless script: decompile all functions to C pseudocode
// @category Analysis

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

public class ExportDecompile extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String outputDir = "D:/mywork/Virtual-Audio-Driver/results/ghidra_decompile";
        new File(outputDir).mkdirs();

        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        FunctionManager fm = currentProgram.getFunctionManager();
        FunctionIterator funcs = fm.getFunctions(true);

        PrintWriter codeWriter = new PrintWriter(new FileWriter(outputDir + "/vbcable_all_functions.c"));
        PrintWriter indexWriter = new PrintWriter(new FileWriter(outputDir + "/vbcable_function_index.txt"));

        int count = 0;
        while (funcs.hasNext() && !monitor.isCancelled()) {
            Function func = funcs.next();
            String name = func.getName();
            String entry = func.getEntryPoint().toString();
            long size = func.getBody().getNumAddresses();

            DecompileResults results = decomp.decompileFunction(func, 120, monitor);
            if (results.decompileCompleted()) {
                String code = results.getDecompiledFunction().getC();
                codeWriter.println("// === " + name + " @ " + entry + " (size=" + size + ") ===");
                codeWriter.println(code);
                codeWriter.println();
                indexWriter.println(String.format("%-40s @ %s size=%d", name, entry, size));
            } else {
                indexWriter.println(String.format("%-40s @ %s FAILED", name, entry));
            }
            count++;
        }

        codeWriter.close();
        indexWriter.close();
        decomp.dispose();

        println("Decompiled " + count + " functions to " + outputDir);
    }
}
