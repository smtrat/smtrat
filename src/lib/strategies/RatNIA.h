/**
 * @file RatNIA.h
 */
#pragma once

#include "../solver/Manager.h"
#include "../modules/FPPModule/FPPModule.h"
#include "../modules/IncWidthModule/IncWidthModule.h"
#include "../modules/SATModule/SATModule.h"
#include "../modules/IntBlastModule/IntBlastModule.h"
#include "../modules/CubeLIAModule/CubeLIAModule.h"
#include "../modules/LRAModule/LRAModule.h"
#include "../modules/VSModule/VSModule.h"
#include "../modules/CADModule/CADModule.h"

namespace smtrat
{
    /**
     * Strategy description.
     *
     * @author
     * @since
     * @version
     *
     */
    class RatNIA:
        public Manager
    {
        static bool conditionEvaluation5( carl::Condition _condition )
        {
            return ( (carl::PROP_CONTAINS_INTEGER_VALUED_VARS <= _condition) );
        }

        static bool conditionEvaluation4( carl::Condition _condition )
        {
            return (  !(carl::PROP_CONTAINS_INTEGER_VALUED_VARS <= _condition) );
        }

        public:

        RatNIA(): Manager()
        {
            setStrategy(
            {
                addBackend<FPPModule<FPPSettings1>>(
                {
                    addBackend<IncWidthModule<IncWidthSettings1>>(
                    {
//                            addBackend<SATModule<SATSettings1>>(
//                            {
                            addBackend<IntBlastModule<IntBlastSettings1>>(
                            {
                                addBackend<SATModule<SATSettings1>>(
                                {
                                    addBackend<CubeLIAModule<CubeLIASettings1>>(
                                    {
                                        addBackend<LRAModule<LRASettings1>>(
                                        {
                                            addBackend<VSModule<VSSettings234>>(
                                            {
                                                addBackend<CADModule<CADSettingsGuessAndSplit>>()
                                            })
                                        })
                                    })
                                }).condition( &conditionEvaluation5 ),
                                addBackend<SATModule<SATSettings1>>().condition( &conditionEvaluation4 )
                            })
//                            })
                    })
                })
            });
        }
    };
} // namespace smtrat