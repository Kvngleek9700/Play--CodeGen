#ifndef _JITTER_STATEMENT_H_
#define _JITTER_STATEMENT_H_

#include <list>
#include <functional>
#include "Jitter_SymbolRef.h"

namespace Jitter
{
	enum OPERATION
	{
		OP_NOP = 0,
		OP_MOV,
		
		OP_ADD,
		OP_SUB,
		OP_CMP,

		OP_AND,
		OP_OR,
		OP_XOR,
		OP_NOT,

		OP_SRA,
		OP_SRL,
		OP_SLL,

		OP_MUL,
		OP_MULS,
		OP_DIV,
		OP_DIVS,

		OP_LZC,

		OP_ADD64,
		OP_SUB64,
		OP_AND64,
		OP_CMP64,
		OP_MERGETO64,
		OP_EXTLOW64,
		OP_EXTHIGH64,
		OP_SRA64,
		OP_SRL64,
		OP_SLL64,

		OP_MD_SUB_B,
		OP_MD_SUB_W,
		OP_MD_AND,
		OP_MD_OR,
		OP_MD_XOR,
		OP_MD_NOT,

		OP_FP_ADD,
		OP_FP_SUB,
		OP_FP_MUL,
		OP_FP_DIV,
		OP_FP_SQRT,
		OP_FP_RCPL,
		OP_FP_NEG,
		OP_FP_CMP,

		OP_FP_TOINT_TRUNC,

		OP_PARAM,
		OP_CALL,
		OP_RETVAL,
		OP_JMP,
		OP_CONDJMP,
		OP_GOTO,

		OP_LABEL,
	};

	enum CONDITION
	{
		CONDITION_NEVER = 0,
		CONDITION_EQ,
		CONDITION_NE,
		CONDITION_BL,
		CONDITION_BE,
		CONDITION_AB,
		CONDITION_AE,
		CONDITION_LT,
		CONDITION_LE,
		CONDITION_GT,
		CONDITION_GE,
	};

	struct STATEMENT
	{
	public:
		STATEMENT()
			: op(OP_NOP)
			, jmpBlock(-1)
			, jmpCondition(CONDITION_NEVER)
		{
			
		}

		OPERATION		op;
		SymbolRefPtr	src1;
		SymbolRefPtr	src2;
		SymbolRefPtr	dst;
		uint32			jmpBlock;
		CONDITION		jmpCondition;

		typedef std::tr1::function<void (const SymbolRefPtr&, bool)> OperandVisitor;

		void VisitOperands(const OperandVisitor& visitor)
		{
			if(dst) visitor(dst, true);
			if(src1) visitor(src1, false);
			if(src2) visitor(src2, false);
		}

		void VisitDestination(const OperandVisitor& visitor)
		{
			if(dst) visitor(dst, true);
		}

		void VisitSources(const OperandVisitor& visitor)
		{
			if(src1) visitor(src1, false);
			if(src2) visitor(src2, false);
		}
	};

	typedef std::list<STATEMENT> StatementList;
}

#endif
