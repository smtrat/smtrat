#pragma once

#include "Common.h"
#include "Lexicon.h"
#include "Identifier.h"
#include "SExpression.h"
#include "Sort.h"
#include "theories/Theories.h"

namespace smtrat {
namespace parser {

struct QualifiedIdentifierParser: public qi::grammar<Iterator, Identifier(), Skipper> {
	QualifiedIdentifierParser(): QualifiedIdentifierParser::base_type(main, "qualified identifier") {
	    main = 
				identifier[qi::_val = qi::_1]
			|	(qi::lit("(") >> "as" >> identifier >> sort >> ")")[qi::_val = px::bind(&QualifiedIdentifierParser::checkQualification, px::ref(*this), qi::_1, qi::_2)]
		;
	}
	
	Identifier checkQualification(const Identifier& identifier, const carl::Sort& sort) const {
		///@todo Check what can be checked here.
		return identifier;
	}
	
    IdentifierParser identifier;
    SortParser sort;
    qi::rule<Iterator, Identifier(), Skipper> main;
};

struct SortedVariableParser: public qi::grammar<Iterator, carl::Variable(), Skipper> {
	SortedVariableParser(): SortedVariableParser::base_type(main, "sorted variable") {
		main = (qi::lit("(") >> symbol >> sort >> ")")[qi::_val = px::bind(&SortedVariableParser::addVariable, px::ref(*this), qi::_1, qi::_2)];
	}
	carl::Variable addVariable(const std::string& _name, const carl::Sort& sort) {
		return carl::Variable::NO_VARIABLE;
	}
	SymbolParser symbol;
	SortParser sort;
	qi::rule<Iterator, carl::Variable(), Skipper> main;
};

struct TermParser: public qi::grammar<Iterator, Theories::TermType(), Skipper> {
	typedef VariantConverter<Theories::TermType> Converter;
	TermParser(Theories* theories): TermParser::base_type(main, "term"), theories(theories) {
		main =
				specconstant[qi::_val = px::bind(&Converter::template convert<Theories::ConstType>, &converter, qi::_1)]
			|	qualifiedidentifier[qi::_val = px::bind(&Theories::resolveSymbol, px::ref(*theories), qi::_1)]
			|	(qi::lit("(") >> termop >> ")")[qi::_val = qi::_1]
		;
		termop = 
				(qualifiedidentifier >> +main)[qi::_val = px::bind(&Theories::functionCall, px::ref(*theories), qi::_1, qi::_2)]
			|	(qi::lit("let") >> "(" >> +binding >> ")" >> main)[qi::_val = qi::_1]
			|	(qi::lit("forall") >> "(" >> +sortedvariable >> ")" >> main)[qi::_val = qi::_2]
			|	(qi::lit("exists") >> "(" >> +sortedvariable >> ")" >> main)[qi::_val = qi::_2]
			//|	(qi::lit("!") >> main >> +attribute)
		;
		binding = (qi::lit("(") >> symbol >> main >> ")")[px::bind(&Theories::addBinding, px::ref(*theories), qi::_1, qi::_2)];
	}

	Theories* theories;
	SymbolParser symbol;
	SpecConstantParser specconstant;
	QualifiedIdentifierParser qualifiedidentifier;
	SortedVariableParser sortedvariable;
	AttributeParser attribute;
	Converter converter;
	qi::rule<Iterator, Skipper> binding;
	qi::rule<Iterator, Theories::TermType(), Skipper> termop;
	qi::rule<Iterator, Theories::TermType(), Skipper> main;
};

}
}
