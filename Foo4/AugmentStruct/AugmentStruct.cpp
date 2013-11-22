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

static cl::opt<std::string> FuncToInstrument("ins-func",
                                             cl::desc("<func to instrument>"));
static cl::opt<std::string> FuncToUpdate("update-func",
                                         cl::desc("<reverse handler to update>"));

bool usingRoss = false;

const std::string jml = "jml";
const std::string jmlAugSwap = "jml.augSwap";
const std::string jmlAugId = "jml.functionPrologue";

// Layout of jmlAugId metadata:
//  -------------------------------------------------------------------
// |  index of LP state member  |  index of LP message storage member  |
//  -------------------------------------------------------------------

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
std::map<Value *, Value *> workList;

MDNode * markJML(Value *v, bool skip = true) {
    MDNode *ret = 0;

    if (Instruction *I = dyn_cast<Instruction>(v)) {
        Value *Elts[] = {
            ConstantInt::get(Type::getInt1Ty(getGlobalContext()), skip ? 1 : 0), // skip
            ret
        };
        ret = MDNode::get(getGlobalContext(), Elts);
        I->setMetadata(jml, ret);
        I->setMetadata(jmlAugSwap, ret);
    }

    return ret;
}

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
            /// This should only happen with optimisation turned on
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

/// Find common Values used by I and the Function_arg[argIndex]
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
        
        /// If the value we load has a name and is not a global...
        /// (i.e. it's a local variable in this function)
        for (unsigned i = 0; i < bucket.size(); ++i) {
            if (LoadInst *L = dyn_cast<LoadInst>(bucket[i])) {
                if (!L->getPointerOperand()->hasName()) {
                    continue;
                }
                DEBUG(errs() << "L is named " << L->getPointerOperand()->getName() << "\n");
                if (isa<GlobalVariable>(L->getPointerOperand())) {
                    DEBUG(errs() << "L is global");
                }
                else {
                    /// Return the empty results vector
                    /// TODO: Is this correct?  Should we return empty on the
                    /// TODO: first local variable we see?  We can't recreate
                    /// TODO: the resulting value w/o the local so I guess so
                    return results;
                }
            }
        }
        
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
        
        /// Are we storing into the LP state?
        if (User *ptrOp = dyn_cast<User>(I.getPointerOperand())) {
            std::vector<Value *> stateStores = overlap(ptrOp, F);
            /// If not, just return
            if (stateStores.size() == 0) {
                return;
            }
            
            /// OK, we're storing into the LP state.  Figure out whether
            /// it's constructive or destructive
            if (User *valOp = dyn_cast<User>(I.getValueOperand())) {
                std::vector<Value *> stateVals = overlap(valOp, F);
                if (stateVals.size() == 0) {
                    /// Where did we get this value?  Not constructive!
                    if (GetElementPtrInst *G = dyn_cast<GetElementPtrInst>(I.getPointerOperand())) {
                        /// We're restricting things here:
                        /// Assume that we're accessing our state struct
                        /// Which implies op0 is the struct, op1 is 0,
                        /// and op2 is the index we're overwriting.
                        Value *zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
                        assert(G->getOperand(1) == zero);
                        workList[G->getOperand(2)] = I.getPointerOperand();
                        markJML(&I);
                    }
                    return;
                }
                /// It is constructive assignment, don't save it
                return;
            }
        }
        
        /// Do some checks in here to determine if we are doing
        /// "destructive" assignment or not.  Mainly, check if our
        /// StoreInst references the same address for a Load
        // workList.push_back(I.getValueOperand());
    }
};

bool AugmentStruct::runOnModule(Module &M)
{
    if (M.getGlobalVariable("__USING_ROSS")) {
        DEBUG(errs() << "ROSS Mode assumes functions have appropriate arguments\n");
        usingRoss = true;
    }
    
    if (!usingRoss)
        return false;
    
    // Find the target function
    Function *F = M.getFunction(FuncToInstrument);
    // Reverse handler to update
    Function *G = M.getFunction(FuncToUpdate);
    
    if (!F) {
        errs() << "FuncToInstrument (" << FuncToInstrument << ") not found\n";
        return false;
    }

    if (!G) {
        errs() << "FuncToUpdate (" << FuncToUpdate << ") not found\n";
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

    std::vector<Value *> valuesToSave;

    std::vector<Type *> TypesToAdd;

    int j = 0;
    for (std::map<Value*, Value*>::iterator i = workList.begin(), e = workList.end(); i != e; ++i) {
        /// Increment the statistics
        ++ValsSavedToMesg;
        
        Value *v = i->second;
        if (v->hasName()) {
            DEBUG(errs() << "Adding " << v->getName() << " to message\n");
        }
        else {
            DEBUG(errs() << "Adding unnamed type " << *v->getType() << " to message\n");
        }
        DEBUG(errs() << "Which has type " << *v->getType() << "\n");
        assert(isa<PointerType>(v->getType()));
        Type *Ty = v->getType()->getPointerElementType();
        assert(Ty);
        TypesToAdd.push_back(Ty);
        
        /// Save the index into the LP state that we're overwriting
        /// But we also need to save the index we are placing it in

        std::vector<Constant *> tempVector;

        /// Create array type
        ArrayType *AT = ArrayType::get(Type::getInt32Ty(getGlobalContext()), 2);

        tempVector.push_back(cast<Constant>(i->first));
        Constant *idx = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                                      structItems.size() + j++);
        tempVector.push_back(idx);
        ArrayRef<Constant *> idxPair(tempVector);
        Constant *constPair = ConstantArray::get(AT, idxPair);
        valuesToSave.push_back(constPair);
    }
    
    MDNode *vals = MDNode::get(getGlobalContext(), valuesToSave);
    NamedMDNode *nmd = M.getOrInsertNamedMetadata(jmlAugId);
    nmd->addOperand(vals);
    
    std::vector<Argument *> funArgsFrom;
    for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E; ++I) {
        funArgsFrom.push_back(I);
    }
    
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
    
    Function *newForward = Function::Create(funTypeTo, F->getLinkage(), F->getName(), &M);
    Function *newReverse = Function::Create(funTypeTo, G->getLinkage(), G->getName(), &M);
    
    ValueToValueMapTy VMap;
    Function::arg_iterator DestI = newForward->arg_begin();
    
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

    CloneFunctionInto(newForward, F, VMap, true, Returns, "", 0, &TyMapper);

    while (!F->use_empty()) {
        User *U = F->use_back();
        DEBUG(errs() << "Use of F: " << *U << "\n");
        DEBUG(errs() << "Type: " << *U->getType() << "\n");
        if (Constant *C = dyn_cast<Constant>(U)) {
            /// replaceAllUsesWith will fail because the function types are
            /// different.  Do it this way instead
            C->replaceUsesOfWithOnConstant(F, newForward, 0);
        }
    }
    newForward->takeName(F);
    F->dropAllReferences();
    F->eraseFromParent();

    while (!G->use_empty()) {
        User *U = G->use_back();
        DEBUG(errs() << "Use of F: " << *U << "\n");
        DEBUG(errs() << "Type: " << *U->getType() << "\n");
        if (Constant *C = dyn_cast<Constant>(U)) {
            /// replaceAllUsesWith will fail because the function types are
            /// different.  Do it this way instead
            C->replaceUsesOfWithOnConstant(G, newReverse, 0);
        }
    }
    newReverse->takeName(G);
    G->dropAllReferences();
    G->eraseFromParent();

    Function *init = M.getFunction("init");

    Returns.clear();
    VMap.clear();

    Function *newInit = Function::Create(init->getFunctionType(), init->getLinkage(), init->getName(), &M);

    DestI = newInit->arg_begin();
    for (Function::arg_iterator I = init->arg_begin(), E = init->arg_end();
         I != E; ++I) {
        DestI->setName(I->getName());
        VMap[I] = DestI++;
    }

    CloneFunctionInto(newInit, init, VMap, true, Returns, "", 0, &TyMapper);

    init->replaceAllUsesWith(newInit);
    newInit->takeName(init);
    init->dropAllReferences();
    init->eraseFromParent();

    Function *main = M.getFunction("main");

    Returns.clear();
    VMap.clear();

    Function *newMain = Function::Create(main->getFunctionType(), main->getLinkage(), main->getName(), &M);

    DestI = newMain->arg_begin();
    for (Function::arg_iterator I = main->arg_begin(), E = main->arg_end();
         I != E; ++I) {
        DestI->setName(I->getName());
        VMap[I] = DestI++;
    }

    ArrayType *at1 = ArrayType::get(fromStruct, 2);
    ArrayType *at2 = ArrayType::get(toStruct, 2);
    PointerType *at1ptr = PointerType::getUnqual(at1);
    PointerType *at2ptr = PointerType::getUnqual(at2);

    TyMapper.TyMap[at1] = at2;
    TyMapper.TyMap[at1ptr] = at2ptr;

    CloneFunctionInto(newMain, main, VMap, true, Returns, "", 0, &TyMapper);

    main->replaceAllUsesWith(newMain);
    newMain->takeName(main);
    main->dropAllReferences();
    main->eraseFromParent();

    return true;
}

char AugmentStruct::ID = 0;
static RegisterPass<AugmentStruct> A ("aug-struct",
                                      "Augment Message Struct");
}
