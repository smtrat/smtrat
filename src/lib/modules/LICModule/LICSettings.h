/**
 * @file LICSettings.h
 * @author YOUR NAME <YOUR EMAIL ADDRESS>
 *
 * @version 2015-09-03
 * Created on 2015-09-03.
 */

#pragma once

#include "../../solver/ModuleSettings.h"

namespace smtrat
{
    struct LICSettings1 : ModuleSettings
    {
		static constexpr auto moduleName = "LICModule<LICSettings1>";
        /**
         * Example for a setting.
         */
        static const bool example_setting = true;
    };
}
