//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "hello"
#include "Foo4.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Type.h"

#include <map>

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/ADT/ilist.h"


// COPIED FROM PREVIOUS ATTEMPT
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/LLVMContext.h"
#include "llvm/Value.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/InstVisitor.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/ValueSymbolTable.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/ADT/PostOrderIterator.h"

#include "llvm/Assembly/Writer.h"

#include <stack>
#include <set>
#include <list>

#include "llvm/Analysis/MemoryDependenceAnalysis.h"

#include "llvm/DebugInfo.h"

using namespace llvm;



namespace {
    static cl::opt<std::string> FuncToInstrument("rev-func",
										 cl::desc("<func to reverse>"));//, llvm::cl::Required);	
	
	static cl::opt<std::string> FuncToGenerate("tgt-func",
										   cl::desc("<func to output>"));//, llvm::cl::Required);
	
    /// Map basic blocks from (old) event handler to (new) synthesized
    /// reverse event handler
	std::map<BasicBlock *, BasicBlock *> bbmOldToNew;
    std::vector<Instruction *> allocas;
    std::vector<Instruction *> args;
    
    std::set<Value*> argUsers[4];
    
    bool usingRoss = false;
    
    /// atadatem = reverse metadata
    class Atadatem : DIDescriptor
    {
    public:
        explicit Atadatem(const MDNode *N) : DIDescriptor(N) { }
        
        bool isSkip() const { return getUnsignedField(0) != 0; }
        //void setSkip(bool b) { skip = b; }
    };
    
    /// Given a type and function, search through the arguments to find it
    Value *searchArguments(Type *t, Module &M, Function *f)
    {
        Function::arg_iterator fi, fe;
        
        for (fi = f->arg_begin(), fe = f->arg_end(); fi != fe; ++fi) {
            Argument *a = fi;
            if (a->getType() == t) {
                return a;
            }
        }
        return 0;
    }
    
    /// Find arg and corresponding alloca instruction for it
    Value *searchArgumentsAllocas(Type *t, Module &M, Function *f)
    {
        Value *a = searchArguments(t, M, f);
        
        Value::use_iterator ui, ue;
        
        for (ui = a->use_begin(), ue = a->use_end(); ui != ue; ++ui) {
            if (StoreInst *store = dyn_cast<StoreInst>(*ui)) {
                Value *v = store->getPointerOperand();
                return v;
            }
        }
        return 0;
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
	
#pragma mark
#pragma mark Instrumenter

#undef DEBUG_TYPE
#define DEBUG_TYPE "Instrumenter"
    
	/// This class will apply the necessary instrumentation on the forward path
	class Instrumenter : public InstVisitor<Instrumenter>
	{
		Module &M;
        Hello *h;
        
        unsigned bitFieldCount;// = 0;
        unsigned loopCount;
        
	public:
		Instrumenter(Module &mod, Hello *H): M(mod), h(H) {
            bitFieldCount = 0;
            loopCount = 0;
		}
		
        std::string newBitFieldName(StringRef s)
        {
            //static unsigned bitFieldCount = 0;
            //
            Twine bff(s);
            
            bff = bff.concat(Twine(bitFieldCount++));
            
            DEBUG(errs() << "Instrumenter: newBitFieldName is returning " << bff << "\n");
            
            return bff.str();
        }
        
		Constant * insertBitField(StringRef Name) {
			Type *Ty = IntegerType::get(getGlobalContext(), 32);
			
			Constant *C = M.getOrInsertGlobal(Name, Ty);
			assert(isa<GlobalVariable>(C) && "Incorrectly typed anchor?");
			GlobalVariable *GV = cast<GlobalVariable>(C);
			
			// If it has an initializer, it is already in the module.
			if (GV->hasInitializer()) {
				assert(0 && "Re-creating existing variable!");
			}
			
			GV->setLinkage(GlobalValue::LinkOnceAnyLinkage);
			GV->setConstant(false);
			
			IRBuilder<> temp(getGlobalContext());
			GV->setInitializer(temp.getInt32(0));
			
			return C;
		}
		
		MDNode * markJML(Value *v, bool skip = true) {
            MDNode *ret = 0;
            
			if (CmpInst *C = dyn_cast<CmpInst>(v)) {
				/// Assert that the next instruction is a branch
				BasicBlock::iterator it(C);
				++it;
				if (BranchInst *B = dyn_cast<BranchInst>(it)) {
					assert(B->getNumSuccessors() == 2 && "Branch doesn't have two sucessors!");
					BasicBlock *then = B->getSuccessor(0);
                    DEBUG(errs() << "Instrumenter: B->getSuccessor(0) is " << then->getName() << "\n");
					BasicBlock *el = B->getSuccessor(1);
                    DEBUG(errs() << "Instrumenter: B->getSuccessor(1) is " << el->getName() << "\n");
                    //B->getParent()->getParent()->viewCFG();
                    
					Value *Elts[] = {
//                        BlockAddress::get(then),
//                        BlockAddress::get(el),
                        ConstantInt::get(Type::getInt32Ty(getGlobalContext()), bitFieldCount)
					};
					ret = MDNode::get(getGlobalContext(), Elts);
					C->setMetadata("jml.icmp", ret);
				}
				else {
					assert(isa<BranchInst>(it) && "CmpInst not followed by BranchInst!");
				}
			}
            
            if (Instruction *I = dyn_cast<Instruction>(v)) {
                Value *Elts[] = {
                    ConstantInt::get(Type::getInt1Ty(getGlobalContext()), skip ? 1 : 0), // skip
                    ret
                };
				ret = MDNode::get(getGlobalContext(), Elts);
				//NamedMDNode *NMD = M.getOrInsertNamedMetadata("jml.new.var");
				//NMD->addOperand(Node);
				I->setMetadata("jml", ret);
			}
            
            return ret;
		}

        /// Add blocks in between the predecessors and successor block
        /// so we can add a var assignment to help the switch later
        void splitUpEdges(BasicBlock *successor, Module &M)
        {
            Function *F = M.getFunction(FuncToInstrument);
            LoopInfo *LI = h->getLoopInfo(*F);
            // If we're in a loop, don't do this.
            if (LI->getLoopDepth(successor)) {
                return;
            }
            errs() << "splitUpEdges(S=" << successor->getName() << ")\n";
            Type *Ty = IntegerType::get(getGlobalContext(), 32);
            std::string Name("backwards_switch_");
            Name += successor->getName();
            Constant *C = M.getOrInsertGlobal(Name.c_str(), Ty);
            errs() << "Creating " << Name << " metadata\n";
            assert(isa<GlobalVariable>(C) && "Incorrectly typed anchor?");
            GlobalVariable *GV = cast<GlobalVariable>(C);
            
            // If it has an initializer, it is already in the module.
            if (GV->hasInitializer()) {
                assert(0 && "Re-creating existing variable!");
            }
            
            GV->setLinkage(GlobalValue::LinkOnceAnyLinkage);
            GV->setConstant(false);
            
            IRBuilder<> temp(getGlobalContext());
            GV->setInitializer(temp.getInt32(0));
            
            errs() << "Splitting edges.\n";
            int i;
            std::vector<Value *> blocks;
            pred_iterator pi, pe;
            
            if (usingRoss) {
                int numPreds = 0;
                
                for (pi = pred_begin(successor), pe = pred_end(successor); pi != pe; ++pi) {
                    numPreds++;
                }
                
                /// We use zero to be a "reset" bf so we have to add 1 to numPreds
                int numBits = lrint(log2(numPreds + 1));
                errs() << numPreds << " pred edges require " << numBits << " bits\n";
                uint32_t mask = 0;
                mask = (1 << numBits) - 1;
                mask <<= bitFieldCount;
                errs() << "Mask should be: ";
                errs().write_hex(mask) << "\n";
                errs() << "Flipped, that is ";
                errs().write_hex(~mask) << "\n";
                
                for (i = 1, pi = pred_begin(successor), pe = pred_end(successor); pi != pe; ++pi, ++i) {
                    BasicBlock *b = llvm::SplitEdge(*pi, successor, h);
                    blocks.push_back(BlockAddress::get(b));
                    temp.SetInsertPoint(b->getTerminator());
                    Value *v = searchArgumentsAllocas(F->getFunctionType()->getFunctionParamType(1), M, F);
                    assert(v && "nothing was returned");
                    /// Load the bitfield
                    Value *loadBitField = temp.CreateLoad(v);
                    /// Bitcast it
                    Value *bcast = temp.CreateBitCast(loadBitField, Type::getInt32PtrTy(getGlobalContext()));
                    /// Another load
                    Value *al = temp.CreateLoad(bcast);
                    /// Clear the old bits
                    Value *clear = temp.CreateAnd(al, ~mask);
                    /// Turn on appropriate bits
                    Value *newVal = temp.getInt32(i);
                    /// Shift our new val
                    Value *shifted = temp.CreateShl(newVal, bitFieldCount);
                    /// Perform an OR
                    Value *OR = temp.CreateOr(shifted, clear);
                    Value *store = temp.CreateStore(OR, bcast);
                    markJML(store);
                }

                blocks.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()), bitFieldCount));
                blocks.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()), numBits));
                
                bitFieldCount += numBits;
            }
            else {
                for (i = 1, pi = pred_begin(successor), pe = pred_end(successor); pi != pe; ++pi, ++i) {
                    BasicBlock *b = llvm::SplitEdge(*pi, successor, h);
                    blocks.push_back(BlockAddress::get(b));
                    temp.SetInsertPoint(b->getTerminator());
                    Value *store = temp.CreateStore(temp.getInt32(i), GV);
                    markJML(store);
                }
            }

            MDNode *switchPaths = MDNode::get(getGlobalContext(), blocks);
            NamedMDNode *nmd = M.getOrInsertNamedMetadata(Name);
            nmd->addOperand(switchPaths);
        }
        
        StringRef insertCounter(BasicBlock *bb)
        {
            // Load
            // Increment
            // Store
            Instruction *I(bb->begin());
            markJML(I);
			
			//BasicBlock::iterator it(I);
			//++it;
			IRBuilder<> b(bb);
            b.SetInsertPoint(I);
			
			/// Manually emit the load, add, store for this
			/// Don't forget to tag all this JML!
			//const Type *newGlobal = IntegerType::get(I.getContext(), 32);
			//Value *l = M.getOrInsertGlobal("bf", newGlobal);
            
            StringRef bf = newBitFieldName("ctr");
			
			Value *l = insertBitField(bf);
			markJML(l);
			
			Value *ll = b.CreateLoad(l);
			//markJML(ll);
			
			DEBUG(errs() << "Instrumenter: ll has type: " << *ll->getType() << "\n");
			DEBUG(errs() << "Instrumenter: I has type: " << *I->getType() << "\n");
			
			//Value *lll = b.CreateCast(Instruction::Trunc, ll, I->getType());
			//markJML(lll);
			
			//DEBUG(errs() << "Instrumenter: lll has type: " << *lll->getType() << "\n");
			
            Value *one = b.getInt32(1);
            
			Value *v = b.CreateNUWAdd(ll, one);
			//markJML(v);
			
			DEBUG(errs() << "Instrumenter: got here...\n");
			
			Value *llll = b.CreateCast(Instruction::SExt, v, ll->getType());
			//markJML(llll);
			
			DEBUG(errs() << "Instrumenter: llll has type: " << *llll->getType() << "\n");
			
			Value *lllll = b.CreateStore(llll, l);
            //markJML(lllll);
            
            return bf;
        }
        
        void instrumentLoopHeader(BasicBlock *BB, StringRef &sr)
        {
            Function *ForwardFunc = M.getFunction(FuncToInstrument);
            LoopInfo *LI = h->getLoopInfo(*ForwardFunc);
            assert(LI->getLoopDepth(BB));
            
            BasicBlock *parent = LI->getLoopFor(BB)->getLoopPreheader();
            BasicBlock *body = 0;
            
            if (BranchInst *BI = dyn_cast<BranchInst>(BB->getTerminator())) {
                body = BI->getSuccessor(0);
            }
            
            assert(body && "Body not set!");

            Value *Elts[] = {
                /// parent
                BlockAddress::get(parent),
                /// loop body
                BlockAddress::get(body),
                MDString::get(getGlobalContext(), sr)
            };
            MDNode *Node = MDNode::get(getGlobalContext(), Elts);
            BB->getTerminator()->setMetadata("jml.icmp.loop", Node);
        }
        
        void handleLoop(CmpInst &I)
        {
            Function *ForwardFunc = M.getFunction(FuncToInstrument);
            LoopInfo *LI = h->getLoopInfo(*ForwardFunc);
            BasicBlock *BB = I.getParent();
            assert(LI->getLoopDepth(BB));
            /// What we need to do is add a counter to the LATCH of this loop
            /// We don't need a bitfield, it's all controlled by the counter.
            /// From the loop header, we can get to the first block in the loop
            if (LI->isLoopHeader(BB)) {
                errs() << "LOOP HEADER\n\n";
                
                StringRef sr;
                
                BasicBlock *latch = LI->getLoopFor(BB)->getLoopLatch();
                sr = insertCounter(latch);
                
                assert(!sr.empty());
                /// Do some bookkeeping here about this loop in metadata
                instrumentLoopHeader(BB, sr);
            }
        }
		
		void visitCmpInst(CmpInst &I) {
			/*
             * What we need to do here is emit a new global variable and assign
             * it the result of the cmp instruction in the function to be
             * reversed.
             */
            DEBUG(errs() << "Instrumenter: COMPARE INSTRUCTION\n");
			
            Function *ForwardFunc = M.getFunction(FuncToInstrument);
            LoopInfo *LI = h->getLoopInfo(*ForwardFunc);
            /// Handle CmpInsts inside of loops a little differently...
            if (LI->getLoopDepth(I.getParent())) {
                handleLoop(I);
                // early exit from this function
                return;
            }
		}

        void visitStoreInst(StoreInst &I) {
            overlap(I);

            if (Constant *C = dyn_cast<Constant>(I.getValueOperand())) {
                errs() << "Assigning a Constant: " << *C << "\n";
                
                Type *ty = C->getType();
                
                errs() << "We'll have to make space for a " << *ty << "\n";
                GlobalVariable *gvar_for_constant;
                /// GlobalVariable ctor - If a parent module is specified, the global is
                /// automatically inserted into the end of the specified modules global list.
//                GlobalVariable(Type *Ty, bool isConstant, LinkageTypes Linkage,
//                               Constant *Initializer = 0, const Twine &Name = "",
//                               bool ThreadLocal = false, unsigned AddressSpace = 0);
                Constant *initializer = Constant::getNullValue(ty);
                gvar_for_constant = new GlobalVariable(M, C->getType(), false,
                                                       GlobalVariable::CommonLinkage, 0, "foobar");
                gvar_for_constant->setInitializer(initializer);
                // Get the original value
                Value *foo = new LoadInst(I.getPointerOperand(), "saveValue", &I);
                Value *bar = new StoreInst(foo, gvar_for_constant, &I);
                // We have to adjust markJML to add MD that doesn't cause skips
                markJML(foo);
                markJML(bar);
                
                /// Create the MD for this swap
                Value *Elts[] = {
                    gvar_for_constant
                };
				MDNode *extra = MDNode::get(getGlobalContext(), Elts);
                I.setMetadata("jml.swap", extra);
				markJML(&I, false);
            }
        }

        void visitAllocaInst(AllocaInst &I)
        {
            allocas.push_back(&I);
            markJML(&I);
            // See if we have any users
            for (Value::use_iterator i = I.use_begin(), e = I.use_end(); i != e; ++i) {
                Value *v = *i;
                if (StoreInst *s = dyn_cast<StoreInst>(v)) {
                    for (User::op_iterator i = s->op_begin(), e = s->op_end();
                         i != e; ++i) {
                        Value *v = *i;

                        if (Argument *a = dyn_cast<Argument>(v)) {
                            errs() << "We have an argument!\n";
                            errs() << "It's arg no. " << a->getArgNo() << "\n";
                            errs() << "Assuming symmetric arguments.\n";

                            args.push_back(s);
                            markJML(s);
                            return;
                        }
                    }
                }
            }
        }

	};
    
    class Hello;
    
#pragma mark
#pragma mark Inverter

#undef DEBUG_TYPE
#define DEBUG_TYPE "Inverter"
    
    std::map<Value *, Value *> oldToNew;

    class Inverter : public InstVisitor<Inverter>
    {
		IRBuilder<> &builder;
		Module &M;
        Hello *h;
		
    public:
        
		Inverter(IRBuilder<> &b, Module &mod, Hello *H): builder(b), M(mod), h(H) {
            Function *f = M.getFunction(FuncToInstrument);
            Function *g = M.getFunction(FuncToGenerate);
            Function::arg_iterator fi, fe, gi, ge;
            
            for (fi = f->arg_begin(), fe = f->arg_end(), gi = g->arg_begin(),
                 ge = g->arg_end(); fi != fe; ++fi, ++gi) {
                oldToNew[fi] = gi;
            }
		}
		
		Value * lookup(Value *k) {
            if (isa<Constant>(k)) {
                return k;
            }
			DEBUG(errs() << "\n\n\nInverter: LOOKUP\n");
            DEBUG(k->dump());
			std::map<Value *, Value *>::iterator it;
			
			it = oldToNew.find(k);
			if (it == oldToNew.end()) {
				DEBUG(errs() << "Inverter: " << *k << " not found, outputting map\n");
				outputMap();
				return 0;
			}
			
			return it->second;
		}
        
        /// This lookup cannot return 0. If it isn't in the mapping, return k
        Value * lookupNotNull(Value *k)
        {
            Value *v = lookup(k);
            if (!v) {
                return k;
            }
            return v;
        }
		
		void outputMap() {
			std::map<Value *, Value *>::iterator it, e;
			
			DEBUG(errs() << "Inverter: MAP BEGIN\n");
			for (it = oldToNew.begin(), e = oldToNew.end(); it != e; ++it) {
				DEBUG(errs() << "Inverter: key: " << *it->first << "\n\tdata: " << *it->second << "\n");
			}
			DEBUG(errs() << "Inverter: MAP END\n");
		}
                
        void visitSExtInst(SExtInst &I)
        {
            if (lookup(&I)) {
                // Already created a counterpart for this instruction, exit
                return;
            }
            DEBUG(errs() << "Inverter: SExtInst\n");
            
            handleDeps(I);
            
            Value *v = builder.CreateSExt(lookup(I.getOperand(0)), I.getDestTy());
            
            oldToNew[&I] = v;
        }
        
        void visitBitCast(BitCastInst &I)
        {
            if (lookup(&I)) {
                // Already created a counterpart for this instruction, exit
                return;
            }
            DEBUG(errs() << "Inverter: BitCastInst\n");
            DEBUG(errs() << I << "\n");
            
            handleDeps(I);
            
            errs() << "Converting " << *I.getSrcTy() << " to ";
            errs() << *I.getDestTy() << "\n";
            
            Value *value = I.getOperand(0);
            value = lookupNotNull(value);
            
            Value *v = builder.CreateBitCast(value, I.getDestTy());
            oldToNew[&I] = v;
        }
		
        void visitGetElementPtrInst(GetElementPtrInst &I)
        {
            if (lookup(&I)) {
                // Already created a counterpart for this instruction, exit
                return;
            }
            DEBUG(errs() << "Inverter: GetElementPtrInst\n");

            handleDeps(I);
            
            Value *storeVal = I.getPointerOperand();
            
            if (isa<GlobalValue>(storeVal)) {
                DEBUG(errs() << "Inverter: " << storeVal->getName() << " is a global value\n");
            }
            else {
                DEBUG(errs() << "Inverter: " << storeVal->getName() << " is not a global value\n");
                storeVal = lookup(storeVal);
            }

            // Pass arguments to I into our new GEP instruction
            std::vector<Value *> arr;
            for (unsigned i = 1, count = I.getNumOperands(); i < count; i++) {
                arr.push_back(lookupNotNull(I.getOperand(i)));
            }
            Value *GEP = builder.CreateGEP(lookupNotNull(I.getOperand(0)), arr);
            
            oldToNew[&I] = GEP;
            
            return;
        }
        
        BasicBlock *createUnreachable()
        {
            Function *f = M.getFunction(FuncToGenerate);
            BasicBlock *ret = BasicBlock::Create(getGlobalContext(), "", f);
            IRBuilder<> temp(getGlobalContext());
            temp.SetInsertPoint(ret);
            temp.CreateUnreachable();
            
            return ret;
        }
        
		void visitTerminatorInst(TerminatorInst &I) {
            if (lookup(&I)) {
                // Already created a counterpart for this instruction, exit
                return;
            }
			DEBUG(errs() << "\n\n\nInverter: TERMINATOR INSTRUCTION\n");
            DEBUG(errs() << "Inverter: For " << I.getParent()->getName() << "\n");
			
			int count = 0;
			
            std::vector<BasicBlock *> Preds;
            
            // bb is the original basic block to be reversed
			BasicBlock *bb = I.getParent();
            
            for (pred_iterator PI = pred_begin(bb), E = pred_end(bb); PI != E; ++PI) {
                BasicBlock *Pred = *PI;
                DEBUG(errs() << "Inverter: pred of " << bb->getName() << ": " << Pred->getName() << "\n");
                Preds.push_back(Pred);
                count++;
            }
            
            Function *f = bb->getParent();
            LoopInfo *LI = h->getLoopInfo(*f);
            
            /// Standard loop construct
            if (count == 2 && LI->getLoopDepth(bb)) {
                /// 1. Get the loop header
                BasicBlock *header = LI->getLoopFor(bb)->getHeader();
                assert(header == I.getParent());
                /// 2. Find the reverse analog to 1
                BasicBlock *analogHeader = bbmOldToNew[header];
                /// 3. Insert instructions to load and decrement the ctr var.
                MDNode *md = header->getTerminator()->getMetadata("jml.icmp.loop");
                assert(md && "Loop metadata not found!");
                BlockAddress *parent = dyn_cast<BlockAddress>(md->getOperand(0));
                assert(parent && "Parent not found!");
                //errs() << parent->getString() << "\n";
                BlockAddress *body = dyn_cast<BlockAddress>(md->getOperand(1));
                assert(body && "Body not found!");
                //errs() << body->getString() << "\n";
                MDString *ctrNum = dyn_cast<MDString>(md->getOperand(2));
                DEBUG(errs() << "ctrNum is " << *ctrNum << "\n");
                DEBUG(errs() << "LOOP HEADER\n\n");
                DEBUG(header->dump());
                DEBUG(errs() << "\nANALOG HEADER\n\n");
                DEBUG(analogHeader->dump());
                DEBUG(errs() << "\n");
                
                Type *newGlobal = IntegerType::get(I.getContext(), 32);

				Value *l = M.getOrInsertGlobal(ctrNum->getString(), newGlobal);
                
                Value *ll = builder.CreateLoad(l);
                
                Value *lll = builder.CreateICmpSGT(ll, builder.getInt32(0));
                
                /// 4. Depending on 3, loop or exit loop
                /// WATCH RIGHT HERE I'M GOING TO CHEAT
                
                BasicBlock *latch = LI->getLoopFor(bb)->getLoopLatch();
                BasicBlock *newBody = bbmOldToNew[latch];
                BasicBlock *newParent = bbmOldToNew[parent->getBasicBlock()];
                builder.CreateCondBr(lll, newBody, newParent);
                
                return;
            }
            
            if (count >= 2) {
                // We have multiple predecessors. We're going to need a switch
                // Load backward_switch_<BB label>
                std::string globalName("backwards_switch_");
                globalName += bb->getName();
                Value *l = M.getOrInsertGlobal(globalName, Type::getInt32Ty(getGlobalContext()));
                Value *load = builder.CreateLoad(l, globalName);
                // Use that for switch
                //NamedMDNode *nmd = M.getOrInsertNamedMetadata(globalName);
                NamedMDNode *nmd = M.getNamedMetadata(globalName);
                assert(nmd->getNumOperands() && "No operands found in NamedMD");
                MDNode *node = cast<MDNode>(nmd->getOperand(0));
                assert(node->getNumOperands() && "No operands found in MDNode");
                BlockAddress *bb = cast<BlockAddress>(node->getOperand(0));
                BasicBlock *block = bb->getBasicBlock();
                Value *switchInst;
                
                unsigned numOperands;
                
                if (usingRoss) {
                    Function *F = M.getFunction(FuncToGenerate);
                    int numOps = node->getNumOperands();
                    numOperands = numOps - 2;
                    
                    int numBits = cast<ConstantInt>(node->getOperand(numOps - 1))->getZExtValue();
                    int bfc = cast<ConstantInt>(node->getOperand(numOps - 2))->getZExtValue();
                    uint32_t mask = 0;
                    mask = (1 << numBits) - 1;
                    mask <<= bfc;
                    errs() << "Mask should be: ";
                    errs().write_hex(mask) << "\n";

                    Value *v = searchArgumentsAllocas(F->getFunctionType()->getFunctionParamType(1), M, F);
                    assert(v && "nothing was returned");
                    /// Load the bitfield
                    Value *loadBitField = builder.CreateLoad(v);
                    /// Bitcast it
                    Value *bcast = builder.CreateBitCast(loadBitField, Type::getInt32PtrTy(getGlobalContext()));
                    /// Another load
                    Value *al = builder.CreateLoad(bcast);
                    /// AND that value
                    Value *And = builder.CreateAnd(al, mask);
                    /// Shift to the right
                    Value *shifted = builder.CreateLShr(And, bfc);
                    /// Create a new shift
                    switchInst = builder.CreateSwitch(shifted, createUnreachable());
                }
                else {
                    numOperands = node->getNumOperands();
                    switchInst = builder.CreateSwitch(load, createUnreachable());
                }
                
                for (unsigned i = 0; i < numOperands; ++i) {
                    bb = cast<BlockAddress>(node->getOperand(i));
                    block = bb->getBasicBlock();
                    block = bbmOldToNew[block];
                    ConstantInt *ci = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), i + 1);
                    cast<SwitchInst>(switchInst)->addCase(ci, block);
                }
                return;
            }
			
            // foo is the generated "inverse" basic block
			BasicBlock *foo = builder.GetInsertBlock();
			
			DEBUG(errs() << "Inverter: count is " << count << "\n");
			
			/// If count is zero, it must be the entry block so make it exit
			if (count == 0) {
				builder.CreateRetVoid();
				return;
			}
			
			/// Count is == 1 -- find the pred
			if (count == 1) {
				BasicBlock *pred = *pred_begin(bb);
				BasicBlock *newSucc = bbmOldToNew[pred];
				builder.CreateBr(newSucc);
				return;
			}
			
			assert(0 && "Unhandled terminator instruction!\n");
		}
		
		void visitAllocaInst(AllocaInst &I) {
            if (lookup(&I)) {
                // Already created a counterpart for this instruction, exit
                return;
            }
			DEBUG(errs() << "\n\n\nInverter: ALLOCA INSTRUCTION\n");
            
            handleDeps(I);

			Value *alloc = builder.CreateAlloca(I.getAllocatedType(), 0, I.getName());
			DEBUG(errs() << "Inverter: Allocating a " << *I.getType() << "\n");
			
			oldToNew[&I] = alloc;
		}
        
        void visitStoreInst(StoreInst &I) {
            if (lookup(&I)) {
                // Already created a counterpart for this instruction, exit
                return;
            }
            DEBUG(errs() << "\n\n\nInverter: STORE INSTRUCTION\n");
            
            handleDeps(I);
            
            /// This is really hacky but should work for now
            if (MDNode *md = I.getMetadata("jml.swap")) {
                Value *originalValue = md->getOperand(0);
                Value *ov = builder.CreateLoad(originalValue);
                builder.CreateStore(ov, lookup(I.getPointerOperand()));
                
                return;
            }
            
            Value *pointerOperand = I.getPointerOperand();
			if (isa<GlobalValue>(pointerOperand) /*|| isa<GetElementPtrInst>(storeVal)*/) {
                DEBUG(errs() << "Inverter: " << pointerOperand->getName() << " is a global value\n");
            }
            else {
                DEBUG(errs() << "Inverter: " << pointerOperand->getName() << " is not a global value\n");
                pointerOperand = lookup(pointerOperand);
            }

            Value *lval = lookup(I.getValueOperand());
            // If lval == 0, it's something we don't want to reverse (e.g. tw_event_new calls)
            if (!lval) {
                return;
            }

            lval = builder.CreateBitCast(lval, pointerOperand->getType()->getPointerElementType());

            builder.CreateStore(lval, pointerOperand);
        }
		
		void visitLoadInst(LoadInst &I) {
            if (lookup(&I)) {
                // Already created a counterpart for this instruction, exit
                return;
            }
			DEBUG(errs() << "\n\n\nInverter: LOAD INSTRUCTION\n");

            handleDeps(I);

			Value *valOfI = &I;
			
			Value *loadVal = I.getPointerOperand();
			if (isa<GlobalValue>(loadVal)) {
				DEBUG(errs() << "Inverter: " << loadVal->getName() << " is a global value\n");
			}
			else {
				DEBUG(errs() << "Inverter: " <<loadVal->getName() << " is not a global value\n");
                loadVal = lookup(loadVal);
			}
			
			/// Create a new load
			LoadInst *inst = builder.CreateLoad(loadVal);
			
			oldToNew[valOfI] = inst;
			outputMap();
			DEBUG(errs() << "\n\n\nInverter: LOAD INSTRUCTION END\n");
		}
        
        void visitCallInst(CallInst &I) {
            if (lookup(&I)) {
                // Already created a counterpart for this instruction, exit
                return;
            }
            DEBUG(errs() << "\n\n\nInverter: CALL INSTRUCTION\n");

            Function *fun = I.getCalledFunction();
            StringRef str = fun->getName();
            DEBUG(errs() << str << " called\n");
            
            if (str == "rng_gen_val" ||
                str == "tw_rand_integer" ||
                str == "tw_rand_exponential") {

                handleDeps(I);

                errs() << "We need to reverse the RNG\n";
                // Fortunately, all the rng functions pass the tw_rng_stream as their first param
                Value *G = I.getArgOperand(0);
                Function *reverse_rng = M.getFunction("rng_gen_reverse_val");
                assert(reverse_rng && "rng_gen_reverse_val not found!");
                Value *v = builder.CreateCall(reverse_rng, lookup(G));
                oldToNew[&I] = v;
            }
            
            /// At some point in the future, we may switch this with a zero
            /// value.  For now, this is the quickest way.
            if (str == "tw_now") {
                errs() << "Calling tw_now()\n";

                handleDeps(I);

                Value *G = I.getArgOperand(0);
                Function *now = M.getFunction("tw_now");
                assert(now && "tw_now not found!");
                Value *v = builder.CreateCall(now, lookup(G));
                oldToNew[&I] = v;
            }
        }
		
		void visitBinaryOperator(BinaryOperator &I) {
            if (lookup(&I)) {
                // Already created a counterpart for this instruction, exit
                return;
            }
			DEBUG(errs() << "\n\n\nBINARY OPERATOR\n");
            
            handleDeps(I);
            
            Value *newInstruction = 0;
            
            switch (I.getOpcode()) {
                case Instruction::Add:
                    DEBUG(errs() << "ADD INSTRUCTION\n");
                    DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                    DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
                    newInstruction = builder.CreateSub(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
                    break;
                    
                case Instruction::FAdd:
                    DEBUG(errs() << "FADD INSTRUCTION\n");
                    DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                    DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
                    newInstruction = builder.CreateFSub(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
                    break;
                    
                case Instruction::Sub:
                    DEBUG(errs() << "SUB INSTRUCTION\n");
                    DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                    DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
                    newInstruction = builder.CreateAdd(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
                    break;
                    
                case Instruction::FSub:
                    DEBUG(errs() << "FSUB INSTRUCTION\n");
                    DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                    DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
                    newInstruction = builder.CreateFAdd(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
                    break;
                    
                case Instruction::Mul:
                    DEBUG(errs() << "MULT INSTRUCTION\n");
                    DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                    DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
                    newInstruction = builder.CreateSDiv(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
                    break;
                    
                default:
                    assert(0 && "visitBinaryOperator fell through!");
                    break;
            }
			
			oldToNew[&I] = newInstruction;
		}
        
        /// Emit all necessary dependencies for Instruction I
        void handleDeps(Instruction &I)
        {
            std::vector<Value *> bucket;
            
            Function *f = I.getParent()->getParent();
            
            getUseDef(&I, bucket, f);
            
			DEBUG(errs() << "Inverter: " << I << "\n");
			DEBUG(errs() << "Inverter: Bucket contains:\n");
			for (std::vector<Value *>::iterator it = bucket.begin(), e = bucket.end();
				 it != e; ++it) {
				DEBUG(errs() << "Inverter: " << **it << "\n");
			}
            
			while (bucket.size()) {
				Value *v = bucket.back();
				bucket.pop_back();
                
                if (!lookup(v)) {
                    if (Instruction *i = dyn_cast<Instruction>(v)) {
                        visit(i);
                    }
                }
			}
        }
        
    };
    
#pragma mark
#pragma mark Hello
    
#undef DEBUG_TYPE
#define DEBUG_TYPE "Hello"
    
    LoopInfo * Hello::getLoopInfo(Function &f)
    {
        LoopInfo &LI = getAnalysis<LoopInfo>(f);
        
        return &LI;
    }
    
    Function *Hello::createReverseFunction(Module &M)
    {
        Function *forward = M.getFunction(FuncToInstrument);
        
        Constant *temp = M.getOrInsertFunction(FuncToGenerate,
                                               forward->getFunctionType());
        
        if (!temp) {
            DEBUG(errs() << "We have a problem\n");
            exit(-1);
        }
        
        Function *reverse = cast<Function>(temp);
        if (reverse) {
            DEBUG(errs() << "OK supposedly we inserted " << FuncToGenerate << "\n");
        }
        else {
            DEBUG(errs() << "Problem inserting function " << FuncToGenerate << "\n");
            exit(-1);
        }
        
        Function::arg_iterator fi, fe, gi, ge;
        for (fi = forward->arg_begin(), fe = forward->arg_end(),
             gi = reverse->arg_begin(), ge = reverse->arg_end(); fi != fe;
             ++fi, ++gi) {
            gi->setName(fi->getName());
        }
        
        return reverse;
    }
    
    
    /**
     This function assumes that the IR is single-exit so we have to
     run that pass first
     */
    BasicBlock *Hello::findExitBlock(Function &F)
    {
        Function::iterator it, E;
        
        BasicBlock *ret = 0;
        
        for (it = F.begin(), E = F.end(); it != E; ++it) {
            BasicBlock *b = it;
            
            if (isa<ReturnInst>(b->getTerminator())) {
                DEBUG(errs() << "We found the return instruction!\n");
                DEBUG(errs() << "findExitBlock returning: " << *b);
                if (ret) {
                    errs() << "Error: multiple function exits found\n";
                    errs() << "use the -mergereturn opt pass\n";
                    exit(-1);
                }
                ret = b;
            }
        }
        
        assert(ret && "No return instruction found!");
        
        return ret;
    }
    
    void Hello::getAnalysisUsage(AnalysisUsage &Info) const
    {
        //Info.addRequired<AliasAnalysis>();
        Info.addRequired<MemoryDependenceAnalysis>();
        Info.addRequired<PostDominatorTree>();
        Info.addRequiredTransitive<DominatorTree>();
        Info.addRequired<LoopInfo>();
        //Info.addRequiredTransitive<MemoryDependenceAnalysis>();
    }

    bool Hello::runOnModule(Module &M) {
        if (Function *ForwardFunc = M.getFunction(FuncToInstrument)) {
            ////////////////////////////////////////
            /// Instrument the forward event handler
            ////////////////////////////////////////
            
            if (M.getGlobalVariable("__USING_ROSS")) {
                errs() << "ROSS Mode assumes functions have appropriate arguments\n";
                usingRoss = true;
            }
            
            errs() << "Instrumentation phase beginning...\n\n";
            
            // Make sure all blocks have names...
            Function::iterator fi, fe;
            for (fi = ForwardFunc->begin(), fe = ForwardFunc->end(); fi != fe; ++fi) {
                BasicBlock *bb = fi;
                if (!fi->hasName()) {
                    if (&ForwardFunc->getEntryBlock() == bb) {
                        bb->setName("entry");
                        continue;
                    }
                    bb->setName("bb");
                }
            }
            
            Instrumenter instrumenter(M, this);
            
            std::set<BasicBlock *> workList;
            for (fi = ForwardFunc->begin(), fe = ForwardFunc->end(); fi != fe; ++fi) {
                BasicBlock *bb = fi;
                std::vector<BasicBlock *> Preds;
                for (pred_iterator PI = pred_begin(bb), PEND = pred_end(bb);
                     PI != PEND; ++PI) {
                    Preds.push_back(*PI);
                }
                if (Preds.size() >= 2) {
                    workList.insert(bb);
                }
            }
            
            for (std::set<BasicBlock*>::iterator wi = workList.begin(),
                 we = workList.end(); wi != we; ++wi) {
                instrumenter.splitUpEdges(*wi, M);
            }

            /// Store argument users
            getArgUsers(ForwardFunc);

            for (inst_iterator I = inst_begin(ForwardFunc), E = inst_end(ForwardFunc); I != E; ++I) {
                // Needed here due to Select approach which may have added
                // metadata already...
                if (MDNode *N = I->getMetadata("jml")) {
                    Atadatem md(N);
                    if (md.isSkip()) {
                        continue;
                    }
                }
                instrumenter.visit(&*I);
            }

            errs() << "Instrumentation complete.\n";
            errs() << "No more modification to the input function.\n";

            Function *target = createReverseFunction(M);

            target->deleteBody();

            /// Put exit BB first in function
            BasicBlock *bb = findExitBlock(*ForwardFunc);
            BasicBlock *newBlock = BasicBlock::Create(getGlobalContext(),
                                                      "return_rev", target);
            bbmOldToNew[bb] = newBlock;
            bbmNewToOld[newBlock] = bb;

            while (allocas.size()) {
                Instruction *inst = allocas.back();
                allocas.pop_back();
                IRBuilder<> builder(newBlock);
                Inverter inv(builder, M, this);
                inv.visit(inst);
            }

            while (args.size()) {
                Instruction *inst = args.back();
                args.pop_back();
                IRBuilder<> builder(newBlock);
                Inverter inv(builder, M, this);
                inv.visit(inst);
            }

            /// Make analogs to all BBs in function
            for (fi = ForwardFunc->begin(), fe = ForwardFunc->end(); fi != fe; ++fi) {
                if (bb == fi) {
                    continue;
                }
                BasicBlock *block = BasicBlock::Create(getGlobalContext(),
                                                       fi->getName() +
                                                       "_rev", target);
                bbmOldToNew[fi] = block;
                bbmNewToOld[block] = fi;
            }
            
            errs() << "Piece-wise (BB-based) inversion phase beginning...\n\n";
            
            std::vector<BasicBlock *> fifo;
            
            unsigned sccNum = 0;
            errs() << "SCCs for Function " << ForwardFunc->getName() << " in PostOrder:";
            for (scc_iterator<Function*> SCCI = scc_begin(ForwardFunc),
                 E = scc_end(ForwardFunc); SCCI != E; ++SCCI) {
                std::vector<BasicBlock*> &nextSCC = *SCCI;
                errs() << "\nSCC #" << ++sccNum << " : ";
                for (std::vector<BasicBlock*>::iterator I = nextSCC.begin(),
                     E = nextSCC.end(); I != E; ++I) {
                    errs() << (*I)->getName() << ", ";
                    BasicBlock *foobar = *I;
                    fifo.push_back(foobar);
                }
                if (nextSCC.size() == 1 && SCCI.hasLoop())
                    errs() << " (Has self-loop).";
            }
            errs() << "\n";
                        
            for (unsigned int i = 0; i < fifo.size(); ++i) {
                DEBUG(errs() << "Looking at " << fifo[i]->getName() << "\n");
                BasicBlock *fi = fifo[i];
            //fifo.pop_front();
            //for (fi = ForwardFunc->begin(), fe = ForwardFunc->end(); fi != fe; ++fi) {
                
                BasicBlock *block = bbmOldToNew[fi];
                
                DEBUG(errs() << block->getName() << " corresponding to "
                      << fi->getName() << "\n");
                
                IRBuilder<> builder(block);
                /// Create a new Inverter (one for each BB)
                Inverter inv(builder, M, this);
                
                std::stack<Instruction*> stores;
                
                for (BasicBlock::iterator j = fi->begin(), k = fi->end(); j != k; ++j) {
                    if (MDNode *N = j->getMetadata("jml")) {
                        Atadatem md(N);
                        if (md.isSkip()) {
                            continue;
                        }
                    }
                    
                    /// Leave the Alloca visitor but just don't call it here
                    /// Handle it via Store visitor like all others
//                    if (AllocaInst *a = dyn_cast<AllocaInst>(j)) {
//                        inv.visitAllocaInst(*a);
//                    }
                    
                    if (StoreInst *b = dyn_cast<StoreInst>(j)) {
                        //negateStoreInstruction(b);
                        //Inverter inv;
                        
                        //AliasAnalysis AA = getAnalysis<AliasAnalysis>();
                        
                        //errs() << "Got AA\n";
                        
                        MemDepResult mdr;
                        
                        MemoryDependenceAnalysis& mda = getAnalysis<MemoryDependenceAnalysis>(*ForwardFunc);
                        
                        DEBUG(errs() << "Got MDR\n");
                        
                        mdr = mda.getDependency(b);
                        
                        DEBUG(errs() << "Instruction " << *b << ": ");
                        
                        DEBUG(errs() << "\nMDR: " << *mdr.getInst() << "\n");
                        
                        // Make sure local stores only store to local loads
//                        if (!isa<GlobalValue>(b->getPointerOperand())) {
//                            if (LoadInst *l = dyn_cast<LoadInst>(mdr.getInst())) {
//                                assert(l->getPointerOperand() == b->getPointerOperand());
//                            }
//                        }
                        
                        // TODO: Get rid of the MDA stuff and just find out if
                        // the place we're storing to is an alloca
                        
                        if (AllocaInst *a = dyn_cast<AllocaInst>(mdr.getInst())) {
                            /// We have a local variable that we're storing into
                            /// Add an AllocaInst and update the mappings for lookup()
                            inv.visitAllocaInst(*a);
                            inv.visitStoreInst(*b);
                        }
                        else {
                            stores.push(b);
                        }
                    }
                    
                    if (CallInst *c = dyn_cast<CallInst>(j)) {
                        stores.push(c);
                    }
                }
                
                Instruction *cur;
                while (stores.size()) {
                    cur = stores.top();
                    stores.pop();
                    
                    if (StoreInst *store = dyn_cast<StoreInst>(cur)) {
                        inv.visitStoreInst(*store);
                    }
                    if (CallInst *call = dyn_cast<CallInst>(cur)) {
                        inv.visitCallInst(*call);
                    }
                }
                
                inv.visitTerminatorInst(*fi->getTerminator());
                
                DEBUG(errs() << "BASIC BLOCK: ");
                DEBUG(errs() << block->getName() << "\n");
            }
            
            return true;
        }
        else {
            errs() << "Error: unable to find function " << FuncToInstrument << "\n";
        }
        return true;
    }
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");
