#include<iostream>
#include<string>
#include <bits/stdc++.h> 
#include<malloc.h>
using namespace std;

#include "analysis/walker.h"
#include "instr/concrete.h"
#include "chunk/dump.h"
#include "disasm/dump.h"
#include "disasm/handle.h"
#include "syspartUtility.h"
#include "chunk/ictarget.h"
#include "elf/elfspace.h"

#include<climits> //For INT_MIN

#undef DEBUG_GROUP
#define DEBUG_GROUP syspartutil
#define D_syspartutil 20

#include "log/log.h"

vector<UDState*> SyspartUtility::matchLoadMemoryToStores(Function* func, UDState* orig, set<UDState*> storeNodes)
{
        vector<UDState*> result;
        auto orig_instr = orig->getInstruction();
        auto workingSet = df.getWorkingSet(func);
        auto cfg = workingSet->getCFG();

        Block* orig_block = NULL;
        for(auto block : CIter::children(func))
        {
            for(auto instr : CIter::children(block))
            {
                    if(instr->getAddress() == orig_instr->getAddress())
                    {
                        orig_block = block;
                    }
            }
        }

        stack<Block*> st;
        st.push(orig_block);
        set<Block*> already_pushed;
        already_pushed.insert(orig_block);
        bool foundFlag = false;
        while(!st.empty())
        {
                foundFlag = false;
                auto cur_block = st.top();
                auto node_id = cfg->getIDFor(cur_block);
                auto node = cfg->get(node_id);
                st.pop();
                vector<Instruction*> reversed;
                for(auto instr : CIter::children(cur_block))
                {
                        reversed.push_back(instr);
                }
                for(auto it = reversed.rbegin(); it != reversed.rend(); ++it)
                {
                        auto instr = *it;
                        auto instr_state = workingSet->getState(instr);
                        if(storeNodes.find(instr_state) != storeNodes.end())
                        {       foundFlag = true;
                                //LOG(1,"Found store instruction "<<std::hex<<instr->getAddress());
                                result.push_back(instr_state);
                                break;
                        }

                }
                if(foundFlag)
                {
                        //LOG(1, "NOT ADDING NEIGHBORS OF "<<cur_block->getName());
                        continue;
                }
                for(auto link : node->backwardLinks())
                {
                        auto neighbor_block = cfg->get(link->getTargetID())->getBlock();
                        if(already_pushed.find(neighbor_block) == already_pushed.end())
                        {
                                st.push(neighbor_block);
                                already_pushed.insert(neighbor_block);
                                //LOG(1,"ADD BLOCK "<<cur_block->getName()<<" -> "<<neighbor_block->getName());
                        }
                }

        }
        return result;
}


void SyspartUtility::getArgumentsPassedToFunction(Function* func, int reg, std::unordered_set<UDResult>& collectedResults)
{
    if(recursion_iter > MAX_ITER)
    {
        UDResult unknownRes{0, 0, "unknown", func};
        collectedResults.insert(unknownRes);
        return;
    }
    recursion_iter++;
    LOG(1, "Iter "<<std::dec<<recursion_iter<<" getArgumentsTo "<<func->getName()<<" at reg "<<reg);
    
    // Check cache for found results
    Inter_result searchKey{func->getName(), func->getAddress(), reg, {}};
    auto foundIt = found_results.find(searchKey);
    if (foundIt != found_results.end()) {
        LOG(1, "ICALL_RESOLVE RESULT_EXISTS " << std::dec << reg << " " << func->getName());
        collectedResults.insert((foundIt->res).begin(), (foundIt->res).end());
        recursion_iter--;
        return;
    }


    getArgumentsPassedToFunction_helper(func, reg, collectedResults);

    found_results.emplace(func->getName(), func->getAddress(), reg, collectedResults);
    recursion_iter--;
    LOG(1,"Iter "<<std::dec<<recursion_iter<<" Returning from getArguments "<<func->getName()<<" "<<std::dec<<reg);

}

void SyspartUtility::getArgumentsPassedToFunction_helper(Function* func, int reg, std::unordered_set<UDResult>& collectedResults)
{
    if(recursion_iter > MAX_ITER)
    {
            UDResult unknownRes{0, 0, "unknown", func};
            collectedResults.insert(unknownRes);
            return;
    }

    recursion_iter++;
    
    LOG(1, "Iter "<<std::dec<<recursion_iter<<" Helper : Get args to func " << func->getName() << " for register " << reg);

    // Helpers
    auto pushUnknownIfNeeded = [&](std::unordered_set<UDResult>& resVec, Function* f) {
        LOG(1,"Adding unknown "<<f->getName());
        UDResult unknownRes{0, 0, "unknown", f};
        resVec.insert(unknownRes);
    };

    auto isVisited = [&](Function* f, int r) {
        auto tup = std::make_tuple(f->getName(), f->getAddress(), r);
        return std::find(visitedFuncRegs.begin(), visitedFuncRegs.end(), tup) != visitedFuncRegs.end();
    };

    auto markVisited = [&](Function* f, int r) {
        visitedFuncRegs.emplace_back(f->getName(), f->getAddress(), r);
    };

    auto unmarkVisited = [&](Function* f, int r) {
        auto tup = std::make_tuple(f->getName(), f->getAddress(), r);
        auto it = std::find(visitedFuncRegs.begin(), visitedFuncRegs.end(), tup);
        if (it != visitedFuncRegs.end())
            visitedFuncRegs.erase(it);
    };

    // Early exit if func is in global AT list
    auto globalATList = ip_callgraph->getGlobalATList();
    if (globalATList.count(func) != 0) {
        pushUnknownIfNeeded(collectedResults, func);
        LOG(1, "ICALL_RESOLVE AT_FUNC " << std::dec << reg << " " << func->getName());
        recursion_iter--;
        return;
    }

    if (isVisited(func, reg)) {
        pushUnknownIfNeeded(collectedResults, func);
        LOG(1, "ICALL_RESOLVE VISITED_FUNC_REG " << std::dec << reg << " " << func->getName());
        recursion_iter--;
        return;
    }


    markVisited(func, reg);

    bool isCalled = false;
    auto curNode = ip_callgraph->getNode(func);
    if (!curNode) {
        LOG(1, "ICALL_RESOLVE NO_IPNODE " << std::dec << reg << " " << func->getName());
        unmarkVisited(func, reg);
        pushUnknownIfNeeded(collectedResults, func);
        recursion_iter--;
        return;
    }

    auto parentTypes = curNode->getParentWithType();
    vector<UDResult> alreadyPrinted;  // For printing results only once
    
    for (auto& pt : parentTypes) {
    
        address_t callingAddr;
        IPCallGraphNode* parent;
        bool type;
        std::tie(callingAddr, parent) = pt.first;
        type = pt.second;

        if (!type && !parent->isIcallResolved(callingAddr)) {
            LOG(1, "Unresolved icall of parent " << parent->getFunction()->getName() << " @ " << std::hex << callingAddr);
            continue;
        }

        auto callingFn = parent->getFunction();

        auto working = df.getWorkingSet(callingFn);
        std::vector<Function*> fp_vec;

        if (analysisType != 0) {
            FPath fp{cur_function->getName(), cur_instr->getAddress(), fp_vec};
            auto icPath_iter = std::find(icPath.begin(), icPath.end(), fp);
            if (icPath_iter != icPath.end()) {
                auto& path_vec = (*icPath_iter).path;
                if (std::find(path_vec.begin(), path_vec.end(), callingFn) == path_vec.end()) {
                    path_vec.push_back(callingFn);
                }
            } else {
                fp_vec.push_back(callingFn);
                icPath.push_back(fp);
            }
        }

        for (auto block : CIter::children(callingFn)) {
            for (auto instr : CIter::children(block)) {
                if (instr->getAddress() == callingAddr) {
                    isCalled = true;
                    auto state = working->getState(instr);
                    if(analysisType == 0)
                    {
                        cout<<"FUNC "<<func->getName()<<" CALLINGFN "<<callingFn->getName()<<" CALLINGADDR "<<std::hex<<callingAddr<<endl;
                    }

                    LOG(1, "----------------");

                    bool refFlag = false;
                    for (auto& s : state->getRegRef(reg)) {
                        refFlag = true;
                        LOG(1, "Withing getArgs - invoking findRegDef for " << callingFn->getName() << " at reg " << std::dec << reg);
                        std::unordered_set<UDResult> inter_result; 
                        findRegDef(callingFn, s, reg, inter_result);
                        collectedResults.insert(inter_result.begin(), inter_result.end());
                    }
                   

                    if (!refFlag) {
                        if (reg == 7 || reg == 6 || reg == 2 || reg == 1 || reg == 8 || reg == 9) 
                        {  // Argument registers
                            LOG(1, "Within getArgs - Finding arguments passed to " << callingFn->getName() << " at reg ");
                            getArgumentsPassedToFunction(callingFn, reg, collectedResults);

                        } 
                        else 
                        {
                            LOG(1, "Within getArgs - No refFlag and not arguments");
                            pushUnknownIfNeeded(collectedResults, func);
                        }
                    }

                    if (analysisType == 0) {
                        for (auto& tr : collectedResults) {
                            if (std::find(alreadyPrinted.begin(), alreadyPrinted.end(), tr) == alreadyPrinted.end()) {
                                alreadyPrinted.push_back(tr);
                                auto mod = static_cast<Module*>(tr.func->getParent()->getParent());
                                std::cout << "TYPE " << tr.type << "\tADDRESS " << std::hex << tr.addr << " DESC " << tr.desc
                                          << " FUNC " << tr.func->getName() << " MODULE " << mod->getName();

                                if (tr.type == 0 || tr.type == 3) {
                                    std::cout << " UNKNOWN" << std::endl;
                                    continue;
                                }

                                if (tr.desc == "0x0") {
                                    std::cout << " SYM NULL" << std::endl;
                                    continue;
                                }

                                ElfMap* elf = mod->getElfSpace()->getElfMap();
                                auto section = elf->findSection(".rodata");
                                auto rodata = elf->getSectionReadPtr<char*>(".rodata");
                                auto vaddr = section->getVirtualAddress();
                                auto size = section->getSize();
                                auto offset = section->convertVAToOffset(tr.addr);
                                auto section_end = vaddr + size;

                                if (tr.addr >= vaddr && tr.addr <= section_end) {
                                    char* value = rodata + offset;
                                    std::cout << " SYM " << value << std::endl;
                                }
                            }
                        }
                        std::cout << " END" << std::endl;
                    }
                }
            }
        }
    }

    if (!isCalled) {
        LOG(1, "ICALL_RESOLVE FUNC_NOT_INVOKED " << std::dec << reg << " " << func->getName());
        pushUnknownIfNeeded(collectedResults, func);
    }

    unmarkVisited(func, reg);
    LOG(1,"Iter "<<recursion_iter<<" Returning from getArgsHelper");
    recursion_iter--;
    return;
}

void SyspartUtility::findRegDef(Function* func, UDState *state, int reg, std::unordered_set<UDResult>& result)
{
    if(recursion_iter > MAX_ITER)
    {
            UDResult unknownRes{0, 0, "unknown", func};
            result.insert(unknownRes);
            return;
    }

    recursion_iter++;

    LOG(1, "Iter "<<std::dec<<recursion_iter<<" Find register definition of "<<reg<<" at instruction "<<std::hex<<state->getInstruction()->getAddress()<<" in function "<<func->getName());


    // 1. Check the cache first. If a result exists, return it immediately.
    Intra_result ir_key{func->getName(), func->getAddress(), state->getInstruction()->getAddress(), reg, {}};
    auto found_it = found_state_results.find(ir_key);
    if(found_it != found_state_results.end()) 
    {
        LOG(1, "Found results for "<<func->getName()<<" "<<reg);
        result.insert(found_it->res.begin(), found_it->res.end());
        recursion_iter--;
        return;
    }


    // 2. Call recursive helper
    findRegDef_helper(func, state, reg, result);

    // 3. Cache the final result
    found_state_results.emplace(func->getName(), func->getAddress(), state->getInstruction()->getAddress(), reg, result);

    LOG(1, "Iter "<<std::dec<<recursion_iter<<" Returning from findRegDef "<<func->getName()<<" at reg "<<reg);
    recursion_iter--;
    
}

void SyspartUtility::findRegDef_helper(Function* func, UDState *state, int reg, std::unordered_set<UDResult>& result)
{
    if(recursion_iter > MAX_ITER)
    {
            UDResult unknownRes{0, 0, "unknown", func};
            result.insert(unknownRes);
            return;
    }

    recursion_iter++;

    if(state->getInstruction() == NULL)
    {
        LOG(1, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! NULL !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        recursion_iter--;
        return;
    }
    LOG(1, "Iter "<<std::dec<<recursion_iter<<" Find register definition of "<<reg<<" at instruction "<<std::hex<<state->getInstruction()->getAddress()<<" in function "<<func->getName());
    
    auto instr = state->getInstruction();
    
    //Already visited?

    tuple<Instruction*, int> visited_tup{instr, reg};
    if(find(visitedStates.begin(),visitedStates.end(),visited_tup) != visitedStates.end())
    {
        LOG(1, "Already visited state."<<func->getName()<<" "<<reg<<" "<<std::hex<<instr->getAddress()); 
        recursion_iter--;
        return;
    }
    visitedStates.push_back(visited_tup);
    
    // Patterns

      //R0:  (deref (+ %R5 -12)) nodeType=1
    typedef TreePatternUnary<TreeNodeDereference,
                TreePatternCapture<TreePatternBinary<TreeNodeAddition,
                    TreePatternTerminal<TreeNodePhysicalRegister>,
                    TreePatternTerminal<TreeNodeConstant>>>>MemoryAddress1;

    //R0:  (deref (- %R5 -12)) nodeType=2
    typedef TreePatternUnary<TreeNodeDereference,
                TreePatternCapture<TreePatternBinary<TreeNodeSubtraction,
                    TreePatternTerminal<TreeNodePhysicalRegister>,
                    TreePatternTerminal<TreeNodeConstant>>>>MemoryAddress2;

    //R2:  (+ %R5 20) nodeType=3
    typedef
            TreePatternBinary<TreeNodeAddition,
                TreePatternCapture<TreePatternTerminal<TreeNodePhysicalRegister>>,
                TreePatternCapture<TreePatternTerminal<TreeNodeConstant>>> MemoryAddress3;
    //R4:  (- %R4 48) nodeType=4
    typedef
            TreePatternBinary<TreeNodeSubtraction,
                TreePatternCapture<TreePatternTerminal<TreeNodePhysicalRegister>>,
                TreePatternCapture<TreePatternTerminal<TreeNodeConstant>>> MemoryAddress4;

    //R0:  (deref %R5) nodeType=9
            typedef TreePatternUnary<TreeNodeDereference,
                                    TreePatternCapture<TreePatternTerminal<TreeNodePhysicalRegister>>> MemoryAddress5;
    //R0:  0 nodeType=5
            typedef TreePatternCapture<TreePatternTerminal<TreeNodeConstant>>ConstantValue;

    //R6:  %R0 nodeType=6
            typedef TreePatternCapture<TreePatternTerminal<TreeNodePhysicalRegister>>RegisterValue;

    //R0:  (+ %rip=0x749 -101) nodeType=7
            typedef TreePatternBinary<TreeNodeAddition,
                        TreePatternCapture<TreePatternRegisterIs<X86_REG_RIP>>,
                        TreePatternCapture<TreePatternTerminal<TreeNodeConstant>>
                    > RIPValue;
    //R0:  (deref (+ %rip=0x749 -101)) nodeType=8
            typedef TreePatternUnary<TreeNodeDereference,
                    TreePatternCapture<TreePatternBinary<TreeNodeAddition,
                            TreePatternCapture<TreePatternRegisterIs<X86_REG_RIP>>,
                            TreePatternCapture<TreePatternTerminal<TreeNodeConstant>>
                    >>> RIPDerefValue;



    //rsi:  (+ %rsi %rax) nodeType=10
            typedef
            TreePatternBinary<TreeNodeAddition,
                TreePatternCapture<TreePatternTerminal<TreeNodePhysicalRegister>>,
                TreePatternCapture<TreePatternTerminal<TreeNodePhysicalRegister>>> RegisterSum;

    //rsi:  (- %rsi %rax) nodeType=11
            typedef
            TreePatternBinary<TreeNodeSubtraction,
                TreePatternCapture<TreePatternTerminal<TreeNodePhysicalRegister>>,
                TreePatternCapture<TreePatternTerminal<TreeNodePhysicalRegister>>> RegisterDifference;



    //Define helper functions

    auto pushUnknownIfNeeded = [&](std::unordered_set<UDResult>& results) {
            UDResult unknownRes{0, 0, "unknown", func};
            results.insert(unknownRes);
    };

    auto isArgRegister = [](int reg) {
            return reg == 7 || reg == 6 || reg == 2 || reg == 1 || reg == 8 || reg == 9;
        };

    auto handleRIPValue = [&](TreeCapture& cap) {
        LOG(1, "Matches RIPValue");
        auto ripTree = dynamic_cast<TreeNodeRegisterRIP *>(cap.get(0));
        auto dispTree = dynamic_cast<TreeNodeConstant *>(cap.get(1));
        if (!ripTree || !dispTree)
        {
            LOG(1,"riptree and disptree RIPValue NULL");
            pushUnknownIfNeeded(result);
        }
        else
        {
            address_t new_val = dispTree->getValue() + ripTree->getValue();
            std::stringstream sstream;
            sstream << "0x" << std::hex << new_val;
            std::string new_str = sstream.str();
            result.emplace(1, new_val, new_str, func);
        }
    };

    auto handleRIPDerefValue = [&](TreeCapture& cap) {
        LOG(1, "Matches RIPDerefValue");
        TreeCapture cap1;
        if (RIPValue::matches(cap.get(0), cap1)) {
            auto ripTree = dynamic_cast<TreeNodeRegisterRIP *>(cap1.get(0));
            auto dispTree = dynamic_cast<TreeNodeConstant *>(cap1.get(1));
            if (!ripTree || !dispTree)
            {
                LOG(1,"riptree and disptree RIPDerefValue NULL");
                pushUnknownIfNeeded(result);
            }
            else
            {
                address_t new_val = dispTree->getValue() + ripTree->getValue();
                std::stringstream sstream;
                sstream << "val(0x" << std::hex << new_val << ")";
                std::string new_str = sstream.str();
                result.emplace(2, new_val, new_str, func);
            }
        }
    };

    auto resolveRegister = [&](int reg, UDState* state, Function* func, std::unordered_set<UDResult>& out) {
        bool refFlag = false;
        for (auto& rf : state->getRegRef(reg)) {
            refFlag = true;
            LOG(1, "findRegDefHelper invoking findRegDef for "<<func->getName()<<" reg ");
            findRegDef(func, rf, reg, out);
        }           
        if (!refFlag && isArgRegister(reg)) {
            LOG(1, "findRegDefHeloper invoking Check argument registers for reg " << reg<<" function "<<func->getName());
            std::unordered_set<UDResult> inter_result; 
            getArgumentsPassedToFunction(func, reg, inter_result);  // must also return std::vector<UDResult
            out.insert(inter_result.begin(), inter_result.end());                                         
            refFlag = true;
        }           
    };

    enum class BinaryOpType { ADD, SUB };
    auto combineResults = [&](const std::unordered_set<UDResult>& a,
                            const std::unordered_set<UDResult>& b,
                            BinaryOpType op,
                std::unordered_set<UDResult>& out) 
    {
        LOG(1, "Res1.size "<<a.size()<< " Res2.size "<<b.size());
        if(op == BinaryOpType::ADD)
        {
            LOG(1, "ADD");
        }
        else if(op == BinaryOpType::SUB)
        {
            LOG(1, "SUB");
        }
        for (const auto& res1 : a) 
        {
            for (const auto& res2 : b) 
            {
                if (res1.type == 0 || res2.type == 0) 
                {
                    pushUnknownIfNeeded(out);

                } 
                else if (res1.type == 1 && res2.type == 1) 
                {
                    address_t new_val = (op == BinaryOpType::ADD)
                        ? res1.addr + res2.addr
                        : res1.addr - res2.addr;        
                    std::stringstream sstream;
                    sstream << "0x" << std::hex << new_val;
                    std::string new_str = sstream.str();
                    out.emplace(1, new_val, new_str, func);
                } 
                else 
                {
                    std::string op_str = (op == BinaryOpType::ADD) ? " + " : " - ";
                    std::string new_str = res1.desc + op_str + res2.desc;
                    out.emplace(3, 0, new_str, func);
                }
            }
        }
    };

    enum class ConstOpType { ADD, SUB };
    auto combineWithConstant = [&](const std::unordered_set<UDResult>& vals,
                              address_t constant,
                              ConstOpType op,
                              std::unordered_set<UDResult>& out) 
    {
        LOG(1, "Res1.size "<<vals.size()<<std::hex<<constant);
        if(op == ConstOpType::ADD)
        {
                LOG(1, "ADD");
        }
        else if(op == ConstOpType::SUB)
        {
                LOG(1, "SUB");
        }

        for (const auto& udres : vals) 
        {
            if (udres.type == 0) 
            {
                LOG(1,"Combine with constant");
                pushUnknownIfNeeded(out);
            } 
            else if (udres.type == 1) 
            {
                address_t new_val = (op == ConstOpType::ADD)
                    ? udres.addr + constant
                    : udres.addr - constant;
                std::stringstream sstream;
                sstream << "0x" << std::hex << new_val;
                out.emplace(1, new_val, sstream.str(), func);
            } 
            else 
            {
                std::stringstream sstream;
                if (op == ConstOpType::ADD) 
                {
                    sstream <<  std::hex << udres.desc << " + 0x" << constant;
                } 
                else 
                {
                    sstream << udres.desc << " - 0x" << std::hex << constant;
                }
                out.emplace(3, 0, sstream.str(), func);
            }
        }
    };

    auto resolveMemRefsForState = [&](Function* func, UDState* memState, const MemLocation& loadLoc, std::unordered_set<UDResult>& out) {
        bool foundMatch = false;
        for (auto& mem : memState->getMemDefList()) 
        {
            MemLocation storeLoc(mem.second);
            if (loadLoc == storeLoc) 
            {
                foundMatch = true;
                bool refFlag = false;
                        
                if(!memState->getRegRef(mem.first).empty())
                {
                    refFlag = true;
                    LOG(1, "resolveMemRefs invoking resolveRegister");
                    resolveRegister(mem.first, memState, func, out);
                    if (out.empty()) 
                    {
                        LOG(1, "mem ref empty");
                        pushUnknownIfNeeded(out);
                    } 
                }
                if (!refFlag) 
                {
                    if (isArgRegister(mem.first)) 
                    {
                        std::unordered_set<UDResult> inter_result;
                        LOG(1, "resolveMemRefs invoking getArguments");
                        getArgumentsPassedToFunction(func, mem.first,inter_result);
                        out.insert(inter_result.begin(), inter_result.end());
                    } 
                    else 
                    {                    
                        LOG(1,"No refFlag and no args");
                        pushUnknownIfNeeded(out);
                    }
                }
            }
        }
        if (!foundMatch) 
        {
            LOG(1,"No matching memory definition");
            pushUnknownIfNeeded(out);
        }
    };

    //Main logic
    if(auto def = state->getRegDef(reg))    //If there is a register definition for reg in this instruction
    {
        TreeCapture cap;
        if (RIPValue::matches(def, cap)) 
        {
            handleRIPValue(cap);
        } 
        else if (RIPDerefValue::matches(def, cap)) 
        {
            handleRIPDerefValue(cap);
        }
        else if (RegisterSum::matches(def, cap)) 
        {
            LOG(1, "Register sum");
        
            auto reg1 = dynamic_cast<TreeNodePhysicalRegister*>(cap.get(0))->getRegister();
            auto reg2 = dynamic_cast<TreeNodePhysicalRegister*>(cap.get(1))->getRegister();
        
            std::unordered_set<UDResult> inter_res1;
            std::unordered_set<UDResult> inter_res2;

            resolveRegister(reg1, state, func, inter_res1);
            resolveRegister(reg2, state, func, inter_res2);
        
            if (inter_res1.empty() && inter_res2.empty()) 
            {
            
                LOG(1, "DFD ? (NO REGREF) FUNC : " << func->getName()
                                                << std::hex << " " << instr->getAddress()
                                                << " " << std::dec << reg1 << " " << reg2);
                pushUnknownIfNeeded(result);

            } 
            else 
            {
            
                LOG(1, "Sum of registers");
                combineResults(inter_res1, inter_res2, BinaryOpType::ADD, result);
            }
        }
        else if (RegisterDifference::matches(def, cap)) 
        {   
            LOG(1, "Register Difference");
        
            auto reg1 = dynamic_cast<TreeNodePhysicalRegister*>(cap.get(0))->getRegister();
            auto reg2 = dynamic_cast<TreeNodePhysicalRegister*>(cap.get(1))->getRegister();
        
            std::unordered_set<UDResult> inter_res1;
            std::unordered_set<UDResult> inter_res2;

            resolveRegister(reg1, state, func, inter_res1);
            resolveRegister(reg2, state, func, inter_res2);
        
            if (inter_res1.empty() && inter_res2.empty()) 
            {
            
                LOG(1, "DFD ? (NO REGREF) FUNC: " << func->getName()
                                                << " " << std::hex << instr->getAddress()
                                                << " " << std::dec << reg1 << " " << reg2);
                pushUnknownIfNeeded(result);

            } 
            else 
            {
                LOG(1, "Diff of results (nested loop)");
                combineResults(inter_res1, inter_res2, BinaryOpType::SUB, result);
            }
        }
        else if (RegisterValue::matches(def, cap)) 
        {
            
            LOG(1, "Matches register value");
            auto reg1 = dynamic_cast<TreeNodePhysicalRegister *>(cap.get(0))->getRegister();
            resolveRegister(reg1, state, func, result);
        
            if (result.empty()) 
            {
                LOG(1,"No ref found and not an argument register");
                pushUnknownIfNeeded(result);
            }
        }
        else if (ConstantValue::matches(def, cap)) 
        {
        
            LOG(1, "Matches constant value");
            auto const_node = dynamic_cast<TreeNodeConstant*>(cap.get(0));
        
            if(!const_node)
            {
                LOG(1, "Unknown CONST NODE");
                pushUnknownIfNeeded(result);
            }   
            else
            {
                address_t const_val = const_node->getValue();
                std::stringstream sstream;
                sstream << "0x" << std::hex << const_val;
                std::string new_str = sstream.str();     
                result.emplace(1, const_val, new_str, func);
            }
        }
        else if (MemoryAddress3::matches(def, cap)) 
        {
            
            LOG(1, "Matches MemoryAddress3");
        
            auto reg1 = dynamic_cast<TreeNodePhysicalRegister*>(cap.get(0))->getRegister();
            auto const_node = dynamic_cast<TreeNodeConstant*>(cap.get(1));

            if(!const_node)
            {
                LOG(1, "Unknown CONST NODE");
                pushUnknownIfNeeded(result);
            }
            else
            {
                address_t const_val = const_node->getValue();
                std::unordered_set<UDResult> inter_result;
                resolveRegister(reg1, state, func, inter_result);
        
                if (inter_result.empty()) 
                {
                    LOG(1, "CAUTION UNKNOWN MEMDEF @ " << std::hex << state->getInstruction()->getAddress());
                    pushUnknownIfNeeded(result);

                } 
                else 
                {
                    LOG(1, "Resolved value found");
                    combineWithConstant(inter_result, const_val, ConstOpType::ADD, result);
                }
        }
        }
        else if (MemoryAddress4::matches(def, cap)) 
        {
            LOG(1, "Matches MemoryAddress4");
            auto reg1 = dynamic_cast<TreeNodePhysicalRegister*>(cap.get(0))->getRegister();
            auto const_node = dynamic_cast<TreeNodeConstant*>(cap.get(1));

            if(!const_node)
            {
                LOG(1, "Unknown CONST NODE");
                pushUnknownIfNeeded(result);
            }
            else
            {
                address_t const_val = const_node->getValue();
                std::unordered_set<UDResult> inter_result;
                resolveRegister(reg1, state, func, inter_result);
        
                if (inter_result.empty()) 
                {
                    LOG(1, "CAUTION UNKNOWN MEMDEF @ " << std::hex << state->getInstruction()->getAddress());
                    pushUnknownIfNeeded(result);
                } 
                else 
                {
                    LOG(1, "Resolved value found");
                    combineWithConstant(inter_result, const_val, ConstOpType::SUB, result);
                }
            }
        }
        else if (MemoryAddress1::matches(def, cap) || MemoryAddress2::matches(def, cap) || MemoryAddress5::matches(def, cap)) 
        {
        
            LOG(1, "Matches memoryaddress 1,2,5");
            MemLocation loadLoc(cap.get(0));
            std::set<UDState*> storeNodes;
            for (auto& memState : state->getMemRef(reg)) 
            {
                for (auto& mem : memState->getMemDefList()) 
                {
                    MemLocation storeLoc(mem.second);
                    if (loadLoc == storeLoc) 
                    {

                        storeNodes.insert(memState);
                        LOG(1, "Adding store state " << std::hex << memState->getInstruction()->getAddress());
                    }
                }
            }

            std::vector<UDState*> matchingStores;
            if (!storeNodes.empty()) 
            {
            
                LOG(1, "Finding matching stores for load at " << func->getName() << " @ " << std::hex << state->getInstruction()->getAddress());
                matchingStores = matchLoadMemoryToStores(func, state, storeNodes);
            }

            bool memFlag = false;
            for (auto& ms : state->getMemRef(reg)) 
            {
                if (std::find(matchingStores.begin(), matchingStores.end(), ms) == matchingStores.end()) 
                {
            
                    LOG(1, "##### NOT a matching store instruction");
                    continue;
                }
            
                LOG(1, "Finding memory references @ " << std::hex << ms->getInstruction()->getAddress());
                memFlag = true;
                resolveMemRefsForState(func, ms, loadLoc, result);
            }
            if (!memFlag) 
            {
            LOG(1, "CAUTION UNKNOWN MEMREF @ " << std::hex << state->getInstruction()->getAddress());
                        
            auto regTree = loadLoc.getRegTree();
            auto offset = loadLoc.getOffset();
            if(auto regNode = dynamic_cast<TreeNodePhysicalRegister*>(regTree))
            {
                auto memReg = regNode->getRegister();
                std::unordered_set<UDResult> inter_result;
                                resolveRegister(memReg, state, func, inter_result);
                if(inter_result.empty())
                {
                    LOG(1, "CAUTION UNKNOWN MEMREF @ " << std::hex << state->getInstruction()->getAddress());
                    pushUnknownIfNeeded(result);
                }
                else
                {
                    for(auto res : inter_result)
                    {
                        if(res.type == 0)
                        {
                            result.insert(res);
                        }
                        else if(offset == 0 && res.type == 1)
                        {
                            std::stringstream sstream;
                            sstream << "val(0x" << std::hex << res.addr<<")";
                                                std::string new_str = sstream.str();
                            result.emplace(3, 0, new_str, func);
                        }
                        else
                        {
                            std::stringstream sstream;
                                            sstream << "val(" << res.desc << " + 0x" << std::hex << offset <<")";
                                            std::string new_str = sstream.str();
                            result.emplace(3, 0, new_str, func);
                        }
                    }
                }
            }
            
            }
        }
    }
    //If there is no register definition found, then there are two cases
    //1. In case of icall target analysis, check if this instruction is a CALL and if the invoked function returns AT list and if reg=rax, then get the returned AT
    //2. If not, result is unresolved 
    else
    {
        bool foundreturnAT = false;
        if(analysisType == 1)   //Only for indirect call analysis
        {
            LOG(1, "NO_DEF_FLAG" << std::hex << instr->getAddress());

            auto pushReturnVals = [&](Function* targetFunc) 
            {
                LOG(1, "NO DEF FLAG " << std::hex << instr->getAddress() << " targets " << targetFunc->getName());
        
                auto target_ipnode = ip_callgraph->getNode(targetFunc);
                if (target_ipnode != nullptr)
                {
                    auto returnvals = target_ipnode->getATReturn();
                    for (auto r : returnvals)
                    {
                        foundreturnAT = true;
                        std::stringstream sstream;
                        sstream << "0x" << std::hex << r->getAddress();
                        std::string new_str = sstream.str();
                        result.emplace(1, r->getAddress(), new_str, func);
                        LOG(1, "RETVAL " << std::hex << instr->getAddress() << " " << r->getAddress() << " " << r->getName());
                    }
                }
            };

            if (auto cfi = dynamic_cast<ControlFlowInstruction*>(instr->getSemantic()))
            {
                auto link = cfi->getLink();
                auto target = link->getTarget();
        
                if (auto func_target = dynamic_cast<Function*>(target))
                {
                    pushReturnVals(func_target);
                }
                else if (auto plt = dynamic_cast<PLTTrampoline*>(target)) // Call via PLT
                {
                    if (auto ext_target = dynamic_cast<Function*>(plt->getTarget()))
                    {
                        pushReturnVals(ext_target);
                    }
                }
            }

        }
        if(!foundreturnAT)
        {
            LOG(1,"DFD ? (NO REGDEF) FUNC : "<<func->getName()<<std::hex<<" "<<instr->getAddress()<<" "<<std::dec<<reg);
            pushUnknownIfNeeded(result);
        }
    }



    //Cleanup logic
    
    auto it1 = find(visitedStates.begin(), visitedStates.end(), visited_tup);
    if(it1 != visitedStates.end())
    {
        visitedStates.erase(it1);
    }
    recursion_iter--;
    LOG(1,"Iter "<<std::dec<<recursion_iter<<" Returning from findRegDef "<<func->getName()<<" "<<std::dec<<reg);
    return;
}


void SyspartUtility::printResult(UDResult res)
{
    LOG(1,"*************************** TYPE : "<<res.type<<" ADDR : 0x"<<std::hex<<res.addr<<" DESC "<<res.desc<<" FUNC : "<<(res.func)->getName()<<" MODULE : " <<(res.func)->getParent()->getParent()->getName());
}


string SyspartUtility::getFunctionName(address_t addr)
{
    string func_name="";
    for(auto module : CIter::children(program))
    {
        for(auto func : CIter::functions(module))
        {
            if(func->getAddress() == addr)
                func_name = func->getName();
        }
    }
    return func_name;
}

bool SyspartUtility::findIndirectCallTargets(IPCallGraphNode* n)
{
    recursion_iter++;
    auto function = n->getFunction();
    bool resolvedFlag = false;

    auto working = df.getWorkingSet(function);

    auto pushUnknownIfNeeded = [&](std::unordered_set<UDResult>& results, Function* function) 
    {
        LOG(1, "Unknown value at "<<function->getName());
            UDResult unknownRes{0, 0, "unknown", function};
        results.insert(unknownRes);
    };

    auto isArgRegister = [](int reg) 
    {
        return reg == 7 || reg == 6 || reg == 2 || reg == 1 || reg == 8 || reg == 9;
    };

    auto resolveRegister = [&](int reg, UDState* state, Function* func, std::unordered_set<UDResult>& results) 
    {
            bool refFlag = false;
            for (auto& rf : state->getRegRef(reg)) 
            {
                refFlag = true;
                findRegDef(func, rf, reg, results);
                LOG(1,"Resolving register "<<reg);
            }           
            if (!refFlag) 
            {
                if(isArgRegister(reg)) 
                {
                    LOG(1, "Check argument registers for reg " << reg);
                    getArgumentsPassedToFunction(func, reg, results);  // must also return std::vector<UDResult>
                    refFlag = true;
                }   
                else
                {
                    LOG(1, "No reg ref and arguments registers");
                     pushUnknownIfNeeded(results, func);
                }       
        }
    };

    enum class BinaryOpType { ADD, SUB };
    auto combineResults = [&](const std::unordered_set<UDResult>& a,
                                const std::unordered_set<UDResult>& b,
                                BinaryOpType op,
                                std::unordered_set<UDResult>& out) 
    {
        LOG(1, "Res1.size "<<a.size()<< " Res2.size "<<b.size());
        if(op == BinaryOpType::ADD)
                LOG(1, "ADD");
        else if(op == BinaryOpType::SUB)
                LOG(1, "SUB");
	    int tot_count=1000000;
	    int i=0;
        for (const auto& res1 : a) 
        {
		    if(i > tot_count)
		    {
			     break;
		    }
                for (const auto& res2 : b) 
                {
		          if(i > tot_count)
		          {
			         break;
		          }
		          i++;
                  if (res1.type == 0 || res2.type == 0)
                  {
                        pushUnknownIfNeeded(out, function);

                  } 
                  else if (res1.type == 1 && res2.type == 1) 
                  {
                        address_t new_val = (op == BinaryOpType::ADD)
                        ? res1.addr + res2.addr
                        : res1.addr - res2.addr;
                        std::stringstream sstream;
                        sstream << "0x" << std::hex << new_val;
                        std::string new_str = sstream.str();
                        out.emplace(1, new_val, new_str, function);
                  } 
                  else 
                  {
                        std::string op_str = (op == BinaryOpType::ADD) ? " + " : " - ";
                        std::string new_str = res1.desc + op_str + res2.desc;
                        out.emplace(3, 0, new_str, function);
                  }
                }
        }
    };

	enum class ConstOpType { ADD, SUB, MULT };
    auto combineWithConstant = [&](const std::unordered_set<UDResult>& vals,
                              address_t constant,
                              ConstOpType op,
                              std::unordered_set<UDResult>& out) 
    {

        LOG(1, "Res1.size "<<vals.size()<<std::hex<<constant);
        if(op == ConstOpType::ADD)
            LOG(1, "ADD");
        else if(op == ConstOpType::SUB)
            LOG(1, "SUB");
	   else if(op == ConstOpType::MULT)
		    LOG(1, "MULT");

        for (const auto& udres : vals) 
        {
            if (udres.type == 0)
            {
	                LOG(1,"Combine with constant");
        	        pushUnknownIfNeeded(out, function);
            }
            else if (udres.type == 1) 
            {
			    address_t new_val;
		        if (op == ConstOpType::ADD)
				    new_val =  udres.addr + constant;
			    else if (op == ConstOpType::SUB)
 	                new_val = udres.addr - constant;
			    else if (op ==  ConstOpType::MULT)
				    new_val = udres.addr * constant;
	            std::stringstream sstream;
        	    sstream << "0x" << std::hex << new_val;
                out.emplace(1, new_val, sstream.str(), function);
            } 
		    else 
		    {
                	std::stringstream sstream;
	                if (op == ConstOpType::ADD) 
                    {
        	                sstream << "val(0x" << std::hex << constant << " + " << udres.desc<<")";
                	} 
			        else if (op == ConstOpType::SUB)
			         {
                        	sstream << "val(" << udres.desc << " - 0x" << std::hex << constant<<  ")";
                	}
			         else if (op == ConstOpType::MULT)
                        {
				        sstream << "val(0x" << std::hex << constant << " * " << udres.desc << ")";
                        }

                	out.emplace(3, 0, sstream.str(), function);
                }
        }
    };



    for (auto block : CIter::children(function)) 
    {
        for (auto instruction : CIter::children(block)) 
        {
            auto semantic = instruction->getSemantic();
            auto state = working->getState(instruction);

            auto ici = dynamic_cast<IndirectCallInstruction *>(semantic);
            auto iji = dynamic_cast<IndirectJumpInstruction *>(semantic);
	    
            if (iji && iji->isForJumpTable())
            {
                continue;
            }
        
            if (!(ici || iji))
            {
                continue;
            }

            // Handle iteration path logic
            cur_function = function;
            cur_instr = instruction;
            bool continueFlag = true;

            if (iter > 1) 
            {
                continueFlag = false;
                LOG(1, "Unsetting continue flag for "<<cur_function->getName()<<" "<<std::hex<<cur_instr->getAddress());
                vector<Function*> fp_vec;
                FPath fp{cur_function->getName(), cur_instr->getAddress(), fp_vec};
                auto icPath_iter = find(icPath.begin(), icPath.end(), fp);
                if (icPath_iter != icPath.end()) 
                {
                    auto& path_set = (*icPath_iter).path;
                    for (auto p : path_set) 
                    {
                        for (auto r : prev_resolvedFns) 
                        {
                            if (r->getName() == p->getName() && r->getAddress() == p->getAddress()) 
                            {
                                continueFlag = true;
                                LOG(1, "Setting continue flag for "<<cur_function->getName()<<" "<<std::hex<<cur_instr->getAddress());
                                path_set.clear();
                                path_set.push_back(cur_function);
                                (*icPath_iter).path = path_set;
                                break;
                            }
                        }
                        if (continueFlag)
                        {
                            break;
                        }
                    }
                }
            } 
            else 
            {
                vector<Function*> fp_vec{cur_function};
                icPath.emplace_back(cur_function->getName(), cur_instr->getAddress(), fp_vec);
            }
                
            if(!continueFlag)
            {
                LOG(1, "Continuing... Flag set");
                continue;
            }
            if(n->isIcallResolved(instruction->getAddress()))
            {
                LOG(1, "Continuing... Already resolved address"<<std::hex<<instruction->getAddress());
                continue;
            }
            
            if(ici)
            {
                ici->clearAllTargets();    
            }
			
            stack_depth = 0;

            vector<IndirectCallTarget> icTargets;
	    std::unordered_set<UDResult> results;
            if(ici && ici->hasMemoryOperand()) 
            {
                    IndirectCallTarget target(instruction->getAddress());
                    target.setUnknown();
                    icTargets.push_back(target);
                    
		auto indexReg = X86Register::convertToPhysical(ici->getIndexRegister());
		auto reg = X86Register::convertToPhysical(ici->getRegister());
		auto scale = ici->getScale();
		auto disp = ici->getDisplacement();

		InstrDumper instrdumper(instruction->getAddress(), INT_MIN);
		instruction->getSemantic()->accept(&instrdumper);
		LOG(1, "ICALL_MEM_OP REG " << std::dec << reg << " INDEXREG = "<< indexReg << " SCALE = " <<  ici->getScale() << std::hex << " DISP = " << ici->getDisplacement());
		
		std::unordered_set<UDResult> regResult;
		std::unordered_set<UDResult> indexResult;

		if(reg != -1)
		{
	                resolveRegister(reg, state, function, regResult);
		}

		if(indexReg != -1)
		{
        	        resolveRegister(indexReg, state, function, indexResult);
		}
                instruction->getSemantic()->accept(&instrdumper);

		// disp(reg, index, scale) = value at memory address (reg + index * scale + disp)
		if(indexReg == -1) // reg + disp
		{
			if(disp > 0)
				combineWithConstant(regResult, (address_t)disp, ConstOpType::ADD, results);
			else if(disp < 0)
			{
				disp = (-1) * disp;
				combineWithConstant(regResult, (address_t)disp, ConstOpType::SUB, results);
			}
			else //disp = 0
			{
				results = regResult;
			}
		}
		else		  // reg + index * scale + disp
		{
			// index*scale
			std::unordered_set<UDResult> inter_result1;
			combineWithConstant(indexResult, scale, ConstOpType::MULT, inter_result1);

			// reg + (index*scale)
			std::unordered_set<UDResult> inter_result2;
			combineResults(inter_result1, regResult, BinaryOpType::ADD, inter_result2);

			// reg + (index*scale) + disp
			if(disp > 0)
                                combineWithConstant(inter_result2, (address_t)disp, ConstOpType::ADD, results);
                        else if(disp < 0)
                        {
                                disp = (-1) * disp;
                                combineWithConstant(inter_result2, (address_t)disp, ConstOpType::SUB, results);
                        }
			else // disp = 0
			{
				results = inter_result2;
			}
		}
		
                LOG(1, "ICALL_RESOLVE UNRESOLVE_MEMORY_OPERAND " << std::hex << instruction->getAddress()
                      << " " << std::dec << instruction->getAddress() << " " << function->getName());
		////indent();
                LOG(1, "CAUTION : UNRESOLVED IC DUE TO MEMORY OPERAND"); 
            }
            else 
            {
                int reg = -1;
		          if(ici)
		          {
                    reg = X86Register::convertToPhysical(ici->getRegister());
		          }
		          else if(iji)
		          {
			         reg = X86Register::convertToPhysical(iji->getRegister());
		          }
                resolveRegister(reg, state, function, results);
            }
                for (auto& r : results) 
                    {
                        printResult(r);
                        if(r.type == 0 | r.type == 3)
                        {
                            IndirectCallTarget target(r.addr);
                            target.setUnknown();
                            icTargets.push_back(target);
                            continue;
                        }
                        if(r.type == 1)
                        {
                            IndirectCallTarget target(r.addr);
                            auto func_name = getFunctionName(r.addr);
                            if(!func_name.empty())
                            {
                                target.setName(func_name);
                            }
                            else if(r.addr != 0)
                            {
                                target.setGlobal();
                            }
                            icTargets.push_back(target);
                        }
                        else
                        {
                            IndirectCallTarget target(r.addr);
                            target.setUnknown();
                            icTargets.push_back(target);
                        }
                    }
                    
                //Add resolved indirect calls
                bool resolved = true;
                set<address_t> targets;
                if(icTargets.size() == 0)
                    resolved = false;
                for(int i=0; i<icTargets.size(); i++)
                {
                    if(icTargets[i].isGlobal() || icTargets[i].isUnknown())
                    {
                        resolved = resolved & false;
                        break;
                    }
                    else
                    {
                        resolved = resolved & true;
                        targets.insert(icTargets[i].getAddress());
                    }
                }   
                if(resolved)    //only icanalysis finds all targets
                {
                    for(auto t : targets)
                    {                           
                        auto target_set = getFunctionByAddress(t, (Module*)function->getParent()->getParent());
                        for(auto func_target : target_set)
                        {
                            auto node_target = ip_callgraph->getNode(func_target);
                            if(node_target == NULL)
                            {
                                //cout<<"STRANGE NO IPNODE WITH ADDR FOUND "<<std::hex<<t<<endl;
                                continue;
                            }
                            if(find(new_resolvedFns.begin(), new_resolvedFns.end(), func_target) == new_resolvedFns.end())
                            {   
                                new_resolvedFns.push_back(func_target);
                            }
                            n->insertCallTarget(instruction->getAddress(), false, node_target);
                            node_target->insertParent(instruction->getAddress(), n, false);
                        }
                    }
                    n->setIcallResolved(instruction->getAddress(), true);
                    
                    resolvedFlag = true;
                }   
            

        }
    }
    visitedStates.clear();
    visitedFuncRegs.clear();
    vector<tuple<Instruction*, int>>().swap(visitedStates);
    vector<tuple<string,address_t,int>>().swap(visitedFuncRegs);
    found_results.clear();
    found_state_results.clear();
    recursion_iter--;
    
    return resolvedFlag;
}

set<Function*> SyspartUtility::getFunctionByAddress(address_t addr, Module* mod)
{
    set<Function*> func_set;
    bool found = false;

    for(auto func : CIter::functions(mod))
    {
        if(func->getAddress() == addr)
        {
            found = true;
            bool flag = true;
            for(auto f : func_set)  //To avoid duplicate function objects that refer to the same function
            {
                if((f->getName() == func->getName()) && (f->getParent()->getParent() == mod))
                    flag = false;
            }
            if(flag)                //No duplicates found
        {
                func_set.insert(func);
        }
        }
    }

    if(!found)
    {
        for(auto module : CIter::children(program))
        {
            if(module->getName() == mod->getName())
                continue;
            for(auto func : CIter::functions(module))
            {
                if(func->getAddress() == addr)
                {
                    bool flag = true;
                    for(auto f : func_set)  //To avoid duplicate function objects that refer to the same function
                    {
                        if((f->getName() == func->getName()) && (f->getParent()->getParent() == module))
                            flag = false;
                    }
                    if(flag)                //No duplicates found
            {
                        func_set.insert(func);
            }
                }
            }
        }
    }
    return func_set;
}

void SyspartUtility::initialize()
{
    for(auto n : ip_callgraph->nodeMap)
    {
        df.addUseDefFor(n.first);
    }

}
