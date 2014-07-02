/**
 * @file Parser.h
 * @author Gereon Kremer <gereon.kremer@cs.rwth-aachen.de>
 */

#pragma once

#include <cassert>
#include <iterator>
#include <list>

#define BOOST_SPIRIT_USE_PHOENIX_V3

#include <boost/variant.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_numeric.hpp>
#include <boost/spirit/include/support_line_pos_iterator.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_statement.hpp>

#include "../../lib/Common.h"
#include "../../lib/ConstraintPool.h"
#include "../../lib/Formula.h"
#include "../../lib/FormulaPool.h"
#include "ParserUtils.h"
#include "ParserTypes.h"

namespace smtrat {
namespace parser {

namespace spirit = boost::spirit;
namespace qi = boost::spirit::qi;
namespace px = boost::phoenix;

template <typename... T>
using rule = qi::rule<Iterator, T()..., Skipper>;

class SMTLIBParser
{
	
private:
	InstructionHandler* handler;
	px::function<SuccessHandler> successHandler;
	px::function<SuccessHandlerPtr> successHandlerPtr;
	px::function<ErrorHandler> errorHandler;
	std::istream* mInputStream;
		
public:
	bool queueInstructions;
	
	DeclaredSymbolParser<carl::Variable> var_bool;
	DeclaredSymbolParser<carl::Variable> var_theory;
	
	DeclaredSymbolParser<const Formula*> bind_bool;
	DeclaredSymbolParser<Polynomial> bind_theory;
	
	// Basic rules
	Skipper skipper;
	SymbolParser symbol;
	StringParser string;
	RelationParser relation;
	BooleanOpParser op_bool;
	TheoryOpParser op_theory;
	DomainParser domain;
	LogicParser logic;
	
	rule<> boundary;
	
	// Numbers
	IntegralParser integral;
	DecimalParser decimal;
	
	// Variables
	rule<carl::Variable> var;
	rule<carl::Variable> quantifiedVar;
	rule<std::pair<std::string, carl::VariableType>> sortedVar;
	rule<std::string> key;
	rule<Attribute> attribute;
	
	rule<AttributeValue> value;
	rule<std::vector<std::string>> symlist;
	rule<std::vector<carl::Variable>> varlist;
	rule<> bindlist;
	qi::rule<Iterator, qi::unused_type, Skipper, qi::locals<std::string>> binding;
	
	// Custom functions
	qi::symbols<char, BooleanFunction> funmap_bool;
	qi::symbols<char, TheoryFunction> funmap_theory;
	qi::rule<Iterator, Skipper, qi::locals<std::string, std::vector<carl::Variable>>> fun_definition;

	rule<Arguments> fun_arguments;

	// Commands	
	rule<> cmd;
	
	// Formula
	rule<const Formula*> formula;
	rule<const Formula*> formula_op;
	rule<PointerSet<Formula>> formula_list;
	
	// Polynomial
	rule<Polynomial> polynomial;
	rule<std::pair<Polynomial::ConstructorOperation, std::vector<Polynomial>>> polynomial_op;
	rule<Polynomial> polynomial_ite;
	rule<Polynomial> polynomial_fun;
	// Main rule
	rule<> main;
	
public:
	
	SMTLIBParser(InstructionHandler* ih, bool queueInstructions, bool debug = false);

	bool parse(std::istream& in, const std::string& filename);

protected:
	void add(const Formula* f);
	void check();
	void declareConst(const std::string&, const carl::VariableType&);
	void declareFun(const std::string& name, const std::vector<carl::VariableType>& args, const carl::VariableType& sort);
	void declareSort(const std::string&, const Rational&);
	void defineFun(const std::string&, const std::vector<carl::Variable>&, const carl::VariableType&, const boost::variant<const Formula*, Polynomial>&);
	void defineSort(const std::string&, const std::vector<std::string>&, const std::string&);
	void exit();
	void getAssertions();
	void getAssignment();
	void getInfo(const std::string& key);
	void getOption(const std::string& key);
	void getProof();
	void getUnsatCore();
	void getValue(const std::vector<carl::Variable>&);
	void pop(const Rational&);
	void push(const Rational&);
	void setInfo(const std::string& key, const AttributeValue& val);
	void setLogic(const smtrat::Logic&);
	void setOption(const std::string& key, const AttributeValue& val);
	
	template<typename Function, typename... Args>
	void callHandler(const Function& f, const Args&... args) {
		if (this->queueInstructions) {
			this->handler->addInstruction(std::bind(f, this->handler, args...));
		} else {
			(this->handler->*f)(args...);
		}
	}
	
private:
	smtrat::Logic mLogic;
	PointerSet<Formula> mTheoryIteBindings;
	std::map<carl::Variable, std::tuple<const Formula*, Polynomial, Polynomial>> mTheoryItes;

	struct Scope {
	private:
		qi::symbols<char, carl::Variable> var_bool;
		qi::symbols<char, carl::Variable> var_theory;
		qi::symbols<char, const Formula*> bind_bool;
		qi::symbols<char, Polynomial> bind_theory;
	public:
		Scope(const SMTLIBParser& parser):
			var_bool(parser.var_bool.sym),
			var_theory(parser.var_theory.sym),
			bind_bool(parser.bind_bool.sym),
			bind_theory(parser.bind_theory.sym)
		{}
		void restore(SMTLIBParser& parser) {
			parser.var_bool.sym = this->var_bool;
			parser.var_theory.sym = this->var_theory;
			parser.bind_bool.sym = this->bind_bool;
			parser.bind_theory.sym = this->bind_theory;
		}
	};

	std::stack<Scope> mScopeStack;

	bool isSymbolFree(const std::string& name, bool output = true) {
		if (output) {
			if (name == "true" || name == "false") this->handler->error() << "\"" << name << "\" is a reserved keyword.";
			else if (this->var_bool.sym.find(name) != nullptr) this->handler->error() << "\"" << name << "\" has already been defined as a boolean variable.";
			else if (this->var_theory.sym.find(name) != nullptr) this->handler->error() << "\"" << name << "\" has already been defined as a theory variable.";
			else if (this->bind_bool.sym.find(name) != nullptr) this->handler->error() << "\"" << name << "\" has already been defined as a boolean binding.";
			else if (this->bind_theory.sym.find(name) != nullptr) this->handler->error() << "\"" << name << "\" has already been defined as a theory binding.";
			else return true;
			return false;
		} else {
			if (name == "true" || name == "false") return false;
			else if (this->var_bool.sym.find(name) != nullptr) return false;
			else if (this->var_theory.sym.find(name) != nullptr) return false;
			else if (this->bind_bool.sym.find(name) != nullptr) return false;
			else if (this->bind_theory.sym.find(name) != nullptr) return false;
			else return true;
		}
	}
	
	void pushScope() {
		mScopeStack.emplace(*this);
	}
	void popScope() {
		mScopeStack.top().restore(*this);
		mScopeStack.pop();
	}

	const Formula* mkBoolean(const carl::Variable& var) {
        return newFormula(var);
    }
	const Formula* mkConstraint(const Polynomial&, const Polynomial&, Relation);
	Polynomial mkIteInExpr(const Formula* _condition, Polynomial& _then, Polynomial& _else);
	const Formula* mkFormula(Type _type, PointerSet<Formula>& _subformulas);
	
	carl::Variable addQuantifiedVariable(const std::string& _name, const boost::optional<carl::VariableType>& type);
	carl::Variable addVariableBinding(const std::pair<std::string, carl::VariableType>&);
	void addTheoryBinding(std::string& _varName, Polynomial& _polynomial);
	void addBooleanBinding(std::string&, const Formula*);
		
public:
	std::stringstream lastrule;
	std::stringstream lastentity;
	template<typename Rule, typename Entity>
	void lastRule(const Rule& rule, Entity& entity) {
		lastrule.str("");
		lastrule << rule.name();
		lastentity.str("");
		lastentity << entity;
	}
};

}
}