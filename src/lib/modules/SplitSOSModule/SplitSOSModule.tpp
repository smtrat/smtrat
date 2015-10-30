/**
 * @file SplitSOSModule.tpp
 * @author Florian Corzilius <corzilius@cs.rwth-aachen.de>
 *
 * @version 2015-09-10
 * Created on 2015-09-10.
 */

namespace smtrat
{
    template<class Settings>
    SplitSOSModule<Settings>::SplitSOSModule( const ModuleInput* _formula, RuntimeSettings*, Conditionals& _conditionals, Manager* _manager ):
        PModule( _formula, _conditionals, _manager ),
        mVisitor()
    {
		splitSOSFunction = std::bind(&SplitSOSModule<Settings>::splitSOS, this, std::placeholders::_1);
    }

    template<class Settings>
    SplitSOSModule<Settings>::~SplitSOSModule()
    {}

    template<class Settings>
    Answer SplitSOSModule<Settings>::checkCore( bool _full )
    {
        auto receivedFormula = firstUncheckedReceivedSubformula();
        while( receivedFormula != rReceivedFormula().end() )
        {
            FormulaT formula = receivedFormula->formula();
            if( receivedFormula->formula().propertyHolds(carl::PROP_CONTAINS_NONLINEAR_POLYNOMIAL) )
            {
                formula = mVisitor.visit( receivedFormula->formula(), splitSOSFunction );
            }
            if( formula.isFalse() )
            {
                mInfeasibleSubsets.clear();
                FormulaSetT infeasibleSubset;
                infeasibleSubset.insert( receivedFormula->formula() );
                mInfeasibleSubsets.push_back( std::move(infeasibleSubset) );
                return False;
            }
            if( !formula.isTrue() )
            {
                addSubformulaToPassedFormula( formula, receivedFormula->formula() );
            }
            ++receivedFormula;
        }
        Answer ans = runBackends( _full );
        if( ans == False )
        {
            mInfeasibleSubsets.clear();
            FormulaSetT infeasibleSubset;
            // TODO: compute a better infeasible subset
            for( auto subformula = rReceivedFormula().begin(); subformula != rReceivedFormula().end(); ++subformula )
            {
                infeasibleSubset.insert( subformula->formula() );
            }
            mInfeasibleSubsets.push_back( std::move(infeasibleSubset) );
        }
        return ans;
    }
	
	template<typename Settings>
    FormulaT SplitSOSModule<Settings>::splitSOS( const FormulaT& formula )
    {
		if( formula.getType() == carl::FormulaType::CONSTRAINT )
        {
            std::vector<std::pair<Rational,Poly>> sosDec;
            bool lcoeffNeg = carl::isNegative( formula.constraint().lhs().lcoeff() );
            if( lcoeffNeg ) {
                sosDec = (-formula.constraint().lhs()).sosDecomposition();
            }
            else
            {
                sosDec = formula.constraint().lhs().sosDecomposition();
            }
            if( sosDec.size() <= 1 )
            {
                return formula;
            }
            carl::Relation rel = carl::Relation::EQ;
            carl::FormulaType boolOp = carl::FormulaType::AND;
            switch( formula.constraint().relation() )
            {
                case carl::Relation::EQ:
                    if( formula.constraint().lhs().hasConstantTerm() )
                        return FormulaT( carl::FormulaType::FALSE );
                    break;
                case carl::Relation::NEQ:
                    if( formula.constraint().lhs().hasConstantTerm() )
                        return FormulaT( carl::FormulaType::TRUE );
                    rel = carl::Relation::NEQ;
                    boolOp = carl::FormulaType::OR;
                    break;
                case carl::Relation::LEQ:
                    if( lcoeffNeg )
                        return FormulaT( carl::FormulaType::TRUE );
                    else if( formula.constraint().lhs().hasConstantTerm() )
                        return FormulaT( carl::FormulaType::FALSE );
                    break;
                case carl::Relation::LESS:
                    if( lcoeffNeg )
                    {
                        if( formula.constraint().lhs().hasConstantTerm() )
                            return FormulaT( carl::FormulaType::TRUE );
                        rel = carl::Relation::NEQ;
                        boolOp = carl::FormulaType::OR;
                    }
                    else 
                        return FormulaT(carl::FormulaType::FALSE);
                    break;
                case carl::Relation::GEQ:
                    if( !lcoeffNeg )
                        return FormulaT( carl::FormulaType::TRUE );
                    else if( formula.constraint().lhs().hasConstantTerm() )
                        return FormulaT( carl::FormulaType::FALSE );
                    break;
                default:
                    assert( formula.constraint().relation() == carl::Relation::GREATER );
                    if( lcoeffNeg )
                        return FormulaT( carl::FormulaType::FALSE );
                    else
                    {
                        if( formula.constraint().lhs().hasConstantTerm() )
                            return FormulaT( carl::FormulaType::TRUE );
                        rel = carl::Relation::NEQ;
                        boolOp = carl::FormulaType::OR;
                    }
            }
            FormulasT subformulas;
			for( auto it = sosDec.begin(); it != sosDec.end(); ++it )
            {
                subformulas.emplace_back( it->second, rel );
			}
			return FormulaT( boolOp, subformulas );
		}
		return formula;
	}
}
