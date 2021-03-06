#pragma once

#include <carl/ran/real_roots.h>
#include <carl/core/polynomialfunctions/Substitution.h>
#include <carl-model/evaluation/ModelEvaluation.h>
#include <smtrat-common/model.h>
#include <smtrat-common/smtrat-common.h>

namespace smtrat::lve {

Rational evaluate(carl::Variable v, const Poly& p, const Rational& r) {
	assert(carl::substitute(p, v, Poly(r)).isConstant());
	return carl::substitute(p, v, Poly(r)).constantPart();
}
carl::Sign sgn(carl::Variable v, const Poly& p, const carl::RealAlgebraicNumber<Rational>& r) {
	Model m;
	m.assign(v, r);
	auto res = carl::model::evaluate(p, m);
	if (res.isRational()) {
		return carl::sgn(res.asRational());
	} else if (res.isRAN()) {
		return carl::sgn(res.asRAN());
	}
	assert(false);
	return carl::Sign::ZERO;
}

std::optional<ModelValue> get_root(carl::Variable v, const Poly& p) {
	auto res = carl::real_roots(carl::to_univariate_polynomial(p, v));
	if (!res.is_univariate() || res.roots().empty()) {
		return std::nullopt;
	} else {
		return res.roots().front();
	}
}

ModelValue get_non_root(carl::Variable v, const Poly& p) {
	Rational r = 0;
	while (carl::isZero(evaluate(v, p, r))) {
		r += Rational(1);
	}
	return r;
}

std::optional<ModelValue> get_value_for_sgn(carl::Variable v, const Poly& p, carl::Sign sign) {
	carl::RealAlgebraicNumber<Rational> test;
	if (sgn(v, p, test) == sign) {
		return ModelValue(test);
	}
	auto res = carl::real_roots(carl::to_univariate_polynomial(p, v));
	if (!res.is_univariate() || res.roots().empty()) {
		return std::nullopt;
	}
	test = sample_below(res.roots().front());
	if (sgn(v, p, test) == sign) {
		return ModelValue(test);
	}
	test = sample_above(res.roots().back());
	if (sgn(v, p, test) == sign) {
		return ModelValue(test);
	}
	for (std::size_t i = 0; i < res.roots().size() - 1; ++i) {
		test = sample_between(res.roots()[i], res.roots()[i+1]);
		if (sgn(v, p, test) == sign) {
			return ModelValue(test);
		}	
	}
	return std::nullopt;
}

carl::Sign sgn_of_invariant_poly(carl::Variable v, const Poly& p) {
	return carl::sgn(evaluate(v, p, 0));
}

}