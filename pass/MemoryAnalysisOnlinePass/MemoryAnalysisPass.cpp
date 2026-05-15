#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/IRBuilder.h"





using namespace llvm;

namespace {

struct MemoryAnalysisPass : PassInfoMixin<MemoryAnalysisPass> {

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
        LLVMContext &Ctx = M.getContext();

        FunctionCallee RecordLoadFn = CreateLoadFunctionCallbackRef(M);
        FunctionCallee RecordStoreFn = CreateStoreFunctionCallbackRef(M);

        

        for (Function &F : M) {
            std::string func_name = F.getName().str();
            if (func_name == "main")
                InstallRuntimeInit(M, F);


            if (F.isDeclaration())
                continue;

            for (BasicBlock &BB : F) {

                for (auto It = BB.begin(); It != BB.end(); ) {

                    Instruction *Inst = &*It++;

                    if (auto *LI = dyn_cast<LoadInst>(Inst)) {

                        IRBuilder<> Builder(LI);

                        //
                        // Get pointer operand
                        //
                        Value *Ptr = LI->getPointerOperand();

                        //
                        // Cast to i8*
                        //
                        Value *CastPtr =
                            Builder.CreateBitCast(
                                Ptr,
                                Type::getInt8PtrTy(Ctx)
                            );

                        //
                        // Compute access size
                        //
                        Type *LoadTy = LI->getType();

                        uint64_t Size =
                            M.getDataLayout().getTypeStoreSize(LoadTy);

                        Value *SizeVal =
                            ConstantInt::get(
                                Type::getInt64Ty(Ctx),
                                Size
                            );

                        //
                        // Insert callback
                        //
                        Builder.CreateCall(
                            RecordLoadFn,
                            {CastPtr, SizeVal}
                        );
                    }
                    else if (auto *SI = dyn_cast<StoreInst>(Inst)) {

                        IRBuilder<> Builder(SI);

                        //
                        // Get pointer operand
                        //
                        Value *Ptr = SI->getPointerOperand();

                        //
                        // Cast to i8*
                        //
                        Value *CastPtr =
                            Builder.CreateBitCast(
                                Ptr,
                                Type::getInt8PtrTy(Ctx)
                            );

                        //
                        // Compute access size
                        //
                        Type *StoreType = SI->getValueOperand()->getType();

                        uint64_t Size =
                            M.getDataLayout().getTypeStoreSize(StoreType);

                        Value *SizeVal =
                            ConstantInt::get(
                                Type::getInt64Ty(Ctx),
                                Size
                            );

                        //
                        // Insert callback
                        //
                        Builder.CreateCall(
                            RecordStoreFn,
                            {CastPtr, SizeVal}
                        );
                    }

                }
            }
        }



        return PreservedAnalyses::all();
    }

    FunctionCallee CreateLoadFunctionCallbackRef(Module& M) {
        LLVMContext &Ctx = M.getContext();
        return M.getOrInsertFunction(
            "record_load",
            FunctionType::get(
                Type::getVoidTy(Ctx),
                {
                    Type::getInt8PtrTy(Ctx),
                    Type::getInt64Ty(Ctx)
                },
                false
            )
        );
    }

    FunctionCallee CreateStoreFunctionCallbackRef(Module& M) {
        LLVMContext &Ctx = M.getContext();
        return M.getOrInsertFunction(
            "record_store",
            FunctionType::get(
                Type::getVoidTy(Ctx),
                {
                    Type::getInt8PtrTy(Ctx),
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
        "MemoryAnalysisPass",
        LLVM_VERSION_STRING,

        // registration callback
        [](PassBuilder &PB) {

            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                   ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {

                    if (Name == "MemoryAnalysisPass") {
                        MPM.addPass(MemoryAnalysisPass());
                        return true;
                    }

                    return false;
                });
        }
    };
}