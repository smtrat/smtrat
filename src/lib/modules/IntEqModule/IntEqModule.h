/**
 * @file IntEqModule.h
 * @author Dustin Huetter <dustin.huetter@rwth-aachen.de>
 *
 * @version 2014-10-17
 * Created on 2014-10-17.
 */
#pragma once
#include "../../solver/Module.h"
#include "IntEqStatistics.h"
#include "IntEqSettings.h"
#include <stdio.h>
namespace smtrat
{
    typedef std::map< FormulaT, std::shared_ptr< std::vector<FormulaT> > > Formula_Origins;
        
    /**
     * A module which checks whether the equations contained in the received formula 
     * have an integer solution.
     */    
    template<typename Settings>
    class IntEqModule : public Module
    {
        private:
            // Stores the current equations of the received constraints and their origins
            Formula_Origins mProc_Constraints;
            // Stores the equations of the every iteration step
            std::vector< Formula_Origins > mRecent_Constraints;
            // Stores the calculated substitutions
            std::vector< std::pair< carl::Variable, Poly > >  mSubstitutions;
            // Stores the origins of the calculated substitutions
            std::map< carl::Variable, std::shared_ptr< std::vector<FormulaT> > > mVariables;
            // Stores the auxiliary variables
            std::set< carl::Variable > mAuxiliaries;
            // Stores the determined (temporary) model 
            mutable Model mTemp_Model;
            // Stores whether a new substitution has been found in the last ckeckCore call and 
            // we had afterwards no addCore call with an equality
            bool mNew_Substitution;
            
            /*
             * This module constructs a solution when it's possible and returns
             * true in this case. Otherwise, e.g. if a disequality is not satisfied,
             * it returns false.
             */
            bool constructSolution();
            
        public:
            
            IntEqModule( ModuleType _type, const ModuleInput* _formula, RuntimeSettings* _settings, Conditionals& _conditionals, Manager* _manager = NULL );
            
            ~IntEqModule() {}
        
            /**
             * The module has to take the given sub-formula of the received formula into account.
             *
             * @param _subformula The sub-formula to take additionally into account.
             * @return false, if it can be easily decided that this sub-formula causes a conflict with
             *          the already considered sub-formulas;
             *          true, otherwise.
             */
            bool addCore( ModuleInput::const_iterator _subformula );
            
            /**
             * Removes the subformula of the received formula at the given position to the considered ones of this module.
             * Note that this includes every stored calculation which depended on this subformula, but should keep the other
             * stored calculation, if possible, untouched.
             *
             * @param _subformula The position of the subformula to remove.
             */
            void removeCore( ModuleInput::const_iterator _subformula );
            
            /**
             * Updates the current assignment into the model.
             * Note, that this is a unique but possibly symbolic assignment maybe containing newly introduced variables.
             */
            void updateModel() const;
            
            /**
             * Checks the received formula for consistency.
             * @param _full false, if this module should avoid too expensive procedures and rather return unknown instead.
             * @return True,    if the received formula is satisfiable;
             *         False,   if the received formula is not satisfiable;
             *         Unknown, otherwise.
             */
            Answer checkCore( bool _full );
    };
}
#include "IntEqModule.tpp"
