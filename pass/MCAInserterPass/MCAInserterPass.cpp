#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

// Correct locations for LLVM 23
#include "llvm/IR/PassManager.h"      // ← This is the correct one
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/IRBuilder.h"

// Support
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Demangle/Demangle.h"   // ← Add this





using namespace llvm;
#include <iostream>
using namespace llvm;

namespace {


std::string getCleanName(const llvm::Function &F) {
    StringRef Mangled = F.getName();

    // Demangle the name
    std::string Demangled = llvm::demangle(Mangled.str());

    // If demangling failed, just return original
    if (Demangled == Mangled.str())
        return Mangled.str();

    // Optional: strip parameters (e.g. "init_matrix(std::vector...)" → "init_matrix")
    size_t pos = Demangled.find('(');
    if (pos != std::string::npos) {
        Demangled = Demangled.substr(0, pos);
    }

    return Demangled;
}


struct MCAInserterPass : public PassInfoMixin<MCAInserterPass> {


    bool shouldAddMCA(Function &F) {
        if (F.isDeclaration()) return false;

        Module *M = F.getParent();
        GlobalVariable *GV = M->getGlobalVariable("llvm.global.annotations");
        if (!GV || !GV->hasInitializer()) return false;

        auto *CA = dyn_cast<ConstantArray>(GV->getInitializer());
        if (!CA) return false;

        for (const Use &U : CA->operands()) {
            auto *CS = dyn_cast<ConstantStruct>(U.get());
            if (!CS || CS->getNumOperands() < 2) continue;

            // Check if annotation belongs to this function
            if (auto *CE = dyn_cast<ConstantExpr>(CS->getOperand(0))) {
                if (CE->getOperand(0) == &F) {
                    if (auto *StrCE = dyn_cast<ConstantExpr>(CS->getOperand(1))) {
                        if (auto *Str = dyn_cast<ConstantDataArray>(StrCE->getOperand(0))) {
                            StringRef Anno = Str->getAsCString();
                            if (Anno.contains("llvm-mca")) {
                                errs() << "[MCA PASS] FOUND tag on " << F.getName() << "\n";
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }

    PreservedAnalyses run(Module &M,
                          ModuleAnalysisManager &) {

        for (Function &F : M) {


            StringRef Name = F.getName();   // Use StringRef - more efficient
                if (Name == "bb_entry_callback" ||
                    Name == "function_entry_callback" ||
                    Name == "function_exit_callback")
                    continue;

                // ==================== STRONG LIBRARY FILTER ====================
                if (F.isDeclaration() || F.isIntrinsic())
                    continue;

                // Skip anything from std:: namespace
                if (Name.starts_with("_ZSt") ||           // std:: (most common)
                    Name.starts_with("_ZNSt") ||          // std:: in namespaces
                    Name.starts_with("_ZNKS") ||
                    Name.starts_with("std::") ||          // demangled
                    Name.starts_with("__") ||
                    Name.contains("__cxx11") ||           // std::__cxx11::basic_string
                    Name.contains("__detail") ||          // std::__detail:: (hashtable internals)
                    Name.contains("_Hashtable") ||        // _Hashtable stuff
                    Name.contains("__gnu_cxx"))
                    continue;

                // Additional common C++ std internals
                if (Name.contains("basic_string") ||
                    Name.contains("vector<") ||
                    Name.contains("allocator<") ||
                    Name.contains("operator new") ||
                    Name.contains("operator delete"))
                    continue;



            errs() << "[MCA PASS] running on function: " << F.getName() << "\n";
            BasicBlock &Entry = F.getEntryBlock();

            IRBuilder<> BeginBuilder(
                &*Entry.getFirstInsertionPt());

            auto *FT =
                FunctionType::get(
                    Type::getVoidTy(M.getContext()),
                    false);

            auto *BeginAsm =
                InlineAsm::get(
                    FT,
                    "# LLVM-MCA-BEGIN " +
                    getCleanName(F),
                    "",
                    true);

            BeginBuilder.CreateCall(BeginAsm);

            for (BasicBlock &BB : F) {
                if (auto *Ret =
                    dyn_cast<ReturnInst>(
                        BB.getTerminator())) {

                    IRBuilder<> EndBuilder(Ret);

                    auto *EndAsm =
                        InlineAsm::get(
                            FT,
                            "# LLVM-MCA-END " +
                            getCleanName(F),
                            "",
                            true);

                    EndBuilder.CreateCall(
                        EndAsm);
                }
            }
        }

        return PreservedAnalyses::none();
    }
};

}

extern "C"
LLVM_ATTRIBUTE_WEAK
PassPluginLibraryInfo llvmGetPassPluginInfo() {

    return {
        LLVM_PLUGIN_API_VERSION,
        "MCAInserterPass",
        "0.1",
        [](PassBuilder &PB) {

            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                   ModulePassManager &MPM,
                   auto) {

                    if (Name ==
                        "MCAInserterPass") {

                        MPM.addPass(
                            MCAInserterPass());

                        return true;
                    }

                    return false;
                });
        }};
}