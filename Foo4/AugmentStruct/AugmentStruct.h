#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace {
    class AugmentStruct : public ModulePass
    {
    public:
        static char ID;
        AugmentStruct() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M);
        const char *getPassName() const {
            return "Augment Message Struct";
        }
    };
}
