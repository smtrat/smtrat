include(${carl_INCLUDE_DIR}/../cmake/carlmacros.cmake)

add_definitions(-fPIC)

configure_everything()

file(GLOB_RECURSE smtrat-qe_sources "*.cpp")

add_new_libraries(smtrat-qe "${PROJECT_VERSION_LIB}" "${smtrat-qe_sources}" "resources")
target_include_dirs_from(smtrat-qe-objects "" PUBLIC smtrat-common-shared carl-model-shared)
target_link_libraries(smtrat-qe-shared smtrat-common-shared carl-model-shared)
target_link_libraries(smtrat-qe-static smtrat-common-static carl-model-static)

install_libraries(smtrat smtrat-qe-shared smtrat-qe-static)
