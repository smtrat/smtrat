include(${carl_INCLUDE_DIR}/../cmake/carlmacros.cmake)

add_definitions(-fPIC)

configure_everything()

file(GLOB smtrat-modules_sources "*.cpp")

add_new_libraries(smtrat-modules "${PROJECT_VERSION}" "${smtrat-modules_sources}" "resources")
target_include_dirs_from(smtrat-modules-objects "" PUBLIC smtrat-common-shared)
target_link_libraries(smtrat-modules-shared smtrat-common-shared)
target_link_libraries(smtrat-modules-static smtrat-common-static)

install_libraries(smtrat-modules-shared smtrat-modules-static)

add_custom_target(smtrat-modules-all)

file(GLOB modules RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *Module/)
foreach(module ${modules})
	if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${module})
	
		# Properly configure Instantiation.h
		if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module}/Instantiation.h.in")
			string(REPLACE "Module" "" Prefix ${module})
			file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${module}/${Prefix}Settings.h" Content)
			string(REGEX MATCHALL "[^_0-9a-zA-Z]${Prefix}Settings[_0-9a-zA-Z]+" Matches ${Content})
			
			set(settings "")
			foreach(Match ${Matches})
				string(SUBSTRING ${Match} 1 -1 Match)
				list(APPEND settings ${Match})
			endforeach()
			list(SORT settings)
			list(REMOVE_DUPLICATES settings)
			set(INSTANTIATIONS "")
			foreach(setting ${settings})
				set(INSTANTIATIONS "${INSTANTIATIONS}template class ${modType}<${setting}>;\n")
			endforeach()
			configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${module}/Instantiation.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/${module}/Instantiation.h")
		endif()

		# Create library
		file(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/${module}/*.cpp)
		
		add_new_libraries(smtrat-module-${module} ${PROJECT_VERSION} "${sources}" "resources")
		target_include_dirs_from(smtrat-module-${module}-objects "" PUBLIC carl-shared smtrat-common-shared)
		target_link_libraries(smtrat-module-${module}-shared smtrat-common-shared)
		target_link_libraries(smtrat-module-${module}-static smtrat-common-static)

		add_dependencies(smtrat-modules-all smtrat-module-${module}-shared smtrat-module-${module}-static)
	endif()
endforeach()