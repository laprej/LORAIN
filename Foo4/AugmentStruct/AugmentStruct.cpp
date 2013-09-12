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
            for (i = foo.begin(), e = foo.end(); i != e; ++i) {
                if (SrcTy == i->first) {
                    return i->second;
                }
            }
            
            assert(0 && "Type not found in map!");
            return 0;
        }
        std::map<Type *, Type *> foo;
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
                errs() << "We have an argument!\n";
                errs() << "It's arg no. " << a->getArgNo() << "\n";
                errs() << "Assuming symmetric arguments.\n";
                
                f->getArgumentList();
                Function::arg_iterator fi, fe;
                unsigned i;
                for (i = 0, fi = f->arg_begin(), fe = f->arg_end(); fi != fe; ++fi, ++i) {
                    if (i == a->getArgNo()) {
                        break;
                    }
                }
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
    std::vector<Value *> overlap(Instruction &I, int argIndex=0)
    {
        std::vector<Value *> results;
        if (usingRoss) {
            std::vector<Value *> bucket;
            std::vector<Value *>::iterator i, e;
            
            Function *f = I.getParent()->getParent();
            
            getUseDef(&I, bucket, f);
            
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
                            errs() << "Put (" << *inst << ") into bucket " << count << "\n";
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
            std::vector<Value *> results = overlap(I);
            /// We have no overlap w/ arg0 (LP state), just return
            if (!results.size()) {
                return;
            }
            
            /// Do some checks in here to determine if we are doing
            /// "destructive" assignment or not.  Mainly, check if our
            /// StoreInst references the same address for a Load
            
            Function *f = M.getFunction(FuncToInstrument);
            std::vector<Argument *> funArgsFrom;
            for (Function::arg_iterator iter = f->arg_begin(),
                 end = f->arg_end(); iter != end; ++iter) {
                funArgsFrom.push_back(iter);
            }
            
            Type *Ty = 0;
            
            FunctionType *funTypeFrom = f->getFunctionType();
            
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
            
            StructType *toStruct = StructType::create(structItems, fromStruct->getName(), fromStruct->isPacked());
            PointerType *toStructPtr = PointerType::getUnqual(toStruct);
            PointerType *toStructPtrPtr = PointerType::getUnqual(toStructPtr);
            
            Argument *newArg = new Argument(toStructPtr, funArgsFrom[2]->getName(), 0);
            
            errs() << "Created new Argument " << newArg->getName() << "\n";
            
            std::vector<Type *> funTypeArgsTo(funTypeArgsFrom.begin(), funTypeArgsFrom.end());
            funTypeArgsTo[2] = toStructPtr;
            
            std::vector<Argument *> funArgsTo(funArgsFrom.begin(), funArgsFrom.end());
            funArgsTo[2] = newArg;
            
            FunctionType *funTypeTo = FunctionType::get(f->getReturnType(), funTypeArgsTo, false);
            
            Function *newFun = Function::Create(funTypeTo, Function::ExternalLinkage, f->getName());
            
            ValueToValueMapTy vmap;
            for (unsigned i = 0; i < funArgsFrom.size(); ++i) {
                vmap.insert(std::make_pair(funArgsFrom[i], funArgsTo[i]));
            }
            
            SmallVector<ReturnInst*, 4> Returns;
            
            MessageUpdater foobar;
            
            foobar.foo.insert(std::make_pair(fromStruct, toStruct));
            foobar.foo.insert(std::make_pair(fromStructPtr, toStructPtr));
            foobar.foo.insert(std::make_pair(fromStructPtrPtr, toStructPtrPtr));
            
            for (inst_iterator I = inst_begin(f), E = inst_end(f); I != E; ++I) {
                foobar.foo.insert(std::make_pair(I->getType(), I->getType()));
            }
            
            std::map<Type *, Type *>::iterator i, e;
            for (i = foobar.foo.begin(), e = foobar.foo.end(); i != e; ++i) {
                errs() << *i->first << " maps to " << *i->second << "\n";
            }
            
            //foobar.foo.insert(std::make_pair(funArgsFrom[2], funArgsTo[2]));
            
            CloneFunctionInto(newFun, f, vmap, false, Returns, "", 0, &foobar);
            
            //                f->removeFromParent();
            //
            //                return;
            
            f->replaceAllUsesWith(ConstantExpr::getBitCast(newFun, f->getType()));
            newFun->removeDeadConstantUsers();
            f->eraseFromParent();
            
            for (Value::use_iterator i = f->use_begin(), e = f->use_end(); i != e; ++i) {
                if (Instruction *Inst = dyn_cast<Instruction>(*i)) {
                    errs() << "F is used in instruction:\n";
                    errs() << *Inst << "\n";
                }
                else {
                    // All uses fall into this category
                    errs() << "F (non-instruction): " << **i << "\n";
                    errs() << "F.type is " << *i->getType() << "\n";
                    
                    if (isa<MDNode>(*i)) {
                        errs() << "MDNode\n";
                    }
                    if (isa<BlockAddress>(*i)) {
                        errs() << "Blockaddress\n";
                    }
                    if (BlockAddress *b = dyn_cast<BlockAddress>(*i)) {
                        Function *baf = b->getFunction();
                        BasicBlock *bb = b->getBasicBlock();
                        //b->replaceUsesOfWithOnConstant(<#llvm::Value *From#>, <#llvm::Value *To#>, <#llvm::Use *U#>)
                    }
                }
            }
            
            return;
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
        
        if (Function *F = M.getFunction(FuncToInstrument)) {
            getArgUsers(F);
            
            Instrumenter instrumenter(M);
        }
        
        for (std::vector<Value*>::iterator i = workList.begin(), e = workList.end(); i != e; ++i) {
            /// Increment the statistics
            ++ValsSavedToMesg;
            
            Value *v = *i;
            if (v->hasName()) {
                errs() << "Adding " << v->getName() << " to message\n";
            }
            else {
                errs() << "Adding unnamed type " << *v->getType() << " to message";
            }
        }
        
        return false;
    }
}

char AugmentStruct::ID = 0;
static RegisterPass<AugmentStruct> A ("aug-struct",
                                      "Augment Message Struct");
