#pragma once

#include "Dependencies.h"

#include <carl/interval/Interval.h>
#include <carl/interval/Contractor.h>
#include <carl/interval/sampling.h>
#include <smtrat-common/smtrat-common.h>
#include <smtrat-mcsat/utils/Bookkeeping.h>

namespace smtrat {
namespace mcsat {
namespace icp {

struct QueueEntry {
	double priority;
	carl::contractor::Contractor<FormulaT, Poly> contractor;
};

class IntervalPropagation {	
private:
	const Model& mModel;
	std::map<carl::Variable, carl::Interval<double>> mBox;
	std::vector<QueueEntry> mContractors;

	Dependencies mDependencies;

	static constexpr double weight_age = 0.5;
	static constexpr double threshold_priority = 0.1;
	static constexpr double threshold_width = 0.1;

	bool has_contractor_above_threshold() const {
		return std::any_of(mContractors.begin(), mContractors.end(),
			[](const auto& qe) { return qe.priority > threshold_priority; }
		);
	}
	bool has_interval_below_threshold() const {
		return std::any_of(mBox.begin(), mBox.end(),
			[](const auto& dim) {
				return (!dim.second.isUnbounded()) && dim.second.diameter() < threshold_width; 
			}
		);
	}
	/**
	 * Samples a rational with a small representation size.
	 * If side < 0: from (|side+1|*lower + upper)/|side|
	 * If side > 0: from (lower + |side-1|*upper)/|side|
	 */
	Rational sample_small_rational(const Rational& lower, const Rational& upper, int side) const {
		int absside = std::abs(side);
		carl::Interval<Rational> i(lower, upper);
		SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Sampling from " << i);
		if (side < 0) {
			i.set(
				i.lower() * absside,
				i.lower() * (absside - 1) + i.upper()
			);
		} else {
			i.set(
				i.lower() + i.upper() * (absside - 1),
				i.upper() * absside
			);
		}
		SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "from " << i);
		auto res = carl::sample_stern_brocot(i / Rational(absside));
		SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "-> " << res);
		return res;
	}

	double update_model(carl::Variable v, const std::vector<carl::Interval<double>>& intervals) {
		auto it = mBox.find(v);
		assert(it != mBox.end());
		auto& cur = it->second;
		auto old = cur;
		if (cur.lowerBound() < intervals.front().lowerBound()) {
			cur = carl::Interval<double>(intervals.front().lower(), intervals.front().lowerBoundType(), cur.upper(), cur.upperBoundType());
		}
		if (intervals.back().upperBound() < cur.upperBound()) {
			cur = carl::Interval<double>(cur.lower(), cur.lowerBoundType(), intervals.back().upper(), intervals.back().upperBoundType());
		}
		SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", old << " -> " << cur);
		if (old.isInfinite()) {
			if (cur.isInfinite()) {
				SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "still infinite");
				return 0.0;
			} else {
				SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "no longer infinite");
				return 1.0;
			}
		} else if (old.isUnbounded()) {
			assert(!cur.isInfinite());
			if (cur.isUnbounded()) {
				SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "still unbounded");
				if (old.lower() < cur.lower() || cur.upper() < old.upper()) {
					return threshold_priority / 2;
				} else {
					return 0.0;
				}
			} else {
				SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "no longer unbounded");
				return 1.0;
			}
		} else {
			SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "reduced size");
			auto size = old.diameter();
			return (size - cur.diameter()) / size;
		}
	}

	std::optional<FormulaT> find_excluded_interval(carl::Variable v, const std::vector<carl::Interval<double>>& admissible) const {
		if (mModel.find(v) == mModel.end()) {
			return std::nullopt;
		}
		SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Checking whether the current model is admissible...");
		auto value = mModel.evaluated(v);
		if (value.isRational()) {
			Rational val = value.asRational();
			SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "model is " << val);
			std::optional<Rational> lower;
			std::optional<Rational> upper;
			for (std::size_t i = 0; i < admissible.size(); ++i) {
				const auto& a = admissible[i];
				carl::Interval<Rational> cur(
					carl::rationalize<Rational>(a.lower()),
					a.lowerBoundType(),
					carl::rationalize<Rational>(a.upper()),
					a.upperBoundType()
				);
				SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "cur is " << cur);
				if (val < cur) {
					upper = cur.lower();
					SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "cur is above " << val);
					break;
				}
				if (cur.contains(val)) {
					SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "cur contains " << val);
					return std::nullopt;
				}
				lower = cur.upper();
			}
			if (lower || upper) {
				FormulasT subs;
				if (lower) {
					lower = sample_small_rational(*lower, val, -100);
					subs.emplace_back(v - *lower, carl::Relation::LEQ);
				}
				if (upper) {
					upper = sample_small_rational(val, *upper, 100);
					subs.emplace_back(v - *upper, carl::Relation::GEQ);
				}
				return FormulaT(carl::FormulaType::OR, std::move(subs));
			}
		}
		return std::nullopt;
	}

	auto construct_direct_conflict(carl::Variable v) const {
		auto constraints = mDependencies.get(v, true);
		SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Constructing not " << constraints);
		return FormulaT(carl::FormulaType::OR, std::move(constraints));
	}
	auto construct_interval_conflict(carl::Variable v, const FormulaT& excluded) const {
		auto constraints = mDependencies.get(v, true);
		SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Constructing " << constraints << " => " << excluded);
		constraints.emplace_back(excluded);
		return FormulaT(carl::FormulaType::OR, std::move(constraints));
	}

public:
	IntervalPropagation(const std::vector<carl::Variable>& vars, const std::vector<FormulaT>& constraints, const Model& model): mModel(model) {
		for (auto v: vars) {
			mBox.emplace(v, carl::Interval<double>(0, carl::BoundType::INFTY, 0, carl::BoundType::INFTY));
		}
		for (const auto& c: constraints) {
			if (c.constraint().relation() == carl::Relation::NEQ) continue;
			for (const auto& v: c.variables()) {
				mContractors.emplace_back(QueueEntry {1.0, carl::contractor::Contractor<FormulaT, Poly>(c, c.constraint(), v)});
			}
		}
	}

	std::optional<FormulaT> execute() {
		SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Current model: " << mBox);
		while (true) {
			if (!has_contractor_above_threshold()) {
				SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "No contraction candidate above the threshold, terminating.");
				break;
			}
			if (has_interval_below_threshold()) {
				SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "The box is below the threshold, terminating.");
				break;
			}
			bool contracted = false;
			for (auto& c: mContractors) {
				SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "***************");
				SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Contracting with " << c.contractor.var());
				auto res = c.contractor.contract(mBox);
				if (res.empty()) {
					mDependencies.add(c.contractor);
					SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Contracted to empty interval, conflict for " << c.contractor.var());
					return construct_direct_conflict(c.contractor.var());
				} else {
					auto excluded = find_excluded_interval(c.contractor.var(), res);
					if (excluded) {
						mDependencies.add(c.contractor);
						SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Contracted to exclude current model, conflict for " << c.contractor.var());
						return construct_interval_conflict(c.contractor.var(), *excluded);
					} else {
						SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Contracted " << c.contractor.var() << " to " << res);
						double factor = update_model(c.contractor.var(), res);
						SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Contraction factor: " << factor);
						if (factor > 0) {
							contracted = true;
							mDependencies.add(c.contractor);
						}
						c.priority = weight_age * c.priority + factor * (1 - c.priority);
						SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "New priority: " << c.priority);
					}
				}
			}
			SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "Current model: " << mBox);
			if (!contracted) {
				SMTRAT_LOG_DEBUG("smtrat.mcsat.icp", "No contraction candidate worked, reached a fixpoint.");
				break;
			}
		}
		return std::nullopt;
	}
};

}
}
} // smtrat
