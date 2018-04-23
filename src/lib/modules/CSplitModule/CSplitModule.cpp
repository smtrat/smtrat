/**
 * @file CSplitModule.cpp
 * @author Ömer Sali <oemer.sali@rwth-aachen.de>
 *
 * @version 2018-04-04
 * Created on 2017-11-01.
 */

#include "CSplitModule.h"

namespace smtrat
{
	template<class Settings>
	CSplitModule<Settings>::CSplitModule(const ModuleInput* _formula, RuntimeSettings*, Conditionals& _conditionals, Manager* _manager)
		: Module( _formula, _conditionals, _manager )
		, mCheckedWithBackends(false)
#ifdef SMTRAT_DEVOPTION_Statistics
		, mStatistics(Settings::moduleName)
#endif
	{}
	
	template<class Settings>
	bool CSplitModule<Settings>::addCore(ModuleInput::const_iterator _subformula)
	{
		addReceivedSubformulaToPassedFormula(_subformula);
		const FormulaT& formula{_subformula->formula()};
		if (formula.getType() == carl::FormulaType::FALSE)
			mInfeasibleSubsets.push_back({formula});
		else if (formula.isBound())
		{
			mVariableBounds.addBound(formula, formula);
			const carl::Variable& variable{*formula.variables().begin()};
			auto expansionIter{mExpansions.firstFind(variable)};
			if (expansionIter == mExpansions.end())
				expansionIter = mExpansions.emplace(variable);
			expansionIter->mChangedBounds = true;
			if (mVariableBounds.isConflicting())
				mInfeasibleSubsets.emplace_back(mVariableBounds.getConflict());
		}
		else if (formula.getType() == carl::FormulaType::CONSTRAINT)
		{
			/// Normalize the left hand side of the constraint and turn the relation accordingly
			const ConstraintT& constraint{formula.constraint()};
			const Poly normalization{constraint.lhs().normalize()};
			carl::Relation relation{constraint.relation()};
			if (carl::isNegative(constraint.lhs().lcoeff()))
				relation = carl::turnAroundRelation(relation);
			
			auto linearizationIter{mLinearizations.firstFind(normalization)};
			if (linearizationIter == mLinearizations.end())
			{
				Poly discretization;
				std::vector<Purification *> purifications;
				bool hasRealVariables{false};
				for (TermT term : normalization)
				{
					if (!term.isConstant())
					{
						size_t realVariables{0};
						for (const auto& exponent : term.monomial()->exponents())
							if (exponent.first.type() == carl::VariableType::VT_REAL)
								realVariables += exponent.second;
						if (realVariables)
						{
							term.coeff() /= carl::pow(Rational(Settings::discrDenom), realVariables);
							hasRealVariables = true;
						}
						
						if (!term.isLinear())
						{
							Purification& purification{mPurifications[term.monomial()]};
							purifications.emplace_back(&purification);
							term = term.coeff()*purification.mSubstitutions[0];
						}
						else if (realVariables)
						{
							const carl::Variable variable{term.getSingleVariable()};
							auto expansionIter{mExpansions.firstFind(variable)};
							if (expansionIter == mExpansions.end())
								expansionIter = mExpansions.emplace(variable);
							term = term.coeff()*expansionIter->mQuotients[0];
						}
					}
					discretization += term;
				}
				linearizationIter = mLinearizations.emplace(normalization, std::move(discretization.normalize()), std::move(purifications), hasRealVariables);
			}
			Linearization& linearization{*linearizationIter};
			propagateFormula(FormulaT(linearization.mTarget, relation), true);
			if (linearization.mRelations.empty())
				for (Purification *purification : linearization.mPurifications)
					++purification->mUsage;	
			linearization.mRelations.emplace(relation);
			
			/// Check if the asserted relation trivially conflicts with other asserted relations
			switch (relation)
			{
				case carl::Relation::EQ:
					if (linearization.mRelations.count(carl::Relation::NEQ))
						mInfeasibleSubsets.push_back({
							FormulaT(normalization, carl::Relation::EQ),
							FormulaT(normalization, carl::Relation::NEQ)
						});
					if (linearization.mRelations.count(carl::Relation::LESS))
						mInfeasibleSubsets.push_back({
							FormulaT(normalization, carl::Relation::EQ),
							FormulaT(normalization, carl::Relation::LESS)
						});
					if (linearization.mRelations.count(carl::Relation::GREATER))
						mInfeasibleSubsets.push_back({
							FormulaT(normalization, carl::Relation::EQ),
							FormulaT(normalization, carl::Relation::GREATER)
						});
					break;
				case carl::Relation::NEQ:
					if (linearization.mRelations.count(carl::Relation::EQ))
						mInfeasibleSubsets.push_back({
							FormulaT(normalization, carl::Relation::NEQ),
							FormulaT(normalization, carl::Relation::EQ)
						});
					break;
				case carl::Relation::LESS:
					if (linearization.mRelations.count(carl::Relation::EQ))
						mInfeasibleSubsets.push_back({
							FormulaT(normalization, carl::Relation::LESS),
							FormulaT(normalization, carl::Relation::EQ)
						});
					if (linearization.mRelations.count(carl::Relation::GEQ))
						mInfeasibleSubsets.push_back({
							FormulaT(normalization, carl::Relation::LESS),
							FormulaT(normalization, carl::Relation::GEQ)
						});
				case carl::Relation::LEQ:
					if (linearization.mRelations.count(carl::Relation::GREATER))
						mInfeasibleSubsets.push_back({
							FormulaT(normalization, relation),
							FormulaT(normalization, carl::Relation::GREATER)
						});
					break;
				case carl::Relation::GREATER:
					if (linearization.mRelations.count(carl::Relation::EQ))
						mInfeasibleSubsets.push_back({
							FormulaT(normalization, carl::Relation::GREATER),
							FormulaT(normalization, carl::Relation::EQ)
						});
					if (linearization.mRelations.count(carl::Relation::LEQ))
						mInfeasibleSubsets.push_back({
							FormulaT(normalization, carl::Relation::GREATER),
							FormulaT(normalization, carl::Relation::LEQ)
						});
				case carl::Relation::GEQ:
					if (linearization.mRelations.count(carl::Relation::LESS))
						mInfeasibleSubsets.push_back({
							FormulaT(normalization, relation),
							FormulaT(normalization, carl::Relation::LESS)
						});
					break;
				default:
					assert(false);
			}
		}
		return mInfeasibleSubsets.empty();
	}
	
	template<class Settings>
	void CSplitModule<Settings>::removeCore(ModuleInput::const_iterator _subformula)
	{
		const FormulaT& formula{_subformula->formula()};	
		if (formula.isBound())
		{
			mVariableBounds.removeBound(formula, formula);
			mExpansions.firstFind(*formula.variables().begin())->mChangedBounds = true;
		}
		else if (formula.getType() == carl::FormulaType::CONSTRAINT)
		{
			/// Normalize the left hand side of the constraint and turn the relation accordingly
			const ConstraintT& constraint{formula.constraint()};
			const Poly normalization{constraint.lhs().normalize()};
			carl::Relation relation{constraint.relation()};
			if (carl::isNegative(constraint.lhs().lcoeff()))
				relation = carl::turnAroundRelation(relation);
			
			/// Retrieve the normalized constraint and mark the separator object as changed
			Linearization& linearization{*mLinearizations.firstFind(normalization)};
			propagateFormula(FormulaT(linearization.mTarget, relation), false);
			linearization.mRelations.erase(relation);
			if (linearization.mRelations.empty())
				for (Purification *purification : linearization.mPurifications)
					++purification->mUsage;	
		}
	}
	
	template<class Settings>
	void CSplitModule<Settings>::updateModel() const
	{
		if(!mModelComputed)
		{
			clearModel();
			if (mCheckedWithBackends)
			{
				getBackendsModel();
				excludeNotReceivedVariablesFromModel();
			}
			else
			{
				for (const Expansion& expansion : mExpansions)
					if (receivedVariable(expansion.mSource))
					{
						Rational value{mLRAModel.at(expansion.mTarget).asRational()};
						if (expansion.mSource.type() == carl::VariableType::VT_REAL)
							value /= Settings::discrDenom;
						mModel.emplace(expansion.mSource, value);
					}
			}
			mModelComputed = true;
		}
	}
	
	template<class Settings>
	Answer CSplitModule<Settings>::checkCore()
	{
		//std::cout << "CHECKCORE CALLED" << std::endl;
		/// Report unsatisfiability if the already found conflicts are still unresolved
		if (!mInfeasibleSubsets.empty())
			return Answer::UNSAT;
		
		if (rReceivedFormula().isConstraintConjunction())
		{
			mLRAModule.push();
			if (resetExpansions())
			{
				mCheckedWithBackends = false;
				for (size_t i = 1; i <= Settings::maxIter; ++i)
					if (mLRAModule.check(true) == Answer::SAT)
					{
						mLRAModel = mLRAModule.model();
						mLRAModule.pop();
						return Answer::SAT;
					}
					else
					{
						FormulaSetT conflict{mLRAModule.infeasibleSubsets()[0]};
						if (bloatDomains(conflict))
						{
							mLRAModule.pop();
							return analyzeConflict(conflict);
						}
					}
			}
			mLRAModule.pop();
		}
		
		/// Check the asserted formula with the backends
		mCheckedWithBackends = true;
		Answer answer{runBackends()};
		if (answer == Answer::UNSAT)
			getInfeasibleSubsets();
		return answer;
	}
	
	template<class Settings>
	bool CSplitModule<Settings>::resetExpansions()
	{
		// Update bounds and check for discretization conflict
		for (Expansion& expansion : mExpansions)
		{
			RationalInterval& maximalDomain{expansion.mMaximalDomain};
			if (expansion.mChangedBounds)
			{
				maximalDomain = mVariableBounds.getInterval(expansion.mSource);
				if (expansion.mSource.type() == carl::VariableType::VT_REAL)
					maximalDomain *= Rational(Settings::discrDenom);
				maximalDomain.integralPart_assign();
				if (expansion.mMaximalDomain.isUnbounded())
					expansion.mMaximalDomainSize = DomainSize::UNBOUNDED;
				else if (expansion.mMaximalDomain.diameter() > Settings::maxDomainSize)
					expansion.mMaximalDomainSize = DomainSize::LARGE;
				else
					expansion.mMaximalDomainSize = DomainSize::SMALL;
				expansion.mChangedBounds = false;
			}
			if (maximalDomain.isEmpty())
				return false;
			expansion.mActiveDomain = RationalInterval::emptyInterval();
			expansion.mPurifications.clear();
		}
		
		// Activate all asserted purifications bottom-up
		for (auto purificationIter = mPurifications.begin(); purificationIter != mPurifications.end(); ++purificationIter)
		{
			Purification& purification{purificationIter->second};
			if (purification.mUsage)
			{
				carl::Monomial::Arg monomial{purificationIter->first};
				
				// Find set of variables with maximal domain
				carl::Variables maxVariables;
				DomainSize maxDomainSize{SMALL};
				for (const auto& exponent : monomial->exponents())
				{
					const carl::Variable& variable{exponent.first};
					auto expansionIter{mExpansions.firstFind(variable)};
					if (expansionIter == mExpansions.end())
						expansionIter = mExpansions.emplace(variable);
					Expansion& expansion{*expansionIter};
					
					// Update maximal domain
					if (maxDomainSize <= expansion.mMaximalDomainSize)
					{
						if (maxDomainSize < expansion.mMaximalDomainSize)
						{
							maxVariables.clear();
							maxDomainSize = expansion.mMaximalDomainSize;
						}
						maxVariables.emplace(variable);
					}
				}
				
				// Find locally optimal reduction for monomial
				const auto isReducible = [&](const auto& purificationsEntry) {
					return purificationsEntry.second.mUsage
						&& monomial->divisible(purificationsEntry.first)
						&& std::any_of(
							maxVariables.begin(),
							maxVariables.end(),
							[&](const carl::Variable& variable) {
								return purificationsEntry.first->has(variable);
							}
						);
				};
				auto reductionIter{std::find_if(std::make_reverse_iterator(purificationIter), mPurifications.rend(), isReducible)};
				
				// Construct sequence of purifications
				carl::Variable reduction;
				if (reductionIter == mPurifications.rend())
				{
					const carl::Variable& maxVariable{*maxVariables.begin()};
					reduction = mExpansions.firstFind(maxVariable)->mQuotients[0];
					monomial = carl::createMonomial(maxVariable, 1);
				}
				else
				{
					reduction = reductionIter->second.mSubstitutions[0];
					monomial = reductionIter->first;
				}
				carl::Monomial::Arg guidance;
				purificationIter->first->divide(monomial, guidance);
				
				for (const auto& exponentPair : guidance->exponents())
				{
					const carl::Variable& variable{exponentPair.first};
					Expansion& expansion{*mExpansions.firstFind(variable)};
					for (carl::exponent exponent = 1; exponent <= exponentPair.second; ++exponent)
					{
						monomial = monomial*variable;
						Purification& purification{mPurifications[monomial]};
						purification.mReduction = reduction;
						reduction = purification.mSubstitutions[0];
						expansion.mPurifications.emplace(&purification);
					}
				}
			}
		}
		
		// Activate expansions that are used for case splits and deactivate them otherwise
		for (Expansion& expansion : mExpansions)
		{
			// Calculate the nucleus where the initial domain is located
			expansion.mNucleus = ZERO_RATIONAL;
			if (expansion.mMaximalDomain.lowerBoundType() != carl::BoundType::INFTY
				&& expansion.mNucleus < expansion.mMaximalDomain.lower())
				expansion.mNucleus = expansion.mMaximalDomain.lower();
			else if (expansion.mMaximalDomain.upperBoundType() != carl::BoundType::INFTY
				&& expansion.mNucleus > expansion.mMaximalDomain.upper())
				expansion.mNucleus = expansion.mMaximalDomain.upper();
			
			// Calculate corresponding active domain
			RationalInterval domain(0, 1);
			domain.mul_assign(Rational(Settings::initialRadius));
			domain.add_assign(expansion.mNucleus);
			domain.intersect_assign(expansion.mMaximalDomain);
			changeActiveDomain(expansion, std::move(domain));
		}
		
		return true;
	}
	
	template<class Settings>
	bool CSplitModule<Settings>::bloatDomains(const FormulaSetT& LRAConflict)
	{
		// Data structure for potential bloating candidates
		struct Candidate
		{
			Expansion& mExpansion;
			const Rational mDirection;
			const Rational mRadius;
			
			Candidate(Expansion& expansion, Rational&& direction, Rational&& radius)
				: mExpansion(expansion)
				, mDirection(std::move(direction))
				, mRadius(std::move(radius))
			{}
			
			bool operator<(const Candidate& rhs) const
			{
				if (mDirection*rhs.mDirection == ONE_RATIONAL)
					return mRadius < rhs.mRadius;
				else if (mDirection == ONE_RATIONAL)
					return mRadius < Rational(Settings::thresholdRadius);
				else
					return rhs.mRadius >= Rational(Settings::thresholdRadius);
			}
		};
		std::set<Candidate> candidates;
		
		for (const FormulaT& formula : LRAConflict)
			if (formula.isBound())
			{
				const ConstraintT& constraint{formula.constraint()};
				const carl::Variable& variable{*constraint.variables().begin()};
				auto expansionIter{mExpansions.secondFind(variable)};
				if (expansionIter != mExpansions.end())
				{
					Expansion& expansion{*expansionIter};
					Rational direction;
					if (constraint.isLowerBound()
						&& (expansion.mMaximalDomain.lowerBoundType() == carl::BoundType::INFTY
							|| expansion.mMaximalDomain.lower() < expansion.mActiveDomain.lower()))
						direction = MINUS_ONE_RATIONAL;
					else if (constraint.isUpperBound()
						&& (expansion.mMaximalDomain.upperBoundType() == carl::BoundType::INFTY
							|| expansion.mMaximalDomain.upper() > expansion.mActiveDomain.upper()))
						direction  = ONE_RATIONAL;
					if (direction != ZERO_RATIONAL)
					{
						Rational radius{(direction*(expansion.mActiveDomain-expansion.mNucleus)).upper()};
						if (radius <= Settings::maximalRadius)
						{
							candidates.emplace(expansion, std::move(direction), std::move(radius));
							if (candidates.size() > Settings::maxBloatedDomains)
								candidates.erase(std::prev(candidates.end()));
						}
					}
				}
			}
		
		for (const Candidate& candidate : candidates)
		{
			RationalInterval domain;
			if (candidate.mRadius <= Settings::thresholdRadius)
				domain = RationalInterval(0, Settings::radiusIncrement);
			else if (candidate.mExpansion.mPurifications.empty())
				domain = RationalInterval(0, carl::BoundType::WEAK, 0, carl::BoundType::INFTY);
			else
				domain = RationalInterval(0, candidate.mRadius);
			domain.mul_assign(candidate.mDirection);
			domain.add_assign(candidate.mExpansion.mActiveDomain);
			domain.intersect_assign(candidate.mExpansion.mMaximalDomain);
			changeActiveDomain(candidate.mExpansion, std::move(domain));
		}
		
		return candidates.empty();
	}
	
	template<class Settings>
	Answer CSplitModule<Settings>::analyzeConflict(const FormulaSetT& LRAConflict)
	{
		FormulaSetT infeasibleSubset;
		for (const FormulaT& formula : LRAConflict)
		{
			if (formula.isBound())
			{
				auto expansionIter{mExpansions.secondFind(*formula.variables().begin())};
				if (expansionIter != mExpansions.end())
				{
					const Expansion& expansion{*expansionIter};
					if (expansion.mSource.type() == carl::VariableType::VT_REAL
						|| expansion.mMaximalDomain != expansion.mActiveDomain)
						return Answer::UNKNOWN;
					else
					{
						FormulaSetT boundOrigins{mVariableBounds.getOriginSetOfBounds(expansion.mSource)};
						infeasibleSubset.insert(boundOrigins.begin(), boundOrigins.end());
					}
				}
			}
			else if (formula.getType() == carl::FormulaType::CONSTRAINT)
			{
				const ConstraintT& constraint{formula.constraint()};
				auto linearizationIter{mLinearizations.secondFind(constraint.lhs().normalize())};
				if (linearizationIter != mLinearizations.end())
				{
					const Linearization& linearization{*linearizationIter};
					if (linearization.mHasRealVariables)
						return Answer::UNKNOWN;
					else
					{
						carl::Relation relation{constraint.relation()};
						if (carl::isNegative(constraint.lhs().lcoeff()))
							relation = carl::turnAroundRelation(relation);
						infeasibleSubset.emplace(linearization.mSource, relation);
					}
				}
			}
		}
		mInfeasibleSubsets.emplace_back(std::move(infeasibleSubset));
		return Answer::UNSAT;
	}
	
	
	template<class Settings>
	void CSplitModule<Settings>::changeActiveDomain(Expansion& expansion, RationalInterval&& domain)
	{
		RationalInterval activeDomain{move(expansion.mActiveDomain)};
		expansion.mActiveDomain = domain;
		
		// Update variable bounds
		if (!activeDomain.isEmpty())
		{
			if (activeDomain.lowerBoundType() != carl::BoundType::INFTY
				&& (domain.lowerBoundType() == carl::BoundType::INFTY
					|| domain.lower() != activeDomain.lower()
					|| domain.isEmpty()))
				propagateFormula(FormulaT(expansion.mQuotients[0]-Poly(activeDomain.lower()), carl::Relation::GEQ), false);
			if (activeDomain.upperBoundType() != carl::BoundType::INFTY
				&& (domain.upperBoundType() == carl::BoundType::INFTY
					|| domain.upper() != activeDomain.upper()
					|| domain.isEmpty()))
				propagateFormula(FormulaT(expansion.mQuotients[0]-Poly(activeDomain.upper()), carl::Relation::LEQ), false);
		}
		if (!domain.isEmpty())
		{
			if (domain.lowerBoundType() != carl::BoundType::INFTY
				&& (activeDomain.lowerBoundType() == carl::BoundType::INFTY
					|| activeDomain.lower() != domain.lower()
					|| activeDomain.isEmpty()))
				propagateFormula(FormulaT(expansion.mQuotients[0]-Poly(domain.lower()), carl::Relation::GEQ), true);
			if (domain.upperBoundType() != carl::BoundType::INFTY
				&& (activeDomain.upperBoundType() == carl::BoundType::INFTY
					|| activeDomain.upper() != domain.upper()
					|| activeDomain.isEmpty()))
				propagateFormula(FormulaT(expansion.mQuotients[0]-Poly(domain.upper()), carl::Relation::LEQ), true);
		}
		
		// Check if digits need to be encoded
		if (expansion.mPurifications.empty())
		{
			activeDomain = RationalInterval::emptyInterval();
			domain = RationalInterval::emptyInterval();
		}
		
		// Update case splits
		for (size_t i = 0; activeDomain != domain; ++i)
		{
			if (activeDomain.diameter() <= Settings::maxDomainSize)
				if (domain.diameter() <= Settings::maxDomainSize)
				{
					// Update existing linear encoding
					RationalInterval intervalA, intervalB;
					bool assertA{true}, assertB{false};
					if (!domain.isEmpty())
						assertB = domain.difference(activeDomain, intervalA, intervalB);
					if (!assertB && !activeDomain.isEmpty())
						assertA = !activeDomain.difference(domain, intervalB, intervalA);
					intervalA.integralPart_assign();
					intervalB.integralPart_assign();
					for (Purification *purification : expansion.mPurifications)
					{
						propagateLinearCaseSplits(expansion, *purification, intervalA, i, assertA);
						propagateLinearCaseSplits(expansion, *purification, intervalB, i, assertB);
					}
				}
				else
				{
					// Switch from linear to logarithmic encoding
					if (expansion.mQuotients.size() <= i+1)
					{
						expansion.mQuotients.push_back(carl::freshIntegerVariable());
						expansion.mRemainders.push_back(carl::freshIntegerVariable());
					}
					for (Purification *purification : expansion.mPurifications)
					{
						while (purification->mSubstitutions.size() <= i+1)
							purification->mSubstitutions.push_back(carl::freshIntegerVariable());
						propagateLinearCaseSplits(expansion, *purification, activeDomain, i, false);
						propagateLogarithmicCaseSplits(expansion, *purification, i, true);
					}
					propagateFormula(FormulaT(Poly(expansion.mQuotients[i])-Poly(Settings::expansionBase)*Poly(expansion.mQuotients[i+1])-Poly(expansion.mRemainders[i]), carl::Relation::EQ), true);
					propagateFormula(FormulaT(Poly(expansion.mRemainders[i]), carl::Relation::GEQ), true);
					propagateFormula(FormulaT(Poly(expansion.mRemainders[i])-Poly(Settings::expansionBase-1), carl::Relation::LEQ), true);
				}
			else if (domain.diameter() <= Settings::maxDomainSize)
			{
				// Switch from logarithmic to linear encoding
				for (Purification *purification : expansion.mPurifications)
				{
					propagateLogarithmicCaseSplits(expansion, *purification, i, false);
					propagateLinearCaseSplits(expansion, *purification, domain, i, true);
				}
				propagateFormula(FormulaT(Poly(expansion.mQuotients[i])-Poly(Settings::expansionBase)*Poly(expansion.mQuotients[i+1])-Poly(expansion.mRemainders[i]), carl::Relation::EQ), false);
				propagateFormula(FormulaT(Poly(expansion.mRemainders[i]), carl::Relation::GEQ), false);
				propagateFormula(FormulaT(Poly(expansion.mRemainders[i])-Poly(Settings::expansionBase-1), carl::Relation::LEQ), false);
			}
			
			// Calculate domain of next digit
			if (!activeDomain.isEmpty())
				if (activeDomain.diameter() <= Settings::maxDomainSize)
					activeDomain = RationalInterval::emptyInterval();
				else
					activeDomain = carl::floor(activeDomain/Rational(Settings::expansionBase));
			if (!domain.isEmpty())
				if (domain.diameter() <= Settings::maxDomainSize)
					domain = RationalInterval::emptyInterval();
				else
					domain = carl::floor(domain/Rational(Settings::expansionBase));
			
			// Update variable bounds
			if (!activeDomain.isEmpty())
			{
				if (domain.isEmpty() || domain.lower() != activeDomain.lower())
					propagateFormula(FormulaT(expansion.mQuotients[i+1]-Poly(activeDomain.lower()), carl::Relation::GEQ), false);
				if (domain.isEmpty() || domain.upper() != activeDomain.upper())
					propagateFormula(FormulaT(expansion.mQuotients[i+1]-Poly(activeDomain.upper()), carl::Relation::LEQ), false);
			}
			if (!domain.isEmpty())
			{
				if (activeDomain.isEmpty() || activeDomain.lower() != domain.lower())
					propagateFormula(FormulaT(expansion.mQuotients[i+1]-Poly(domain.lower()), carl::Relation::GEQ), true);
				if (activeDomain.isEmpty() || activeDomain.upper() != domain.upper())
					propagateFormula(FormulaT(expansion.mQuotients[i+1]-Poly(domain.upper()), carl::Relation::LEQ), true);
			}
		}
	}
	
	template<class Settings>
	inline void CSplitModule<Settings>::propagateLinearCaseSplits(const Expansion& expansion, const Purification& purification, const RationalInterval& interval, size_t i, bool assert)
	{
		if (!interval.isEmpty())
			for (Rational alpha = interval.lower(); alpha <= interval.upper(); ++alpha)
				propagateFormula(
					FormulaT(
						carl::FormulaType::IMPLIES,
						FormulaT(Poly(expansion.mQuotients[i])-Poly(alpha), carl::Relation::EQ),
						FormulaT(Poly(purification.mSubstitutions[i])-Poly(alpha)*Poly(purification.mReduction), carl::Relation::EQ)
					),
					assert
				);
	}
	
	template<class Settings>
	inline void CSplitModule<Settings>::propagateLogarithmicCaseSplits(const Expansion& expansion, const Purification& purification, size_t i, bool assert)
	{
		for (Rational alpha = 0; alpha < Settings::expansionBase; ++alpha)
			propagateFormula(
				FormulaT(
					carl::FormulaType::IMPLIES,
					FormulaT(Poly(expansion.mRemainders[i])-Poly(alpha), carl::Relation::EQ),
					FormulaT(Poly(purification.mSubstitutions[i])-Poly(Settings::expansionBase)*Poly(purification.mSubstitutions[i+1])-Poly(alpha)*Poly(purification.mReduction), carl::Relation::EQ)
				),
				assert
			);
	}

	
	
	
	
	
	/*
	template<class Settings>
	void CSplitModule<Settings>::changeActiveDomain(Expansion& expansion, RationalInterval&& domain)
	{
		std::cout << "CHANGEACTIVEDOMAIN CALLED" << std::endl;
		// Update variable bounds
		if (!expansion.mActiveDomain.isEmpty())
		{
			if (expansion.mActiveDomain.lowerBoundType() != carl::BoundType::INFTY
				&& (domain.lowerBoundType() == carl::BoundType::INFTY
					|| domain.lower() != expansion.mActiveDomain.lower()
					|| domain.isEmpty()))
				propagateFormula(FormulaT(expansion.mQuotients[0]-Poly(expansion.mActiveDomain.lower()), carl::Relation::GEQ), false);
			if (expansion.mActiveDomain.upperBoundType() != carl::BoundType::INFTY
				&& (domain.upperBoundType() == carl::BoundType::INFTY
					|| domain.upper() != expansion.mActiveDomain.upper()
					|| domain.isEmpty()))
				propagateFormula(FormulaT(expansion.mQuotients[0]-Poly(expansion.mActiveDomain.upper()), carl::Relation::LEQ), false);
		}
		if (!domain.isEmpty())
		{
			if (domain.lowerBoundType() != carl::BoundType::INFTY
				&& (expansion.mActiveDomain.lowerBoundType() == carl::BoundType::INFTY
					|| expansion.mActiveDomain.lower() != domain.lower()
					|| expansion.mActiveDomain.isEmpty()))
				propagateFormula(FormulaT(expansion.mQuotients[0]-Poly(domain.lower()), carl::Relation::GEQ), true);
			if (domain.upperBoundType() != carl::BoundType::INFTY
				&& (expansion.mActiveDomain.upperBoundType() == carl::BoundType::INFTY
					|| expansion.mActiveDomain.upper() != domain.upper()
					|| expansion.mActiveDomain.isEmpty()))
				propagateFormula(FormulaT(expansion.mQuotients[0]-Poly(domain.upper()), carl::Relation::LEQ), true);
		}
		
		// Update case splits
		if (expansion.mPurifications.empty())
			expansion.mActiveDomain = std::move(domain);
		else
		{
			RationalInterval activeDomain{std::move(expansion.mActiveDomain)};
			expansion.mActiveDomain = domain;
			
			for (size_t i = 0; activeDomain != domain; ++i)
			{
				if (domain.diameter() <= Settings::maxDomainSize)
				{
					// Update existing linear encoding
					for (const Purification *purification : expansion.mPurifications)
					{
						for (Rational alpha = domain.lower(); alpha < activeDomain.lower(); ++alpha)
							propagateFormula(
								FormulaT(
									carl::FormulaType::IMPLIES,
									FormulaT(Poly(expansion.mQuotients[i])-Poly(alpha), carl::Relation::EQ),
									FormulaT(Poly(purification->mSubstitutions[i])-Poly(alpha)*Poly(purification->mReduction), carl::Relation::EQ)
								),
								true
							);
						for (Rational alpha = activeDomain.upper()+ONE_RATIONAL; alpha <= domain.upper(); ++alpha)
							propagateFormula(
								FormulaT(
									carl::FormulaType::IMPLIES,
									FormulaT(Poly(expansion.mQuotients[i])-Poly(alpha), carl::Relation::EQ),
									FormulaT(Poly(purification->mSubstitutions[i])-Poly(alpha)*Poly(purification->mReduction), carl::Relation::EQ)
								),
								true
							);
					}
				}
				else if (activeDomain.diameter() <= Settings::maxDomainSize)
				{
					// Switch from linear to logarithmic encoding
					if (expansion.mQuotients.size() <= i+1)
					{
						expansion.mQuotients.emplace_back(carl::freshIntegerVariable());
						expansion.mRemainders.emplace_back(carl::freshIntegerVariable());
					}
					for (Purification *purification : expansion.mPurifications)
					{
						while (purification->mSubstitutions.size() <= i+1)
							purification->mSubstitutions.emplace_back(carl::freshIntegerVariable());/////////////////////////////////////////////////////////////
						for (Rational alpha = activeDomain.lower(); alpha <= activeDomain.upper(); ++alpha)
							propagateFormula(
								FormulaT(
									carl::FormulaType::IMPLIES,
									FormulaT(Poly(expansion.mQuotients[i])-Poly(alpha), carl::Relation::EQ),
									FormulaT(Poly(purification->mSubstitutions[i])-Poly(alpha)*Poly(purification->mReduction), carl::Relation::EQ)
								),
								false
							);
						for (Rational alpha = ZERO_RATIONAL; alpha < Settings::expansionBase; ++alpha)
							propagateFormula(
								FormulaT(
									carl::FormulaType::IMPLIES,
									FormulaT(Poly(expansion.mRemainders[i])-Poly(alpha), carl::Relation::EQ),
									FormulaT(Poly(purification->mSubstitutions[i])-Poly(Settings::expansionBase)*Poly(purification->mSubstitutions[i+1])-Poly(alpha)*Poly(purification->mReduction), carl::Relation::EQ)
								),
								true
							);
					}
					propagateFormula(FormulaT(Poly(expansion.mQuotients[i])-Poly(Settings::expansionBase)*Poly(expansion.mQuotients[i+1])-Poly(expansion.mRemainders[i]), carl::Relation::EQ), true);
					propagateFormula(FormulaT(Poly(expansion.mRemainders[i]), carl::Relation::GEQ), true);
					propagateFormula(FormulaT(Poly(expansion.mRemainders[i])-Poly(Settings::expansionBase-1), carl::Relation::LEQ), true);
				}
				
				// Calculate domain of next digit
				if (!activeDomain.isEmpty())
					if (activeDomain.diameter() <= Settings::maxDomainSize)
						activeDomain = RationalInterval::emptyInterval();
					else
						activeDomain = carl::floor(activeDomain/Rational(Settings::expansionBase));
				if (!domain.isEmpty())
					if (domain.diameter() <= Settings::maxDomainSize)
						domain = RationalInterval::emptyInterval();
					else
						domain = carl::floor(domain/Rational(Settings::expansionBase));
				
				// Update variable bounds of next digit
				if (!activeDomain.isEmpty())
				{
					if (domain.isEmpty() || domain.lower() != activeDomain.lower())
						propagateFormula(FormulaT(expansion.mQuotients[i+1]-Poly(activeDomain.lower()), carl::Relation::GEQ), false);
					if (domain.isEmpty() || domain.upper() != activeDomain.upper())
						propagateFormula(FormulaT(expansion.mQuotients[i+1]-Poly(activeDomain.upper()), carl::Relation::LEQ), false);
				}
				if (!domain.isEmpty())
				{
					if (activeDomain.isEmpty() || activeDomain.lower() != domain.lower())
						propagateFormula(FormulaT(expansion.mQuotients[i+1]-Poly(domain.lower()), carl::Relation::GEQ), true);
					if (activeDomain.isEmpty() || activeDomain.upper() != domain.upper())
						propagateFormula(FormulaT(expansion.mQuotients[i+1]-Poly(domain.upper()), carl::Relation::LEQ), true);
				}
			}
		}
	}*/
	
	template<class Settings>
	inline void CSplitModule<Settings>::propagateFormula(const FormulaT& formula, bool assert)
	{
		if (assert)
			mLRAModule.add(formula);
		else
			mLRAModule.remove(std::find(mLRAModule.formulaBegin(), mLRAModule.formulaEnd(), formula));
	}
}

#include "Instantiation.h"
