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

using namespace llvm;

namespace {

struct InstructionAnalysisPass : PassInfoMixin<InstructionAnalysisPass> {
    uint64_t BBIDCounter = 0;
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
        LLVMContext &Ctx = M.getContext();

        FunctionCallee BBFn = CreateBBFunctionCallbackRef(M);

        for (Function &F : M) {
            std::string func_name = F.getName().str();
            //if (func_name == "main")
            //    InstallRuntimeInit(M, F);


            if (F.isDeclaration())
                continue;

            for (BasicBlock &BB : F) {
                uint64_t BBID = BBIDCounter++;
                IRBuilder<> Builder(&*BB.getFirstInsertionPt());

                Value *BBIDVal =
                    Builder.getInt64(BBID);

               Builder.CreateCall(BBFn, { BBIDVal });
            }
        }



        return PreservedAnalyses::all();
    }

    FunctionCallee CreateBBFunctionCallbackRef(Module& M) {
        LLVMContext &Ctx = M.getContext();
        return M.getOrInsertFunction(
            "bb_entry_callback",
            FunctionType::get(
                Type::getVoidTy(Ctx),
                {
                    Type::getInt64Ty(Ctx)
                },
                false
            )
        );
    }

    
    FunctionCallee CreateRuntimeInitFunctionCallbackRef(Module& M) {
        LLVMContext &Ctx = M.getContext();
        return M.getOrInsertFunction(
            "runtime_init",
            FunctionType::get(
                Type::getVoidTy(Ctx),
                {
                },
                false
            )
        );
    }


    void InstallRuntimeInit(Module& M, llvm::Function& mainFunc)
    {
        FunctionCallee RuntimeInitFn = CreateRuntimeInitFunctionCallbackRef(M);
        BasicBlock &EntryBB = mainFunc.getEntryBlock();
        IRBuilder<> Builder(&*EntryBB.begin());

        Builder.CreateCall(RuntimeInitFn);
    }

};

} // namespace


// REQUIRED LLVM 14 plugin entry point
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo(void) {

    return {
        LLVM_PLUGIN_API_VERSION,
        "InstructionAnalysisPass",
        LLVM_VERSION_STRING,

        // registration callback
        [](PassBuilder &PB) {

            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                   ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {

                    if (Name == "InstructionAnalysisPass") {
                        MPM.addPass(InstructionAnalysisPass());
                        return true;
                    }

                    return false;
                });
        }
    };
}