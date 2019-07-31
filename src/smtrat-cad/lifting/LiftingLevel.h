#pragma once

#include "../common.h"
#include "CADInterval.h"

#include <carl/interval/sampling.h>
#include <carl/interval/Interval.h>
#include <carl/core/rootfinder/RootFinder.h>

#include "Sample.h"

namespace smtrat {
namespace cad {
	template<typename Settings>
	class LiftingLevel {
	private:
		//@todo check which of these are used and that all are initialized
		std::vector<ConstraintT> mConstraints = std::vector<ConstraintT>();				/**< constraints */
		carl::Variable currVar						= carl::Variable();					/**< this level is considered univariate on given variable */
		std::vector<carl::Variable> mVariables 		= std::vector<carl::Variable>();	/**< variables, ordered */
		Sample curSample	 						= Sample();							/**< current sample to be checked */
		std::vector<CADInterval> intervals 			= std::vector<CADInterval>();		/**< unsat intervals */
		std::set<RAN> levelintervals 				= set<RAN>();						/**< all bounds of unsat intervals, ordered */
		bool levelintervalminf			 			= false;							/**< whether -inf is a bound */
		bool levelintervalpinf 						= false;							/**< whether +inf is a bound */
		
		//@todo check all fcnts and add doxygen conform comments

		/** gets the current dimension (#variables)
		 * @returns current dimension
		 */
		std::size_t dim() const {
			return mVariables.size();
		}

		/** check whether the given value is in the list of unsat intervals
		 * @returns true if value is in unsat interval
		 */
		bool isInUnsatInterval(RAN val) {
			for(auto inter : intervals) {
				if(inter.contains(val)) {
					return true;
				}
			}
			return false;
		}

		/** 
		 * @brief Gives the lowest bound followed by an unexplored interval
		 * 
		 * Goes through the unsat intervals starting from -inf,
		 * if -inf is not a bound yet it is determined to be the first "upper" bound 
		 * to encode that there is an open interval smaller than any known bound.
		 * Else the first bound not followed by another unsat interval is returned.
		 * 
		 * @returns bool (1st value of tuple) true iff a bound was found
		 * @returns RAN (2nd value of tuple) bound iff one was found, 0 otherwise
		 * @returns bool (3rd value of tuple) true iff the bound is open, otherwise it is closed
		 * @returns bool (4th value of tuple) true iff -inf is is not a bound yet
		 * @returns set<CADInterval> (5th value of tuple) contains intervals of covering iff no bound was found, else empty
		 * 
		 * @note The output (true, 0, false, true) stands for an unexplored
		 * interval before the first recorded bound
		 */
		std::tuple<bool, RAN, bool, bool, std::set<CADInterval>> getLowestUpperBound() {
			// if (-inf, +inf) is included, return false
			if(isSingletonCover()) {
				return std::make_tuple(false, (RAN) 0, false, false);
			}
			// if -inf is no bound, there is some unexplored interval before the first recorded bound
			if(levelintervalminf == false) {
				return std::make_tuple(true, (RAN) 0, false, true);
			}

			std::set<CADInterval> cover;

			// get an interval with -inf bound, store its higher bound
			RAN highestbound;
			bool boundopen;
			for(auto inter : intervals) {
				if(inter.getLowerBoundType() == CADInterval::CADBoundType::INF) {
					// note: the higher bound cannot be +inf as there is no singleton cover
					highestbound = inter.getUpper();
					boundopen = (inter.getUpperBoundType() == CADInterval::CADBoundType::OPEN) ? true : false;
					cover.insert(inter);
					break;
				}
			}
			// iteratively check for highest reachable bound
			bool stop = false;
			while(!stop) {
				bool updated = false;
				for(auto inter : intervals) {
					updated = false;
					// if the upper bound is the highest bound but is included only update bound type
					if(highestbound == inter.getUpper() && boundopen && inter.getUpperBoundType() == CADInterval::CADBoundType::CLOSED) {
						boundopen = false;
						cover.insert(inter);
					}
					// update highest bound if the upper bound is not equal to the current highest bound
					else if(!(highestbound == inter.getUpper() && 
						((boundopen && inter.getUpperBoundType() == CADInterval::CADBoundType::OPEN) || 
						 (!boundopen && inter.getUpperBoundType() == CADInterval::CADBoundType::CLOSED)))) {
						// and is contained in the interval or is bordered by the lower bound of the interval
						if(inter.contains(highestbound) ||
							(highestbound == inter.getLower() && 
							((boundopen && inter.getLowerBoundType() == CADInterval::CADBoundType::CLOSED) || 
								(!boundopen && inter.getLowerBoundType() == CADInterval::CADBoundType::OPEN)))) {
							
							cover.insert(inter);
							if(inter.getUpperBoundType() == CADInterval::CADBoundType::INF) {
								// an unset cover was found
								return std::make_tuple(false, (RAN) 0, false, false, cover);
							}
							// update to next higher bound
							highestbound = inter.getUpper();
							boundopen = (inter.getUpperBoundType() == CADInterval::CADBoundType::OPEN) ? true : false;
							updated = true;
						}
					}
				}
				// if the highest bound could not be updated (& was not +inf), break
				if(!updated) {
					stop = true;
				}
			}
			return std::make_tuple(true, highestbound, boundopen, false, std::set<CADInterval>());
		}

		/**
		 * Calculates the intervals between the real roots of the given set of constraints
		 * 
		 * (Paper Alg. 1, l.9-11)
		 */
		std::set<CADInterval> calcIntervalsFromPolys(
			std::vector<ConstraintT> conss, 
			EvaluationMap evalmap) {
			std::set<CADInterval> inters;
			for (const auto& c: conss) {
				// find real roots of every constraint corresponding to current var
				auto r = carl::rootfinder::realRoots(c.lhs().toUnivariatePolynomial(currVar), evalmap);
				std::sort(r.begin(), r.end()); // roots have to be ordered
				
				// go through roots to build region intervals and add them to the lifting level
				std::vector<RAN>::iterator it;
				for (it = r.begin(); it != r.end(); it++) {
					// add closed point interval for each root
					inters.insert( CADInterval(*it, c) );

					// add inf intervals if appropriate
					if (it == r.begin()) // add (-inf, x)
						inters.insert( CADInterval(0, *it, CADInterval::CADBoundType::INF, CADInterval::CADBoundType::OPEN, c) );
					else if (it == r.rbegin().base()) // add (x, inf)
						inters.insert( CADInterval(*it, 0, CADInterval::CADBoundType::OPEN, CADInterval::CADBoundType::INF, c) );
					
					// add open interval to next root
					if (it != r.rbegin().base())
						inters.insert(CADInterval(*it, *(std::next(it,1)), c) );
				}
			}
			/* sort intervals by ascending order of lower bounds */
			std::sort(inters.begin(), inters.end(), [](CADInterval a, CADInterval b) {
        		return a.isLowerThan(b);
    		});
			return inters;
		}

		/** checks whether the variable is at least as high in the var order as currVar of level */
		bool isAtLeastCurrVar(carl::Variable v) {
			/* if currVar is given, obviously true */
			if(v == currVar)
				return true;

			/* go throught vars until currvar, then look for the given variable */
			bool curr = false;
			for(auto var : mVariables) {
				if(!curr && var == currVar)
					curr = true;
				else if(curr && var == v) {
					return true;
				/* case v = currVar covered before */
				}

			/* if the given var was not found at/after currVar */
			return false;
			}
		}


		/** adds an unsat interval to the internal data structures of the level */
		void addUnsatInterval(CADInterval inter) {
			intervals.push_back(inter);

			// -inf or +inf are special cases
			if(inter.isInfinite()) {
				levelintervalminf = true;
				levelintervalpinf = true;
			}
			else if(inter.isHalfBounded())
			{
				if(inter.getLowerBoundType() == CADInterval::CADBoundType::INF) {
					levelintervalminf = true;
					levelintervals.insert(inter.getUpper());
				}
				else {
					levelintervalpinf = true;
					levelintervals.insert(inter.getLower());
				}
			}
			else {
				levelintervals.insert(inter.getLower());
				levelintervals.insert(inter.getUpper());
			}
		}

	public:

		//@todo init all class vars
		LiftingLevel(std::vector<ConstraintT> conss, carl::Variable v): 
			mConstraints(conss), currVar(v) {
			addUnsatIntervals(calcIntervalsFromPolys(currVar, mConstraints));
		}

		void reset(std::vector<carl::Variable>&& vars) {
			mVariables = std::move(vars);
			//@todo current sample
			resetIntervals();
		}

		/** removes all stored intervals */
		void resetIntervals() {
			intervals.clear();
			levelintervals.clear();
			levelintervalminf = false;
			levelintervalpinf = false;
		}

		/** gets the current sample */
		const auto& getCurrentSample() const {
			return curSample;
		}

		/** gets all unsat intervals
		 * @param s sample for variable of depth i-1 (only necessary if dimension is > 1)
		 * @note asserts that constraints were given to level beforehand
		 * (Paper Alg. 1)
		*/
		const std::set<CADInterval>& getUnsatIntervals(Sample s) const {
			resetIntervals(); /*@todo necessary? */

			/* constraints are filtered for ones with main var currVar or higher */
			std::vector<ConstraintT> constraints;
			for(auto c : mConstraints) {
				auto consvars = c.variables();
				bool add = false;
				for(auto v : consvars) {
					if(isAtLeastCurrVar(v)) {
						add = true;
						break;
					}
				}
				if(add)
					constraints.push_back(c);
			}

			/* map variable of depth i-1 to sample value */
			EvaluationMap evalbase = new EvaluationMap();
			if(dim() > 1) {
				carl::Variable v = mVariables.at(mVariables.size() - 2);
				evalbase.insert( std::pair<carl::Variable, RAN>(v, s.value()) );
			}

			/* gather intervals from each constraint */
			std::set<CADInterval> newintervals;
			for(auto c : constraints) {
				unsigned issat = c.satisfiedBy(evalbase);
				/* if unsat, return (-inf, +inf) */
				if(issat == 0) /*@todo is this equiv to "c(s) == false"? */
					return new std::set(new CADInterval(c));
				/* if sat, constraint is finished */
				else if(issat == 1)
					continue;
				else {
					// get unsat intervals for constraint
					auto inters = calcIntervalsFromPolys(c, evalbase);
					for(auto inter : inters) {
						auto r = inter.getRepresentative();
						EvaluationMap eval = new EvaluationMap(evalbase);
						eval.insert(std::pair<carl::Variable, RAN>(currVal, r));
						std::vector<Poly> lowerreason;
						std::vector<Poly> upperreason;
						if(c.satisfiedBy(eval) == 0) { //@todo again, is this right?
							if(inter.getLowerBoundType() != CADInterval::CADBoundType::INF)
								lowerreason.push_back(c.lhs());
							if(inter.getUpperBoundType() != CADInterval::CADBoundType::INF)
								upperreason.push_back(c.lhs());
							newintervals.insert(new CADInterval(inter.getLower(), inter.getUpper(), inter.getLowerBoundType(), inter.getUpperBoundType(), lowerreason, upperreason, c));
						}
					} 
				}
			}
			return newintervals;
		}


		/** adds a set of unsat intervals */
		void addUnsatIntervals(std::set<CADInterval> inters) {
			for(auto inter : inters) {
				addUnsatInterval(inter);
			}
		}


		/** checks whether the unsat intervals contain (-inf, inf) */
		bool isSingletonCover() {
			if(intervals.empty()) {
				return false;
			}
			else {
				for(auto inter : intervals) {
					if(inter.isInfinite()) {
						return true;
					}
				}
			}
			return false;
		}

		/** @brief Checks whether an unsat cover was found
		 * 
		 * Checks whether the detected unsat intervals cover the reals in this level with given prefix
		 * @returns true iff there is an unsat cover
		 */
		bool isUnsatCover() {
			// check whether -inf and +inf are included
			if(!levelintervalminf || !levelintervalpinf) {
				return false;
			}
			if(isSingletonCover()) {
				return true;
			}
			
			// check whether any unexplored interval remains
			auto boundtuple = getLowestUpperBound();
			if (!std::get<0>(boundtuple)) {
				return true;
			}
			return false;
		}

		/**@brief computes a cover from the given set of intervals
		 * 
		 * @returns subset of intervals that form a cover or an empty set if none was found
		 * (Paper Alg. 2)
		 */
		std::set<CADInterval>compute_cover(std::set<CADInterval> inters) {
			// check whether there is a gap in the covering
			auto boundtuple = getLowestUpperBound();
			if (!std::get<0>(boundtuple)) {		// there is a cover
				return std::get<4>(boundtuple);
			}

			return std::set<CADInterval>();
		}

		/** @brief computes the next Sample
		 * 
		 * Chooses a Sample outside the currently known unsat intervals
		 * 
		 * @note check whether an unsat cover has been found before calling this
		 * @todo when using infty bounds in carl intervals, is 0 a valid bound input?
		 */
		Sample chooseSample() {
			//@todo remove the current value of curSample

			// if -inf is not a bound find sample in (-inf, first bound)
			if(!levelintervalminf) {
				RAN upper = levelintervals.begin();
				auto computeinterval = carl::Interval<RAN>(0, carl::BoundType::INFTY, upper, carl::BoundType::STRICT);
				RAN samplenr = sample(computeinterval, false); 
				curSample = Sample(samplenr);
			}

			auto boundtuple = getLowestUpperBound();
			assert(std::get<0>(boundtuple)); //@todo handle this instead
			RAN bound = std::get<1>(boundtuple);

			// check whether the nex unexplored interval is a point interval
			for(auto inter: intervals) {
				if(bound == inter.getLower() && inter.getLowerBoundType() == CADInterval::CADBoundType::OPEN 
					&& !isInUnsatInterval(bound)) {
					curSample = Sample(bound); //@todo is this a root in this case? if so, set isRoot of sample
				}
			}

			// case the next lowest upper bound is the last recorded bound
			if(bound == *levelintervals.rbegin()) {
				auto computeinterval = carl::Interval<RAN>(bound, carl::BoundType::STRICT, 0, carl::BoundType::INFTY);
				RAN samplenr = sample(computeinterval, false); 
				curSample = Sample(samplenr);
			}

			// go to the next bound
			//@todo assuming that set is ordered. check that
			std::set<RAN>::iterator it = levelintervals.begin();
			while((*it) < std::get<1>(boundtuple)) {
				it++;
			}
			// determine whether next bound is closed, do not break in case bound appears > once.
			bool boundopen = true;
			for(auto bound : intervals) {
				if(bound.getLower() == (*it)) {
					if(bound.getLowerBoundType() == CADInterval::CADBoundType::CLOSED) {
						boundopen = false;
					}
				}
			}
			// we got the bounds and their types, find sample in between
			auto computeinterval = carl::Interval<RAN>(std::get<1>(boundtuple),(*it)); //@todo bound types?
			RAN samplenr = sample(computeinterval, false); 
			//@todo the false leads to bounds not included, so we prefer choosing samples that are not bounds. is that important?
			curSample = Sample(samplenr);

			return curSample;
		}
	};
}
};