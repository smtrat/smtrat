#pragma once

#include "properties.h"

namespace smtrat::cadcells::operators::delineation {

template<typename P>
void delineate(datastructures::DelineatedDerivation<P>& deriv, const properties::poly_irreducible_sgn_inv& prop) {
    if (deriv.proj().is_nullified(deriv.underlying_sample(), prop.poly)) {
        deriv.delin().add_poly_nullified(prop.poly);
    } else {
        auto roots = deriv.proj().real_roots(deriv.underlying_sample(), prop.poly);
        if (roots.empty()) {
            deriv.delin().add_poly_noroot(prop.poly);
        } else {
            for (size_t idx = 0; idx < roots.size(); idx++) {
                deriv.delin().add_root(roots[idx], datastructures::IndexedRoot(prop.poly, idx+1));
            }
        }
    }
}
    
}