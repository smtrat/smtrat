#pragma once

#include "Sample.h"
#include "../Settings.h"

namespace smtrat {
namespace cad {
	template<typename Iterator, SampleCompareStrategy Strategy>
	struct SampleComparator {};
	
	template<typename Iterator>
	struct SampleComparator<Iterator, SampleCompareStrategy::Value> {
		bool operator()(const Iterator& lhs, const Iterator& rhs) const {
			return *lhs < *rhs;
		}
	};

	template<typename Iterator>
	struct SampleComparator<Iterator, SampleCompareStrategy::Integer> {
		bool operator()(const Iterator& lhs, const Iterator& rhs) const {
			bool lint = lhs->value().isIntegral();
			bool rint = rhs->value().isIntegral();
			if (lint && rint) return *lhs < *rhs;
			if (lint) return true;
			return false;
		}
	};
	
	template<typename Iterator, FullSampleCompareStrategy Strategy>
	struct FullSampleComparator {};
	
	template<typename Iterator>
	struct FullSampleComparator<Iterator, FullSampleCompareStrategy::Value>: SampleComparator<Iterator, SampleCompareStrategy::Value> {};
	template<typename Iterator>
	struct FullSampleComparator<Iterator, FullSampleCompareStrategy::Integer>: SampleComparator<Iterator, SampleCompareStrategy::Integer> {};
}
}