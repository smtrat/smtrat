/**
 * @file RatOne.h
 */
#pragma once

#include "../solver/Manager.h"
#include "../modules/PreprocessingModule/PreprocessingModule.h"
#include "../modules/SATModule/SATModule.h"
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
    class RatOne:
        public Manager
    {
        public:

        RatOne(): Manager()
        {
            setStrategy(
            {
                addBackend<PreprocessingModule<PreprocessingSettings1>>(
                {
                    addBackend<SATModule<SATSettings1>>(
                    {
                        addBackend<LRAModule<LRASettings1>>(
                        {
                            addBackend<VSModule<VSSettings234>>(
                            {
                                addBackend<CADModule<CADSettings1>>()
                            })
                        })
                    })
                })
            });
        }
    };
} // namespace smtrat