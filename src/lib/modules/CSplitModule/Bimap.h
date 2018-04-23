#ifndef BIMAP_HPP
#define BIMAP_HPP

#include <forward_list>
#include <set>

namespace smtrat
{
	template<class Class, typename FirstKeyType, FirstKeyType Class::*FirstKeyName, typename SecondKeyType, SecondKeyType Class::*SecondKeyName>
	class Bimap
	{
		public:
			typedef std::forward_list<Class> Data;
			typedef typename Data::iterator Iterator;
			typedef typename Data::const_iterator ConstIterator;
		
		private:
			struct FirstCompare
			{
				using is_transparent = void;
				
				bool operator()(const Iterator& lhs, const Iterator& rhs) const
				{
					return (*lhs).*FirstKeyName<(*rhs).*FirstKeyName;
				}
				
				bool operator()(const Iterator& lhs, const FirstKeyType& rhs) const
				{
					return (*lhs).*FirstKeyName<rhs;
				}
				
				bool operator()(const FirstKeyType& lhs, const Iterator& rhs) const
				{
					return lhs<(*rhs).*FirstKeyName;
				}
			};
			
			struct SecondCompare
			{
				using is_transparent = void;
				
				bool operator()(const Iterator& lhs, const Iterator& rhs) const
				{
					return (*lhs).*SecondKeyName<(*rhs).*SecondKeyName;
				}
				
				bool operator()(const Iterator& lhs, const SecondKeyType& rhs) const
				{
					return (*lhs).*SecondKeyName<rhs;
				}
				
				bool operator()(const SecondKeyType& lhs, const Iterator& rhs) const
				{
					return lhs<(*rhs).*SecondKeyName;
				}
			};
			
			Data mData;
			std::set<Iterator, FirstCompare> mFirstMap;
			std::set<Iterator, SecondCompare> mSecondMap;
		
		public:
			Iterator begin() noexcept
			{
				return mData.begin();
			}
			
			ConstIterator begin() const noexcept
			{
				return mData.begin();
			}
			
			Iterator end() noexcept
			{
				return mData.end();
			}
			
			ConstIterator end() const noexcept
			{
				return mData.end();
			}
			
			Iterator firstFind(const FirstKeyType& firstKey)
			{
				auto firstIter{mFirstMap.find(firstKey)};
				if (firstIter == mFirstMap.end())
					return mData.end();
				else
					return *firstIter;
			}
			
			Iterator secondFind(const SecondKeyType& secondKey)
			{
				auto secondIter{mSecondMap.find(secondKey)};
				if (secondIter == mSecondMap.end())
					return mData.end();
				else
					return *secondIter;
			}
			
			template<typename... Args>
			Iterator emplace(Args&&... args)
			{
				mData.emplace_front(std::move(args)...);
				mFirstMap.emplace(mData.begin());
				mSecondMap.emplace(mData.begin());
				return mData.begin();
			}
	};
}

#endif
