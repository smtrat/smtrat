/**
 * @file CBSettings.h
 * @author YOUR NAME <YOUR EMAIL ADDRESS>
 *
 * @version 2015-09-10
 * Created on 2015-09-10.
 */

#pragma once

#include "../../solver/ModuleSettings.h"

namespace smtrat
{
    struct CBSettings1 : ModuleSettings
    {
		static constexpr auto moduleName = "CBModule<CBSettings1>";
        /**
         * Example for a setting.
         */
        static const bool example_setting = true;
    };
}
