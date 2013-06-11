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

using namespace llvm;



namespace {
    static cl::opt<std::string> FuncToInstrument("rev-func",
										 cl::desc("<func to reverse>"));//, llvm::cl::Required);	
	
	static cl::opt<std::string> FuncToGenerate("tgt-func",
										   cl::desc("<func to output>"));//, llvm::cl::Required);
	
    /// Map basic blocks from (old) event handler to (new) synthesized
    /// reverse event handler
	std::map<BasicBlock *, BasicBlock *> bbmOldToNew;
    
    /// atadatem = reverse metadata
    class Atadatem
    {
        const MDNode *mdnode;
        
        bool skip = false;
        
    public:
        explicit Atadatem(const MDNode *N) : mdnode(N) {}
        
        operator MDNode *() const { return const_cast<MDNode*>(mdnode); }
        MDNode *operator ->() const { return const_cast<MDNode*>(mdnode); }
        
        bool isSkip() const { return skip; }
        void setSkip(bool b) { skip = b; }
    };
	
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
		
		void markJML(Value *v) {
			if (Instruction *I = dyn_cast<Instruction>(v)) {
				MDNode *Node = MDNode::get(getGlobalContext(), 0);
				//NamedMDNode *NMD = M.getOrInsertNamedMetadata("jml.new.var");
				//NMD->addOperand(Node);
				I->setMetadata("jml", Node);
			}
			
			if (CmpInst *I = dyn_cast<CmpInst>(v)) {
				/// Assert that the next instruction is a branch
				BasicBlock::iterator it(I);
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
					MDNode *Node = MDNode::get(getGlobalContext(), Elts);
					I->setMetadata("jml.icmp", Node);
				}
				else {
					assert(isa<BranchInst>(it) && "CmpInst not followed by BranchInst!");
				}
			}
		}
        
        void throwOutPred(SmallVector<BasicBlock *, 4> &Preds)
        {
            assert(Preds.size() < 4 && "Don't yet support arb. size preds!");
            
            if (h->findDom(Preds[0]) == h->findDom(Preds[1])) {
                Preds.erase(&Preds[2]);
                return;
            }
            if (h->findDom(Preds[0]) == h->findDom(Preds[2])) {
                Preds.erase(&Preds[1]);
                return;
            }
            if (h->findDom(Preds[1]) == h->findDom(Preds[2])) {
                Preds.erase(&Preds[0]);
                return;
            }
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
			
#if 0
			{
				if (I.getMetadata("jml")) {
					return;
				}
				
				/* Instruction *pi = ...;
				 Instruction *newInst = new Instruction(...);
				 
				 pi->getParent()->getInstList().insert(pi, newInst); */
				
				Instruction *pi = &I;
				Instruction *newInst = I.clone();
				
				/*
				 This is from http://llvm.org/docs/SourceLevelDebugging.html
				 
				 if (MDNode *N = I->getMetadata("dbg")) {  // Here I is an LLVM instruction
				 DILocation Loc(N);                      // DILocation is in DebugInfo.h
				 unsigned Line = Loc.getLineNumber();
				 StringRef File = Loc.getFilename();
				 StringRef Dir = Loc.getDirectory();
				 }
				 */
				
				/// See DIBuilder.cpp for more examples
				Value *Elts[] = {
					//MDString::get(getGlobalContext(), "jml.new.var"),
					ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0),
					ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0),
					llvm::Constant::getNullValue(Type::getInt32Ty(getGlobalContext())),
					ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0),
					llvm::Constant::getNullValue(Type::getInt32Ty(getGlobalContext())),
				};
				
				MDNode *Node = MDNode::get(getGlobalContext(), 0);
				//NamedMDNode *NMD = M.getOrInsertNamedMetadata("jml.new.var");
				//NMD->addOperand(Node);
				
				newInst->setMetadata("jml", Node);
				
				if (newInst->hasMetadata()) {
					errs() << "has metadata\n";
				}
				else {
					errs() << "does not have metadata\n";
				}
				if (MDNode *N = newInst->getMetadata("jml")) {
					errs() << "We found JML\n";
				}
				pi->getParent()->getInstList().insertAfter(pi, newInst);
			}
#endif
            Function *ForwardFunc = M.getFunction(FuncToInstrument);
            LoopInfo *LI = h->getLoopInfo(*ForwardFunc);
            /// Handle CmpInsts inside of loops a little differently...
            if (LI->getLoopDepth(I.getParent())) {
                handleLoop(I);
                // early exit from this function
                return;
            }
			
            // We are creating a dangerous coupling here between our metadata
            // necessary for marking a cmp instruction and our bitfield count.
            // We'll have to revisit this if it becomes a problem
			markJML(&I);
			
			BasicBlock::iterator it(I);
			++it;
			IRBuilder<> b(it);
			
			/// Manually emit the load, add, store for this
			/// Don't forget to tag all this JML!
			//const Type *newGlobal = IntegerType::get(I.getContext(), 32);
			//Value *l = M.getOrInsertGlobal("bf", newGlobal);
            
            StringRef bf = newBitFieldName("bf");
			
			Value *l = insertBitField(bf);
			markJML(l);
			
			Value *ll = b.CreateLoad(l);
			markJML(ll);
			
			DEBUG(errs() << "Instrumenter: ll has type: " << *ll->getType() << "\n");
			DEBUG(errs() << "Instrumenter: I has type: " << *I.getType() << "\n");
			
			Value *lll = b.CreateCast(Instruction::Trunc, ll, I.getType());
			markJML(lll);
			
			DEBUG(errs() << "Instrumenter: lll has type: " << *lll->getType() << "\n");
			
			Value *v = b.CreateNUWAdd(lll, &I);
			markJML(v);
			
			DEBUG(errs() << "Instrumenter: got here...\n");
			
			Value *llll = b.CreateCast(Instruction::SExt, v, ll->getType());
			markJML(llll);
			
			DEBUG(errs() << "Instrumenter: llll has type: " << *llll->getType() << "\n");
			
			markJML(b.CreateStore(llll, l));
			
			
			DEBUG(errs() << "Instrumenter: Here, too!\n");
            
            
            
            //////////////// BIG CHANGE HERE //////////////////////
            /// Create a diamond!  Unfortunately llvm will not generate
            /// a merge point with just two preds (it just adds more on)
            /// so we'll need to add another BB after the merge
            BasicBlock *bb = h->postdomTreeLookup(I.getParent());
            
            DEBUG(errs() << "Instrumenter: PostDom of " << I.getParent()->getName()
                  << " is " << bb->getName() << "\n");
            
            int pcount = 0;
            
            SmallVector<BasicBlock *, 4> Preds;
            
            // Count predecessors
            for (pred_iterator PI = pred_begin(bb), E = pred_end(bb); PI != E; ++PI) {
                BasicBlock *Pred = *PI;
                DEBUG(errs() << "Instrumenter: pred[" << pcount << "] is " << Pred->getName() << "\n");
                Preds.push_back(Pred);
                pcount++;
                // ...
            }
            
            if (pcount < 3) {
                return;
            }
            
            // Throw out one of the preds
            if (Preds.size() > 2) {
                throwOutPred(Preds);
            }
            
            DEBUG(errs() << "Instrumenter: We have " << Preds.size() << " predecessors\n");
            
            llvm::SplitBlockPredecessors(bb, Preds, "_diamond");
		}
#if 0
        void visitStoreInst(StoreInst &I) {
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
                markJML(foo);
                markJML(bar);
            }
        }
#endif
	};
    
    class Hello;
    
#pragma mark
#pragma mark Inverter

#undef DEBUG_TYPE
#define DEBUG_TYPE "Inverter"
    
    class Inverter : public InstVisitor<Inverter>
    {
		bool currently_reversing;
		std::map<Value *, Value *> oldToNew;
		IRBuilder<> &builder;
		Value *lastVal;
		Module &M;
        Hello *h;
		
    public:
        
		Inverter(IRBuilder<> &b, Module &mod, Hello *H): builder(b), M(mod), h(H) { 
			currently_reversing = true;
			lastVal = 0;
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
		
		void outputMap() {
			std::map<Value *, Value *>::iterator it, e;
			
			DEBUG(errs() << "Inverter: MAP BEGIN\n");
			for (it = oldToNew.begin(), e = oldToNew.end(); it != e; ++it) {
				DEBUG(errs() << "Inverter: key: " << *it->first << "\n\tdata: " << *it->second << "\n");
			}
			DEBUG(errs() << "Inverter: MAP END\n");
		}
        
        MDNode * findDiamondBf(Instruction &I)
        {
            DEBUG(errs() << "Inverter: findDiamondBf(" << I << "): ");
            MDNode * md = h->domTreeLookup(I);
            if (md) {
                DEBUG(errs() << *md << "\n\n");
            }
            else {
                DEBUG(errs() << "NULL\n\n");
            }
            
            return md;
        }
        
        /// Starting at start, can we arrive at target?
        /// We may need to revisit this when loop support is added
        bool bbDFS(BasicBlock *start, BasicBlock *target, int indent = 0)
        {
            if (indent == 0) {
                DEBUG(errs() << "\nInverter: bbDFS(" << start->getName() << ", " << target->getName() << ")\n");
            }
            else {
                DEBUG(errs() << 2 * ' ' << "\nbbDFS(" << start->getName() << ", " << target->getName() << ")\n");
            }
            
            if (start == target) {
                return true;
            }
            
            for (succ_iterator SI = succ_begin(start), E = succ_end(start); SI != E; ++SI) {
                BasicBlock *Succ = *SI;
                DEBUG(errs() << "Inverter: succ of " << start->getName() << ": " << Succ->getName() << "\n");
                if (target == Succ) {
                    return true;
                }
                if (true == bbDFS(Succ, target)) {
                    return true;
                }
            }
            
            return false;
        }
        
        /// bb1 is a basic block which terminates with a branch instruction.
        /// find the "true" branch and then check if bb2 is a descendant
        /// We may need to revisit this when loop support is added
        bool isTrueAncestor(BasicBlock * ancestor, BasicBlock * descendant)
        {
            BasicBlock *dom = h->findDom(ancestor);
            
            // Now we have to find the BB for "true" in our br instruction
            if (BranchInst *b = dyn_cast<BranchInst>(dom->getTerminator())) {
                BasicBlock *then = b->getSuccessor(0);
                if (bbDFS(then, descendant)) {
                    return true;
                }
                else {
                    return false;
                }
            }
            else {
                assert(0 && "Expecting a BranchInst from dominator!");
            }
        }
        
        void visitSExtInst(SExtInst &I)
        {
            DEBUG(errs() << "Inverter: SExtInst\n");
            
            Value *v = builder.CreateSExt(lookup(I.getOperand(0)), I.getDestTy());
            
            oldToNew[&I] = v;
        }
		
        void visitGetElementPtrInst(GetElementPtrInst &I)
        {
            DEBUG(errs() << "Inverter: GetElementPtrInst\n");

			std::vector<Value *> bucket;
			//oldToNew.clear();

            Value *storeVal = I.getPointerOperand();
            
            if (Argument *a = dyn_cast<Argument>(storeVal)) {
                DEBUG(errs() << "Argument\n");
                // Pass arguments to I into our new GEP instruction
                std::vector<Value *> arr;
                arr.push_back(I.getOperand(1));
                arr.push_back(lookup(I.getOperand(2)));
                
                unsigned int i;
                Function::arg_iterator fi, fe, gi, ge;
                Function *f = I.getParent()->getParent();
                Function *g = M.getFunction(FuncToGenerate);

                for (fi = f->arg_begin(), fe = f->arg_end(), gi = g->arg_begin(), ge = g->arg_end(), i = 0;
                     fi != fe; ++fi, ++gi, ++i) {
                    if (a->getArgNo() == i) {
                        DEBUG(errs() << "Found our argument, replacing it with the new arg\n");
                        Value *v = gi;
                        lastVal = builder.CreateGEP(v, arr);
                        oldToNew[&I] = lastVal;
                        break;
                    }
                }
                
                return;
            }
			else if (isa<GlobalValue>(storeVal)) {
                DEBUG(errs() << "Inverter: " << storeVal->getName() << " is a global value\n");
				currently_reversing = true;
            }
            else {
                DEBUG(errs() << "Inverter: " << storeVal->getName() << " is not a global value\n");
				currently_reversing = false;
                storeVal = lookup(storeVal);
            }

			getUseDef(&I, bucket);
            
			DEBUG(errs() << "Inverter: " << I << "\n");
			DEBUG(errs() << "Inverter: Bucket contains:\n");
			for (std::vector<Value *>::iterator it = bucket.begin(), e = bucket.end();
				 it != e; ++it) {
				DEBUG(errs() << "Inverter: " << **it << "\n");
			}

			while (bucket.size()) {
				Value *v = bucket.back();
				bucket.pop_back();

                if (lookup(v) != v) {
                    continue;
                }
                
				if (Instruction *i = dyn_cast<Instruction>(v)) {
					visit(i);
				}
			}
            
            // Pass arguments to I into our new GEP instruction
            std::vector<Value *> arr;
            arr.push_back(I.getOperand(1));
            arr.push_back(lookup(I.getOperand(2)));
            lastVal = builder.CreateGEP(I.getOperand(0), arr);
            
            oldToNew[&I] = lastVal;
            
            return;
            
            lastVal = builder.getInt64(0);

			assert(lastVal && "lastVal not set!");

			//builder.CreateStore(lastVal, storeVal);
            lastVal = builder.CreateGEP(storeVal, lastVal);
            
            oldToNew[&I] = lastVal;

			lastVal = 0;
        }
        
		void visitTerminatorInst(TerminatorInst &I) {
			DEBUG(errs() << "\n\n\nInverter: TERMINATOR INSTRUCTION\n");
            DEBUG(errs() << "Inverter: For " << I.getParent()->getName() << "\n");
			
			int count = 0;
			
            std::vector<BasicBlock *> bbv;
            
            // bb is the original basic block to be reversed
			BasicBlock *bb = I.getParent();
            
            for (pred_iterator PI = pred_begin(bb), E = pred_end(bb); PI != E; ++PI) {
                BasicBlock *Pred = *PI;
                DEBUG(errs() << "Inverter: pred of " << bb->getName() << ": " << Pred->getName() << "\n");
                
                bbv.push_back(Pred);
                
                count++;
            }
            
            assert(count < 3 && "Basic Blocks must have < 3 predecessors!\n");
			
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
				Value *v = builder.CreateBr(newSucc);
                errs() << "single predecessor: " << *v << "\n";
				return;
			}
			
            Function *f = bb->getParent();
            LoopInfo *LI = h->getLoopInfo(*f);
            /// Standard ``if'' statement
			if (count == 2 && !(LI->getLoopDepth(bb))) {
                MDNode *md = findDiamondBf(I);
				
                DEBUG(errs() << "Inverter: " << md->getName() << " has " << md->getNumOperands() << " operands\n");
                ConstantInt *bfn = dyn_cast<ConstantInt>(md->getOperand(0));
                assert(bfn);
				/// We have two predecessors; we're going to need an "if"
				DEBUG(errs() << "Inverter: WE NEED A BRANCH\n");
				/// Emit a load of bf, compare, and jump to appropriate BBs
				
				/// CURRENTLY WORKING ON THIS
				/// Manually emit the load & cmp the bf
				/// Don't forget to tag all this JML!
                Type *newGlobal = IntegerType::get(I.getContext(), 32);
                Twine bfX("bf");
                bfX = bfX.concat(Twine(bfn->getZExtValue()));
                DEBUG(errs() << "Inverter: bfn->getZExtValue() returned " << bfn->getZExtValue() << "\n");
                DEBUG(errs() << "Inverter: and bfX is " << bfX << "\n");
				Value *l = M.getOrInsertGlobal(bfX.str(), newGlobal);
				//markJML(l);
				
				Value *ll = builder.CreateLoad(l);
				//markJML(ll);
				
				DEBUG(errs() << "Inverter: ll has type: " << *ll->getType() << "\n");
				DEBUG(errs() << "Inverter: I has type: " << *I.getType() << "\n");
				
				Value *lll = builder.CreateCast(Instruction::Trunc, ll, IntegerType::get(I.getContext(), 1));
				//markJML(lll);
				
				DEBUG(errs() << "Inverter: lll has type: " << *lll->getType() << "\n");
				
				Value *llll = builder.CreateICmpEQ(lll, builder.getInt1(true));
                
                /// Reset the bitfield
                Value *lllll = builder.CreateSub(ll, ll);
                /// Store back the bitfield
                Value *llllll = builder.CreateStore(lllll, l);
                
                if (isTrueAncestor(bb, bbv[0])) {
                    // Nothing is necessary, our bbv array is in correct order
                }
                else {
                    BasicBlock *temp = bbv[0];
                    bbv[0] = bbv[1];
                    bbv[1] = temp;
                }
				
				BasicBlock *thenBB = bbmOldToNew[bbv[0]], *elBB = bbmOldToNew[bbv[1]];
				
				assert(thenBB);
				assert(elBB);
				/// Create a branch instruction here that jumps to the bb names
				/// stored in the metadata
				builder.CreateCondBr(llll, thenBB, elBB);
				
                return;
			}
            
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
                //Twine bfX("bf");
                //bfX = bfX.concat(Twine(bfn->getZExtValue()));
                //DEBUG(errs() << "Inverter: bfn->getZExtValue() returned " << bfn->getZExtValue() << "\n");
                //DEBUG(errs() << "Inverter: and bfX is " << bfX << "\n");
				Value *l = M.getOrInsertGlobal(ctrNum->getString(), newGlobal);
                
                Value *ll = builder.CreateLoad(l);
                
                //Value *lll = builder.CreateSub(ll, builder.getInt32(1));
                
                //Value *llll = builder.CreateStore(lll, l);
                
                Value *lllll = builder.CreateICmpSGT(ll, builder.getInt32(0));
                
                /// 4. Depending on 3, loop or exit loop
                /// WATCH RIGHT HERE I'M GOING TO CHEAT
                
                //BasicBlock *newBody = bbmOldToNew[body->getBasicBlock()];
                BasicBlock *latch = LI->getLoopFor(bb)->getLoopLatch();
                BasicBlock *newBody = bbmOldToNew[latch];
                BasicBlock *newParent = bbmOldToNew[parent->getBasicBlock()];
                builder.CreateCondBr(lllll, newBody, newParent);

                return;
            }
			
			assert(0 && "Unhandled terminator instruction!\n");
		}
		
		void visitAllocaInst(AllocaInst &I) {
			DEBUG(errs() << "\n\n\nInverter: ALLOCA INSTRUCTION\n");

			Value *alloc = builder.CreateAlloca(I.getAllocatedType(), 0, I.getName());
			DEBUG(errs() << "Inverter: Allocating a " << *I.getType() << "\n");
			
			oldToNew[&I] = alloc;
		}
        
        void visitStoreInst(StoreInst &I) {
            DEBUG(errs() << "\n\n\nInverter: STORE INSTRUCTION\n");
			
			std::vector<Value *> bucket;
			//oldToNew.clear();
            
            getUseDef(&I, bucket);
			
			DEBUG(errs() << "Inverter: " << I << "\n");
			DEBUG(errs() << "Inverter: Bucket contains:\n");
			for (std::vector<Value *>::iterator it = bucket.begin(), e = bucket.end();
				 it != e; ++it) {
				DEBUG(errs() << "Inverter: " << **it << "\n");
			}
			
			while (bucket.size()) {
				Value *v = bucket.back();
				bucket.pop_back();
				
				if (Instruction *i = dyn_cast<Instruction>(v)) {
					visit(i);
				}
			}
            
            Value *storeVal = I.getPointerOperand();
			if (isa<GlobalValue>(storeVal) /*|| isa<GetElementPtrInst>(storeVal)*/) {
                DEBUG(errs() << "Inverter: " << storeVal->getName() << " is a global value\n");
				currently_reversing = true;
            }
            else {
                DEBUG(errs() << "Inverter: " << storeVal->getName() << " is not a global value\n");
				currently_reversing = false;
                storeVal = lookup(storeVal);
            }

            if (Constant *C = dyn_cast<Constant>(I.getValueOperand())) {
                errs() << "Assigning a Constant: " << *C << "\n";
                lastVal = C;
            }
			
			assert(lastVal && "lastVal not set!");
			
			builder.CreateStore(lastVal, storeVal);
			
			lastVal = 0;
		}
		
		void visitLoadInst(LoadInst &I) {
			DEBUG(errs() << "\n\n\nInverter: LOAD INSTRUCTION\n");
			
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
            DEBUG(errs() << "\n\n\nInverter: CALL INSTRUCTION\n");
            
            Function *fun = I.getCalledFunction();
            StringRef str = fun->getName();
            DEBUG(errs() << str << " called\n");
            
            if (str == "rng_gen_val" ||
                str == "tw_rand_integer" ||
                str == "tw_rand_exponential") {
                std::vector<Value *> bucket;
                //oldToNew.clear();
                
                getUseDef(&I, bucket);
                
                DEBUG(errs() << "Inverter: " << I << "\n");
                DEBUG(errs() << "Inverter: Bucket contains:\n");
                for (std::vector<Value *>::iterator it = bucket.begin(), e = bucket.end();
                     it != e; ++it) {
                    DEBUG(errs() << "Inverter: " << **it << "\n");
                }
                
                while (bucket.size()) {
                    Value *v = bucket.back();
                    bucket.pop_back();
                    
                    if (Instruction *i = dyn_cast<Instruction>(v)) {
                        visit(i);
                    }
                }
                
                errs() << "We need to reverse the RNG\n";
                // Fortunately, all the rng functions pass the tw_rng_stream as their first param
                Value *G = I.getArgOperand(0);
                Function *reverse_rng = M.getFunction("rng_gen_reverse_val");
                assert(reverse_rng && "rng_gen_reverse_val not found!");
                builder.CreateCall(reverse_rng, lookup(G));
            }
        }
		
		void visitBinaryOperator(BinaryOperator &I) {
			Value *newInstruction;
			DEBUG(errs() << "\n\n\nBINARY OPERATOR\n");
            
			if (I.getOpcode() == Instruction::Add) {
				DEBUG(errs() << "ADD INSTRUCTION\n");
                //DEBUG(errs() << "lastVal: " << *lastVal << "\n");
                DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
				//lastVal = builder.CreateSub(lastVal, I.getOperand(1));
				newInstruction = builder.CreateSub(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
			}
			
			if (I.getOpcode() == Instruction::Sub) {
				DEBUG(errs() << "SUB INSTRUCTION\n");
                //DEBUG(errs() << "lastVal: " << *lastVal << "\n");
                DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
				//lastVal = builder.CreateAdd(lastVal, I.getOperand(1));
				newInstruction = builder.CreateAdd(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
			}
			
			if (I.getOpcode() == Instruction::Mul) {
				DEBUG(errs() << "MULT INSTRUCTION\n");
                //DEBUG(errs() << "lastVal: " << *lastVal << "\n");
                DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
				//lastVal = builder.CreateSDiv(lastVal, I.getOperand(1));
                //lastVal = builder.CreateSDiv(I.getOperand(0), I.getOperand(1));
				newInstruction = builder.CreateSDiv(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
			}
			
			oldToNew[&I] = newInstruction;
			lastVal = newInstruction;
		}
		
		/// Collect all use-defs into a container
		void getUseDef(User *I, std::vector<Value *> &bucket, int indent = 0) {
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
					
					/// Don't store alloca's if that's a dependent!!!
					if (!isa<AllocaInst>(w)) {
						bucket.push_back(w);
					}
					//bucket.push_back(w);
					getUseDef(w, bucket, indent + 1);
					DEBUG(errs() << std::string(2*indent, ' '));
                    DEBUG(errs() << "bucket now has " << bucket.size() << " elements\n");
				}
                else {
					DEBUG(errs() << std::string(2*indent, ' '));
                    DEBUG(errs() << "This is a " << *v->getType() << "\n");
                }
				
				
				/*
				 else if (Operator *w = dyn_cast<Operator>(v)) {
				 getUseDef(w, indent + 1);
				 } else if (Constant *w = dyn_cast<Constant>(v)) {
				 getUseDef(w, indent + 1);
				 }
				 */
			}
			DEBUG(errs() << std::string(2*indent, ' '));
            DEBUG(errs() << "This instruction has " << uses << " uses\n\n");
		}
    };
    
    class Swapper {
        
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
    
    BasicBlock * Hello::findDom(BasicBlock *BB)
    {
        Function *f = BB->getParent();
        DominatorTree &DT = getAnalysis<DominatorTree>(*f);
        DomTreeNode *Rung = DT.getNode(BB);
        Rung = Rung->getIDom();
        //errs() << "Rung is " << Rung << "\n";
        BasicBlock *bb = Rung->getBlock();
        //errs() << "BB is " << *bb << "\n";
        return bb;
    }
    
    
    /// Lookup our IDom (from a merge point) so we can find the correct
    /// metadata to tell us where to go (BBs) and what to check (bitfields)
    MDNode * Hello::domTreeLookup(Instruction &I)
    {
        Function *f = I.getParent()->getParent();
        DominatorTree &DT = getAnalysis<DominatorTree>(*f);
        DomTreeNode *Rung = DT.getNode(I.getParent());
        Rung = Rung->getIDom();
        errs() << "Rung is " << Rung << "\n";
        BasicBlock *bb = Rung->getBlock();
        errs() << "BB is " << bb->getName() << "\n";
        
        CmpInst *C = 0;
        
        // find the cmp
        for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i) {
            if ((C = dyn_cast<CmpInst>(i))) {
                errs() << *C << "\n";
                break;
            }
        }
        
        DEBUG(I.getParent()->getParent()->dump());
        
        assert(C && "CmpInst not found!");
        
        MDNode *md = C->getMetadata("jml.icmp");
        
        assert(md && "No metadata found on CmpInst!");
        
        return md;
    }
    
    BasicBlock * Hello::postdomTreeLookup(BasicBlock *BB)
    {
        Function *f = BB->getParent();
        PostDominatorTree &DT = getAnalysis<PostDominatorTree>(*f);
        DomTreeNode *Rung = DT.getNode(BB);
        Rung = Rung->getIDom();
        DEBUG(errs() << "PD Rung is " << Rung << "\n");
        BasicBlock *bb = Rung->getBlock();
        DEBUG(errs() << "PD BB is " << *bb << "\n");
        return bb;
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
            for (inst_iterator I = inst_begin(ForwardFunc), E = inst_end(ForwardFunc); I != E; ++I)
                instrumenter.visit(&*I);
            
            DEBUG(errs() << "Found " << FuncToInstrument << "\n");
            
            Function *target = createReverseFunction(M);
            
            target->deleteBody();
            
            /// Put exit BB first in function
            BasicBlock *bb = findExitBlock(*ForwardFunc);
            BasicBlock *newBlock = BasicBlock::Create(getGlobalContext(),
                                                      "return_rev", target);
            bbmOldToNew[bb] = newBlock;
            bbmNewToOld[newBlock] = bb;
            
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
                    if (j->getMetadata("jml")) {
                        continue;
                    }
                    
                    if (AllocaInst *a = dyn_cast<AllocaInst>(j)) {
                        inv.visitAllocaInst(*a);
                    }
                    
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
                        
                        stores.push(b);
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
