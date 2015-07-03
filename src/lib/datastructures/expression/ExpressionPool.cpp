#include "ExpressionPool.h"

#include "ExpressionContent.h"
#include "simplifier/Simplifier.h"

namespace smtrat {
namespace expression {

	const ExpressionContent* ExpressionPool::add(ExpressionContent* _ec) {
		std::cout << "Adding " << *_ec << std::endl;
		simplifier::Simplifier s;
		const ExpressionContent* simplified = s(_ec);
		if (simplified != nullptr) return simplified;
		auto it = mPool.find(_ec);
		if (it != mPool.end()) {
			delete _ec;
			return *it;
		}
		ExpressionContent* res = *mPool.insert(_ec).first;
		res->id = mNextID++;
		return res;
	}
		
	const ExpressionContent* ExpressionPool::create(carl::Variable::Arg var)
	{
		return add(new ExpressionContent(var));
	}
	const ExpressionContent* ExpressionPool::create(ITEType _type, Expression&& _if, Expression&& _then, Expression&& _else)
	{
		return new ExpressionContent(ITEExpression(_type, std::move(_if), std::move(_then), std::move(_else)));
	}
	const ExpressionContent* ExpressionPool::create(QuantifierType _type, std::vector<carl::Variable>&& _variables, Expression&& _expression)
	{
		return add(new ExpressionContent(QuantifierExpression(_type, std::move(_variables), std::move(_expression))));
	}
	const ExpressionContent* ExpressionPool::create(UnaryType _type, Expression&& _expression)
	{
		return add(new ExpressionContent(UnaryExpression(_type, std::move(_expression))));
	}
	const ExpressionContent* ExpressionPool::create(BinaryType _type, Expression&& _lhs, Expression&& _rhs)
	{
		return add(new ExpressionContent(BinaryExpression(_type, std::move(_lhs), std::move(_rhs))));
	}
	const ExpressionContent* ExpressionPool::create(NaryType _type, Expressions&& _expressions)
	{
		return add(new ExpressionContent(NaryExpression(_type, std::move(_expressions))));
	}
	const ExpressionContent* ExpressionPool::create(NaryType _type, const std::initializer_list<Expression>& _expressions)
	{
		return add(new ExpressionContent(NaryExpression(_type, _expressions)));
	}
}
}
