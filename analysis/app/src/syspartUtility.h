#ifndef SYSPARTUTILITY
#define SYSPARTUTILITY

#include<vector>
#include<unordered_set>


#include "ipcallgraph.h"
#include "analysis/usedef.h"
#include "analysis/usedefutil.h"
#include "analysis/dataflow.h"

struct UDResult
{
	int type;	//0 for unknown, 1 for value, 2 for value stored in address, 3 for complex
	address_t addr;
	string desc;
	Function* func;

	UDResult(int t, address_t a, const std::string& d, Function* f)
        : type(t), addr(a), desc(d), func(f) {}

	bool operator==(const UDResult& l) const
  	{
    	 if((l.type == this->type) && (l.addr == this->addr) && (l.desc == this->desc) && ((l.func)->getName() == (this->func)->getName()))
    	 	return true;
    	 return false;
  	}
};

//Required for using std::unordered_set<UDResult>
namespace std {
    template<> struct hash<UDResult>
    {
        size_t operator()(const UDResult& k) const
        {
	
	     // Get hashes for all four key members
            size_t h1 = std::hash<int>()(k.type);
            size_t h2 = std::hash<address_t>()(k.addr);
            size_t h3 = std::hash<std::string>()(k.desc);
            size_t h4 = k.func ? std::hash<std::string>()(k.func->getName()) : 0;

            // Combine them robustly
            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h4 + 0x9e3779b9 + (seed << 6) + (seed >> 2); // <-- Added hash for func name

            return seed;
        }
    };
}

struct Inter_result
{
	string fname;
	address_t addr;
	int reg;
	std::unordered_set<UDResult> res;

	Inter_result(const std::string& name, address_t a, int r, const std::unordered_set<UDResult>& resultSet)
        : fname(name), addr(a), reg(r), res(resultSet)
    	{}

	bool operator==(const Inter_result& l) const
	{
		if((l.fname == this->fname) &&(l.addr == this->addr) && (l.reg == this->reg))
			return true;
		return false;
	}
};

namespace std {
    template<> struct hash<Inter_result>
    {
        size_t operator()(const Inter_result& k) const
        {
            size_t h1 = std::hash<std::string>()(k.fname);
            size_t h2 = std::hash<address_t>()(k.addr);
            size_t h3 = std::hash<int>()(k.reg);

            // Combine the hashes robustly
            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}

struct Intra_result
{	string fname;
	address_t faddr;
	address_t iaddr;
	int reg;
	std::unordered_set<UDResult> res;


	Intra_result(const std::string& name, address_t func_addr, address_t instr_addr, int r, const std::unordered_set<UDResult>& resultSet)
        : fname(name),
          faddr(func_addr),
          iaddr(instr_addr),
          reg(r),
          res(resultSet)
    {}

	bool operator==(const Intra_result& l) const
	{
		if((l.fname == this->fname) &&(l.faddr == this->faddr) && (l.iaddr == this->iaddr) && (l.reg == this->reg))
			return true;
		return false;
	}

};

namespace std {
    template<> struct hash<Intra_result>
    {
        size_t operator()(const Intra_result& k) const
        {
            size_t h1 = std::hash<std::string>()(k.fname);
            size_t h2 = std::hash<address_t>()(k.faddr);
            size_t h3 = std::hash<address_t>()(k.iaddr);
            size_t h4 = std::hash<int>()(k.reg);

            // Combine the four hashes robustly
            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h4 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}

struct FPath
{
	string fname;
	address_t iaddr;
	vector<Function*> path;

	 FPath(const std::string& fname_, address_t iaddr_, const std::vector<Function*>& path_)
        : fname(fname_), iaddr(iaddr_), path(path_) {}

	bool operator==(const FPath& l)
	{
		if((l.fname == this->fname) &&(l.iaddr == this->iaddr))
			return true;
		return false;
	}
};

class SyspartUtility
{
	public :
		
		SyspartUtility(Program *program, IPCallGraph *ipc, int analysisType)
		{
			this->program = program;
			this->ip_callgraph = ipc;
			this->iter = 0;
			this->analysisType = analysisType;
			this->recursion_iter = 0;

		}
		void initialize();
		void getArgumentsPassedToFunction(Function* func, int reg, std::unordered_set<UDResult>& results_out);		
		void findRegDef(Function* func, UDState *state, int reg, std::unordered_set<UDResult>& results_out);
		bool findIndirectCallTargets(IPCallGraphNode* n);
		string getFunctionName(address_t addr);
		void printResult(UDResult res);
		set<Function*> getFunctionByAddress(address_t addr, Module* mod);
		vector<Function*> new_resolvedFns;
		vector<Function*> prev_resolvedFns;
		int iter=0;
	private :
		int recursion_iter=0;
		int MAX_ITER=100;
		int analysisType; ////0 when passed to find values of argument register, 1 for indirect call target analysis, 2 for others
		std::unordered_set<Inter_result> found_results;
		std::unordered_set<Intra_result> found_state_results;
		vector<tuple<string,address_t,int>> visitedFuncRegs;
		Program *program=NULL;
		IPCallGraph *ip_callgraph=NULL;
		vector<tuple<Instruction*, int>> visitedStates;
		vector<FPath>icPath;
		Function* cur_function;
		Instruction* cur_instr;
		int arg_count=0;
		int reg_count=0;
		int stack_depth=0;
		DataFlow df;

		void getArgumentsPassedToFunction_helper(Function* func, int reg, std::unordered_set<UDResult>& results_out);
	    	void findRegDef_helper(Function* func, UDState *state, int reg, std::unordered_set<UDResult>& results_out);
    		vector<UDState*> matchLoadMemoryToStores(Function* func, UDState* orig, set<UDState*> storeNodes);

};
#endif
