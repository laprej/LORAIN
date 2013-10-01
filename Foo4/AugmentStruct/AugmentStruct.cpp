#define DEBUG_TYPE "aug-struct"

#include <set>

#include "AugmentStruct.h"

#include "llvm/Value.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstVisitor.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Transforms/Utils/Cloning.h"

// Basically, we want to:
// Find the target function
// Using a InstVisitor, find all Stores
// If the Store overwrites something derived from the LP state, we need to save it
// Add the corresponding Value (from above) to a work list
// Create a new struct appending the collected Values (from above) to the message struct
// Create a new function type
// Copy all arguments, adjusting the message struct to our new type
// Clone our event handler to the new Function type
// Drop original function

using namespace llvm;

namespace {
    STATISTIC(ValsSavedToMesg, "Number of Values saved in Message struct");

    static cl::opt<std::string> FuncToInstrument("rev-func",
                                                 cl::desc("<func to reverse>"));
    	
	static cl::opt<std::string> FuncToGenerate("tgt-func",
                                               cl::desc("<func to output>"));
    
    bool usingRoss = false;
    
    class MessageUpdater: public ValueMapTypeRemapper
    {
    public:
        Type *remapType(Type *SrcTy)
        {
            std::map<Type *, Type *>::iterator i, e;
            for (i = TyMap.begin(), e = TyMap.end(); i != e; ++i) {
                if (SrcTy == i->first) {
                    return i->second;
                }
            }
            /// We aren't overriding this type so just return it
            return SrcTy;
        }
        std::map<Type *, Type *> TyMap;
    };
    
    std::set<Value*> argUsers[4];
    std::vector<Value *> workList;
    
    /// Collect all use-defs into a container
    void getUseDef(User *I, std::vector<Value *> &bucket, Function *f, int indent = 0) {
        if (indent == 0) {
            DEBUG(errs() << "uses\n");
        }
        
        int uses = 0;
        
        for (User::op_iterator i = I->op_begin(), e = I->op_end();
             i != e; ++i) {
            uses++;
            
            Value *v = *i;
            
            if (Instruction *w = dyn_cast<Instruction>(v)) {
                DEBUG(errs() << std::string(2*indent, ' '));
                DEBUG(errs() << "outputing instruction dependent: " << *w << '\n');
                bucket.push_back(w);
                getUseDef(w, bucket, f, indent + 1);
                DEBUG(errs() << std::string(2*indent, ' '));
                DEBUG(errs() << "bucket now has " << bucket.size() << " elements\n");
            }
            else if (Argument *a = dyn_cast<Argument>(v)) {
                DEBUG(errs() << "We have an argument!\n");
                DEBUG(errs() << "It's arg no. " << a->getArgNo() << "\n");
                DEBUG(errs() << "Assuming symmetric arguments.\n");
                bucket.push_back(a);
            }
            else {
                DEBUG(errs() << std::string(2*indent, ' '));
                DEBUG(errs() << "This is a " << *v->getType() << "\n");
            }
        }
        DEBUG(errs() << std::string(2*indent, ' '));
        DEBUG(errs() << "This instruction has " << uses << " uses\n\n");
    }
    
    /// Find common Values used by I and the Function arg[argIndex]
    /// This function returns a set (no dupes)
    std::vector<Value *> overlap(User *I, Function *F, int argIndex=0)
    {
        std::vector<Value *> results;
        if (usingRoss) {
            std::vector<Value *> bucket;
            std::vector<Value *>::iterator i, e;
                        
            getUseDef(I, bucket, F);
            
            std::sort(bucket.begin(), bucket.end());
            i = std::unique(bucket.begin(), bucket.end());
            bucket.resize(std::distance(bucket.begin(), i));
            
            results.resize(argUsers[argIndex].size());
            
            i = std::set_intersection(bucket.begin(),
                                      bucket.end(),
                                      argUsers[argIndex].begin(),
                                      argUsers[argIndex].end(),
                                      results.begin());
            results.resize(i - results.begin());
        }
        
        return results;
    }
    
    /// Collect arg users into argUsers
    void getArgUsers(Function *f)
    {
        int count = 0;
        Function::arg_iterator i, e;
        Value::use_iterator ui, ue, UI, UE;
        
        /// For each argument
        for (i = f->arg_begin(), e = f->arg_end(); i != e; ++i) {
            /// For each use of arg[i]
            for (ui = i->use_begin(), ue = i->use_end(); ui != ue; ++ui) {
                if (StoreInst *st = dyn_cast<StoreInst>(*ui)) {
                    Value *v = st->getPointerOperand();
                    /// For each of the users of the pointer operand
                    for (UI = v->use_begin(), UE = v->use_end(); UI != UE; ++UI) {
                        if (Instruction *inst = dyn_cast<Instruction>(*UI)) {
                            argUsers[count].insert(inst);
                            DEBUG(errs() << "Put (" << *inst << ") into bucket " << count << "\n");
                        }
                    }
                }
            }
            count++;
        }
    }
    
    class Instrumenter : public InstVisitor<Instrumenter>
    {
        Module &M;
        
    public:
        Instrumenter(Module &mod): M(mod) {}
        
        /// Create the list of types that are destructively assigned and
        /// require state-saving be done so they can be restored
        void visitStoreInst(StoreInst &I) {
            Function *F = I.getParent()->getParent();
            // If the Store overwrites something derived from the LP state, we need to save it
            std::vector<Value *> results = overlap(I, F);
            /// We have no overlap w/ arg0 (LP state), just return
            if (!results.size()) {
                return;
            }
            
            /// Do some checks in here to determine if we are doing
            /// "destructive" assignment or not.  Mainly, check if our
            /// StoreInst references the same address for a Load
            workList.push_back(I.getValueOperand());
        }
    };
    
    bool AugmentStruct::runOnModule(Module &M)
    {
        if (M.getGlobalVariable("__USING_ROSS")) {
            errs() << "ROSS Mode assumes functions have appropriate arguments\n";
            usingRoss = true;
        }
        
        if (!usingRoss)
            return false;
        
        // Find the target function
        Function *F = M.getFunction(FuncToInstrument);
        
        if (!F) {
            errs() << FuncToInstrument << " not found\n";
            return false;
        }
        
        getArgUsers(F);
        
        // Using a InstVisitor, find all Stores
        Instrumenter instrumenter(M);
        for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
            instrumenter.visit(*I);
        }

        if (!workList.size()) {
            return false;
        }
        
        std::vector<Type *> TypesToAdd;
        for (std::vector<Value*>::iterator i = workList.begin(), e = workList.end(); i != e; ++i) {
            /// Increment the statistics
            ++ValsSavedToMesg;
            
            Value *v = *i;
            if (v->hasName()) {
                DEBUG(errs() << "Adding " << v->getName() << " to message\n");
            }
            else {
                DEBUG(errs() << "Adding unnamed type " << *v->getType() << " to message\n");
            }
            TypesToAdd.push_back(v->getType());
        }
        
        std::vector<Argument *> funArgsFrom;
        for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E; ++I) {
            funArgsFrom.push_back(I);
        }
        
        Type *Ty = 0;
        
        FunctionType *funTypeFrom = F->getFunctionType();
        
        std::vector<Type *> funTypeArgsFrom(funTypeFrom->param_begin(),
                                            funTypeFrom->param_end());
        Ty = funTypeArgsFrom[2];
        
        assert(Ty);
        assert(isa<PointerType>(Ty));
        
        Ty = Ty->getPointerElementType();
        
        assert(isa<StructType>(Ty));
        
        StructType *fromStruct = cast<StructType>(Ty);
        PointerType *fromStructPtr = PointerType::getUnqual(fromStruct);
        PointerType *fromStructPtrPtr = PointerType::getUnqual(fromStructPtr);
        
        std::vector<Type *> structItems(fromStruct->element_begin(),
                                        fromStruct->element_end());
        /// Add the other items to the struct we're about to create
        structItems.insert(structItems.end(), TypesToAdd.begin(), TypesToAdd.end());
        
        StructType *toStruct = StructType::create(structItems, fromStruct->getName(), fromStruct->isPacked());
        PointerType *toStructPtr = PointerType::getUnqual(toStruct);
        PointerType *toStructPtrPtr = PointerType::getUnqual(toStructPtr);
        
        Argument *newArg = new Argument(toStructPtr, funArgsFrom[2]->getName(), 0);
        
        DEBUG(errs() << "Created new Argument " << newArg->getName() << "\n");
        
        std::vector<Type *> funTypeArgsTo(funTypeArgsFrom.begin(), funTypeArgsFrom.end());
        funTypeArgsTo[2] = toStructPtr;
        
        std::vector<Argument *> funArgsTo(funArgsFrom.begin(), funArgsFrom.end());
        funArgsTo[2] = newArg;
        
        FunctionType *funTypeTo = FunctionType::get(F->getReturnType(), funTypeArgsTo, F->getFunctionType()->isVarArg());
        
        Function *newFun = Function::Create(funTypeTo, F->getLinkage(), F->getName(), &M);
        
        ValueToValueMapTy VMap;
        Function::arg_iterator DestI = newFun->arg_begin();
        
        for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end();
             I != E; ++I) {
            DestI->setName(I->getName());
            VMap[I] = DestI++;
        }
        
        SmallVector<ReturnInst*, 4> Returns;
        
        MessageUpdater TyMapper;
        
        TyMapper.TyMap[fromStruct] = toStruct;
        TyMapper.TyMap[fromStructPtr] = toStructPtr;
        TyMapper.TyMap[fromStructPtrPtr] = toStructPtrPtr;

        CloneFunctionInto(newFun, F, VMap, true, Returns, "", 0, &TyMapper);
        
        while (!F->use_empty()) {
            User *U = F->use_back();
            DEBUG(errs() << "Use of F: " << *U << "\n");
            DEBUG(errs() << "Type: " << *U->getType() << "\n");
            if (Constant *C = dyn_cast<Constant>(U)) {
                /// replaceAllUsesWith will fail because the function types are
                /// different.  Do it this way instead
                C->replaceUsesOfWithOnConstant(F, newFun, 0);
            }
        }
        
        newFun->takeName(F);

        F->dropAllReferences();
        F->eraseFromParent();
        
        return true;
    }
    
    char AugmentStruct::ID = 0;
    static RegisterPass<AugmentStruct> A ("aug-struct",
                                          "Augment Message Struct");
}
