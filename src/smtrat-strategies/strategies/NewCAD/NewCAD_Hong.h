#pragma once

#include <smtrat-solver/Manager.h>

#include <smtrat-modules/NewCADModule/NewCADModule.h>
#include <smtrat-modules/SATModule/SATModule.h>

namespace smtrat
{
	class NewCAD_Hong: public Manager
	{
		public:
			NewCAD_Hong(): Manager() {
				setStrategy(
					addBackend<SATModule<SATSettings1>>(
						addBackend<NewCADModule<NewCADSettingsHong>>()
					)
				);
			}
	};
}
