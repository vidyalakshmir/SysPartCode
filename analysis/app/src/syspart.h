#ifndef SYSPART
#define SYSPART

#include<map>
#include<set>
#include<tuple>
#include<bits/stdc++.h>
using namespace std;

#include "conductor/setup.h"
#include "chunk/concrete.h"
#include "ipcallgraph.h"
#include "analysis/usedef.h"
#include "syspartUtility.h"


class Syspart
{
	public :
		struct SysNode
		{
			Function *func;
    		set<int> direct_syscalls;           //System calls directly invoked by this function
    		set<int> derived_syscalls;
    		set<int> all_syscalls;
    		int color;
    		std::map<SysNode*,bitset<350>> syscall_info;   //All system calls from the function <Node,bitset>, where node is one among the children
    		std::map<Instruction *, std::set<unsigned long>> syscallMap;
		};

	private :
		int partitionSize=0;
		set<Function*> partitionFns;
		map<IPCallGraphNode*, SysNode*> syscall_mapping;	//IPCallGraphNode to SysNode mapping (IPCallGraph)
		map<int, vector<SysNode*>> who_invokes_syscalls;
		IPCallGraph ip_callgraph;						//Object of IPCallGraph
		int build_count=0;
		Function *start_func=NULL;
		string startFuncFile;
		vector<Function*> finiFuncs;
		vector<Function*> initFuncs;
		std::map<int, string> system_calls;
		Program *program=NULL;
		ConductorSetup *setup;
		enum Color{WHITE, GREY, BLACK};
		string typearmorPath;
		map<Block*,int> noreturn_done;	//To keep track of blocks whose nonreturn are evaluated
		vector<IPCallGraphNode*> stack_of_nodes_visited_nodes;
	public :
		//Initialization and general functionalities
		void setStartFunc(Function *func);
		void setStartFuncFile(string file);
		void setProgram(Program *program);
		void setConductorSetup(ConductorSetup *setup);
	    Function* findFunctionByName(string fname);
	    Function* findFunctionByAddress(address_t addr);
		void getCallPath(IPCallGraphNode* n, int depth, set<IPCallGraphNode*> *visited, set<Function*> atlist);
		void printAllFunctions();
		void ifPathExists(string start,string end, bool icanalysisFlag, bool typearmorFlag);
		void getArgumentValue(bool icanalysisFlag, bool typearmorFlag, string function, int reg, char* filename);
		void printAllSections();
		void setTypeArmorPath(string path)
		{
			typearmorPath = path;
		}
		//System call related methods
		
		void populateSyscallMap();
		void populateDefaultSyscalls();
		bool populateFromHeaders();
		void findDirectSyscalls();
		void findDirectSyscallsOfModule(Module* m);
		void findCallGraphOfModule(Module* m);		
		void findDerivedSyscalls1(Function *func);
		void findDerivedSyscalls(Function *func);
		void findDerivedSyscalls3(Function *func);
		void findDerived4();

		bitset<350> buildSysCallTree(IPCallGraphNode *n, bool *flag);
		void getSyscallInfo(Function *func);
		SysNode* getSysNode(Function *f);
		set<int> findSyscallsAccessible(address_t addr, Function* f);
		set<int> getSyscalls(SysNode* sysnode);
		void getDirectSyscallsFromStart();
		void getDirectSyscalls();
		set<int> getSyscallsofFini();
		set<int> getSyscallsofInit();
		void isFunctionReachable(string address, string start, string end, bool icanalysisFlag, bool typearmorFlag);
		bool findFunctionsReachable(address_t addr, Function* func, string end);

		//Thread related functions
		void find_syscalls_in_thread(bool direct, bool icanalysisFlag, bool typearmorFlag);
		Function* find_thread_function(Function *f, address_t addr, address_t &thread_start_func);
		Function* findRegDef(UDState* state, int reg, address_t &thread_start_func);
		set<Function*> getThreadStartFunction();
		bool ifForkInvoked();

		//Partition methods
		void findReachableCode(bool direct, bool icanalysisFlag, bool typearmorFlag, address_t addr, string func_name);
		int isNonReturn(ControlFlowGraph *cfg, Block* bl, set<Block*> visited);

		//Methods that implement experimental analysis
		void run1(bool direct, bool icanalysisFlag, bool typearmorFlag);
		
		void run2(bool direct, bool icanalysisFlag, bool typearmorFlag, string func_name="*");
		void run3(bool direct, bool icanalysisFlag, bool typearmorFlag);
		void run4(bool direct, bool icanalysisFlag, bool typearmorFlag);
		void run5(bool direct, bool icanalysisFlag, bool typearmorFlag);

		void run6(bool direct, bool icanalysisFlag, bool typearmorFlag);
		
		void run7(bool direct, bool icanalysisFlag, bool typearmorFlag, string fname);
		void run8(bool direct, bool icanalysisFlag, bool typearmorFlag, vector<string> funcs);
		void run9(bool direct, bool icanalysisFlag, bool typearmorFlag);
		void run10(bool direct, bool icanalysisFlag, bool typearmorFlag, string sys_name);
		void syscallsOfMainLoop(bool icanalysisFlag, bool typearmorFlag, string addr, string func_name);
		void run11();
		void run12(bool direct, bool icanalysisFlag, bool typearmorFlag);
		void run13(bool direct, bool icanalysisFlag, bool typearmorFlag);
		void run14(bool direct, bool icanalysisFlag, bool typearmorFlag, string func_name="*");
		void getPartitionSize(bool direct, bool icanalysisFlag, bool typearmorFlag, string fname);
		void printAICT(bool icanalysisFlag, bool typearmorFlag);	//Average Indirect Call Target
		void getSyscallsFromDlsym(bool direct, bool icanalysisFlag, bool typearmorFlag, string file_name);
		int getNoReturnFnCount();
		void printFunctions();
		void printDlArgs(string dlname);
		void printDirectSyscalls();
		void run15(bool direct, bool icanalysisFlag, bool typearmorFlag, int option);
		void printDisass(string fname);
};
#endif
