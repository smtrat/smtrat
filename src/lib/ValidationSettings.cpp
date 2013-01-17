/*
 *  SMT-RAT - Satisfiability-Modulo-Theories Real Algebra Toolbox
 * Copyright (C) 2012 Florian Corzilius, Ulrich Loup, Erika Abraham, Sebastian Junges
 *
 * This file is part of SMT-RAT.
 *
 * SMT-RAT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SMT-RAT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SMT-RAT.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


/**
 * @file ValidationSettings.cpp
 * @author Sebastian Junges
 * @version 2013-01-16
 */

#include "ValidationSettings.h"
#include <string>
#include <iostream>

namespace smtrat
{
    ValidationSettings::ValidationSettings() :
        mLogLemmata(false),
        mLogTCalls(false),
        mLogInfSubsets(false),
        mPath("assumptions.smt2")    
    {
        
    }


    void ValidationSettings::parseCmdOption(const std::string& keyValueString) 
    {
        std::map<std::string, std::string> keyvalues = splitIntoKeyValues(keyValueString);
        if(keyvalues.count("log-all") > 0) 
        {
            mLogLemmata = true;
            mLogTCalls = true;
            mLogInfSubsets = true;
        }
        else
        {
            // Yes, this is in nlogn but it is more readable then if it is in n. And n is small anyway.
            setFlagIfOptionSet(keyvalues, mLogLemmata, "log-lemmata");
            setFlagIfOptionSet(keyvalues, mLogTCalls, "log-tcalls");
            setFlagIfOptionSet(keyvalues, mLogInfSubsets, "log-infsubsets");
        }
        setValueIfKeyExists(keyvalues, mPath, "path");
    }

    void ValidationSettings::printHelp(const std::string& prefix) const
    {
        std::cout << prefix <<  "Seperate options by a comma." << std::endl;
        std::cout << prefix <<  "Options:" << std::endl;
        std::cout << prefix <<  "\t log-all \t\t Log all intermediate steps." << std::endl;
        std::cout << prefix <<  "\t log-lemmata \t\t Enables logging of produced lemmata." << std::endl;
        std::cout << prefix <<  "\t log-tcalls \t\t Enables logging of theory calls" << std::endl;
        std::cout << prefix <<  "\t log-infsubsets \t Enalbes logging of the infeasible subsets" << std::endl;
        std::cout << prefix <<  "\t path=<value> \t\t Sets the output path. Default is assumptions.smt2" << std::endl;
    }

    bool ValidationSettings::logTCalls() const 
    {
        return mLogTCalls;
    }
    
    bool ValidationSettings::logLemmata() const 
    {
        return mLogLemmata;
    }
    
    bool ValidationSettings::logInfSubsets() const
    {
        return mLogInfSubsets;
    }
    
    const std::string& ValidationSettings::path() const
    {
        return mPath;
    }
            
}


