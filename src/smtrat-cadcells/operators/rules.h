#pragma once

#include "properties.h"
#include "../datastructures/derivation.h"

namespace smtrat::cadcells::operators::rules {

template<typename P>
void root_well_def(datastructures::SampledDerivation<P>& deriv, datastructures::IndexedRoot root) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "well_def(" << root << ", " << deriv.sample() << ")");
    assert(deriv.contains(properties::poly_pdel{ root.poly }));

    if (root.index != 1 && root.index != deriv.proj().num_roots(deriv.sample(), root.poly)) {
        SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "-> well_def(" << root << ", " << deriv.sample() << ") <= proj_del(" << root.poly << ") && 0 < " << root << ".index < |real_roots(" << root.poly << "(" << deriv.sample() << "))|");
    }
    else if (deriv.proj().is_ldcf_zero(deriv.sample(), root.poly)) {
        SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "-> well_def(" << root << ", " << deriv.sample() << ") <= proj_del(" << root.poly << ") && ldcf(" << root.poly << ")(" << deriv.sample() << ") = 0");
    }
    else {
        SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "-> well_def(" << root << ", " << deriv.sample() << ") <= proj_del(" << root.poly << ") && sgn_inv(ldcf(" << root.poly << "))");
        deriv.insert(properties::poly_sgn_inv{ deriv.proj().ldcf(root.poly) });
    }
}

template<typename P>
bool poly_non_null(datastructures::SampledDerivation<P>& deriv, datastructures::PolyRef poly) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "non_null(" << poly << ")");
    if(deriv.proj().is_nullified(deriv.sample(), poly)) {
        SMTRAT_LOG_TRACE("-> smtrat.cadcells.operators.rules", "non_null(" << poly << ") <= false");
        return false;
    } else if (deriv.proj().has_const_coeff(poly)) {
        SMTRAT_LOG_TRACE("-> smtrat.cadcells.operators.rules", "non_null(" << poly << ") <= " << poly << " has const coeff");
    } else if (!deriv.proj().is_ldcf_zero(deriv.sample(), poly) && deriv.contains(properties::poly_sgn_inv{ deriv.proj().ldcf(poly) } )) {
        SMTRAT_LOG_TRACE("-> smtrat.cadcells.operators.rules", "non_null(" << poly << ") <= ldcf(" << poly << ")(" << deriv.sample() << ")!=0 && sgn_inv(ldcf(" << poly << "))");
    } else if (
        deriv.proj().know_disc(poly) && !deriv.proj().is_disc_zero(deriv.sample(), poly) &&
        (deriv.contains(properties::poly_sgn_inv{ deriv.proj().disc(poly) }) || deriv.contains(properties::poly_ord_inv{ deriv.proj().disc(poly) }))
    ) {
        SMTRAT_LOG_TRACE("-> smtrat.cadcells.operators.rules", "non_null(" << poly << ") <= disc(" << poly << ")(" << deriv.sample() << ")!=0 && sgn_inv(disc(" << poly << "))");
    } else {
        auto coeff = deriv.proj().simplest_nonzero_coeff(deriv.sample(), poly, [&](const Poly& a, const Poly& b) {
            if (deriv.proj().known(a) && deriv.contains( properties::poly_sgn_inv { deriv.proj().polys()(a)} )) return true;
            if (deriv.proj().known(b) && deriv.contains( properties::poly_sgn_inv { deriv.proj().polys()(b)} )) return true;
            return helper::level_of(deriv.polys().var_order(),a) < helper::level_of(deriv.polys().var_order(),b) || (helper::level_of(deriv.polys().var_order(),a) == helper::level_of(deriv.polys().var_order(),b) && a.degree(helper::main_var(deriv.polys().var_order(), a)) < b.degree(helper::main_var(deriv.polys().var_order(), b)));
        });
        SMTRAT_LOG_TRACE("-> smtrat.cadcells.operators.rules", "non_null(" << poly << ") <= sgn_inv(" << coeff << ") && " << coeff << " is coeff of " << poly << "");
        deriv.insert(properties::poly_sgn_inv{ coeff });
    }
    return true;
}

template<typename P>
bool poly_pdel(datastructures::SampledDerivation<P>& deriv, datastructures::PolyRef poly) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "proj_del(" << poly << ")");
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "-> proj_del(" << poly << ") <= non_null(" << poly << ") && ord_inv(disc(" << poly <<"))");
    if (!poly_non_null(deriv, poly)) return false;
    deriv.insert(properties::poly_ord_inv{ deriv.proj().disc(poly) });
    return true;
}

template<typename P>
void poly_irrecubile_ord_inv(datastructures::SampledDerivation<P>& deriv, datastructures::PolyRef poly) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "ord_inv(" << poly << "), " << poly << " irreducible");
    if (deriv.proj().is_const(poly)) {
        SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "-> ord_inv(" << poly << ") <= " << poly << " const");
    } else {
        if (deriv.proj().is_zero(deriv.sample(), poly)) {
            SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "-> ord_inv(" << poly << ") <= proj_del(" << poly << ") && sgn_inv(" << poly << ")");
            deriv.insert(properties::poly_pdel{ poly });
        } else {
            SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "-> ord_inv(" << poly << ") <= " << poly << "(" << deriv.sample() << ") != 0 && sgn_inv(" << poly << ")");
        }
        deriv.insert(properties::poly_irreducible_sgn_inv{ poly });
    }
}

template<typename P>
void poly_ord_inv(datastructures::SampledDerivation<P>& deriv, datastructures::PolyRef poly) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "ord_inv(" << poly << ")");
    if (deriv.proj().is_const(poly)) {
        SMTRAT_LOG_TRACE("-> smtrat.cadcells.operators.rules", "-> ord_inv(" << poly << ") <= " << poly << " const");
    } else {
        auto factors = deriv.proj().factors_nonconst(poly);
        SMTRAT_LOG_TRACE("-> smtrat.cadcells.operators.rules", "-> ord_inv(" << poly << ") <= ord_inv(factors(" << poly << ")) <=> ord_inv(" << factors << ")");
        for (const auto& factor : factors) {
            poly_irrecubile_ord_inv(deriv, factor);
        }
    }   
}

template<typename P>
void poly_sgn_inv(datastructures::BaseDerivation<P>& deriv, datastructures::PolyRef poly) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "sgn_inv(" << poly << ")");
    if (deriv.proj().is_const(poly)) {
        SMTRAT_LOG_TRACE("-> smtrat.cadcells.operators.rules", "-> sgn_inv(" << poly << ") <= " << poly << " const");
    } else if (deriv.contains(properties::poly_ord_inv{ poly })) {
        SMTRAT_LOG_TRACE("-> smtrat.cadcells.operators.rules", "-> sgn_inv(" << poly << ") <= ord_inv(" << poly << ")");
    } else {
        auto factors = deriv.proj().factors_nonconst(poly);
        SMTRAT_LOG_TRACE("-> smtrat.cadcells.operators.rules", "-> sgn_inv(" << poly << ") <= sgn_inv(factors(" << poly << ")) <=> sgn_inv(" << factors << ")");
        for (const auto& factor : factors) {
            deriv.insert(properties::poly_irreducible_sgn_inv{ factor });
        }
    }
}

template<typename P>
void poly_irrecubile_nonzero_sgn_inv(datastructures::DelineatedDerivation<P>& deriv, datastructures::PolyRef poly) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "sgn_inv(" << poly << "), " << poly << " irreducible and non-zero");
    assert(deriv.contains(properties::poly_pdel{ poly }));
    assert(deriv.proj().num_roots(deriv.underlying_sample(), poly) == 0);
    if (deriv.proj().is_ldcf_zero(deriv.underlying_sample(), poly)) {
        deriv.insert(properties::poly_sgn_inv{ deriv.proj().ldcf(poly) });
    }
}

template<typename P>
void cell_connected(datastructures::SampledDerivation<P>& deriv, const datastructures::CellDescription& cell) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "connected(" << deriv.level() << ")");
    if (cell.is_sector() && cell.lower() && cell.upper() && cell.lower()->poly != cell.upper()->poly) {
        assert(deriv.contains(properties::poly_pdel{ cell.lower()->poly }));
        assert(deriv.contains(properties::poly_pdel{ cell.upper()->poly }));
        deriv.insert(properties::poly_ord_inv{ deriv.proj().res(cell.lower()->poly, cell.upper()->poly) });
    }
}

template<typename P>
void cell_analytic_submanifold(datastructures::SampledDerivation<P>& deriv, const datastructures::CellDescription&) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "an_sub(" << deriv.level() << ")");
}

template<typename P>
void poly_irrecubile_sgn_inv_ec(datastructures::SampledDerivation<P>& deriv, const datastructures::CellDescription& cell, datastructures::PolyRef poly) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "sgn_inv(" << poly << "), using EC");
    assert(cell.is_section());
    assert(deriv.contains(properties::poly_pdel{ cell.section_defining().poly }));
    assert(deriv.contains(properties::poly_sgn_inv{ deriv.proj().ldcf(cell.section_defining().poly) }));
    if (cell.section_defining().poly != poly) {
        deriv.insert(properties::poly_ord_inv{ deriv.proj().res(cell.section_defining().poly, poly) });
    }
}

template<typename P>
void root_represents(datastructures::SampledDerivation<P>& deriv, datastructures::IndexedRoot root) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "ir_rep(" << root << ", " << deriv.sample() << ")");
    assert(deriv.contains(properties::poly_pdel{ root.poly }));
    deriv.insert(properties::poly_sgn_inv{ deriv.proj().ldcf(root.poly) });
}

template<typename P>
void cell_represents(datastructures::SampledDerivation<P>& deriv, const datastructures::CellDescription& cell) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "repr(" << cell << ")");
    if (cell.is_sector()) {
        if (cell.lower()) {
            root_represents(deriv, *cell.lower());
        }
        if (cell.upper()) {
            root_represents(deriv, *cell.upper());
        }
    } else {
        root_represents(deriv, cell.section_defining());
    }
}

template<typename P>
void cell_well_def(datastructures::SampledDerivation<P>& deriv, const datastructures::CellDescription& cell) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "well_def(" << cell << ")");
    if (cell.is_sector()) {
        if (cell.lower()) {
            deriv.insert(properties::root_well_def{ *cell.lower() });
        }
        if (cell.upper()) {
            deriv.insert(properties::root_well_def{ *cell.upper() });
        }
    } else {
        deriv.insert(properties::root_well_def{ cell.section_defining() });
    }
}

template<typename P>
void root_ordering_holds(datastructures::SampledDerivation<P>& deriv, const datastructures::CellDescription&, const datastructures::IndexedRootOrdering& ordering) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "ir_rep(" << ordering << ", " << deriv.sample() << ")");
    for (const auto& rel : ordering.below()) {
        if (rel.first.poly != rel.second.poly) {
            assert(deriv.contains(properties::poly_pdel{ rel.first.poly }));
            assert(deriv.contains(properties::poly_pdel{ rel.second.poly }));
            deriv.insert(properties::poly_ord_inv{ deriv.proj().res(rel.first.poly, rel.second.poly) });
            deriv.insert(properties::root_well_def{ rel.second });
        }
    }
    for (const auto& rel : ordering.above()) {
        if (rel.first.poly != rel.second.poly) {
            assert(deriv.contains(properties::poly_pdel{ rel.first.poly }));
            assert(deriv.contains(properties::poly_pdel{ rel.second.poly }));
            deriv.insert(properties::poly_ord_inv{ deriv.proj().res(rel.first.poly, rel.second.poly) });
            deriv.insert(properties::root_well_def{ rel.first });
        }
    }
}

template<typename P>
void poly_irrecubile_sgn_inv(datastructures::SampledDerivation<P>& deriv, const datastructures::CellDescription& cell, const datastructures::IndexedRootOrdering& ordering, datastructures::PolyRef poly) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "sgn_inv(" << poly << "), " << poly << " irreducible");
    assert(deriv.contains(properties::poly_pdel{ poly }));
    if (cell.is_section() && deriv.proj().is_zero(deriv.sample(), poly)) {
        auto roots = deriv.proj().real_roots(deriv.underlying_sample(), poly);
        auto it = std::find(roots.begin(), roots.end(), deriv.main_var_sample());
        auto dist = std::distance(roots.begin(), it);
        assert(dist >= 0);
        deriv.insert(properties::root_well_def{ datastructures::IndexedRoot(poly, (size_t)dist + 1) });
    } else {
        assert(!deriv.proj().is_zero(deriv.sample(), poly));
        if (cell.is_sector() && (deriv.cell().lower_unbounded() || deriv.cell().upper_unbounded())) {
            deriv.insert(properties::poly_sgn_inv{ deriv.proj().ldcf(poly) });
        } else {
            bool has_lower = (poly == cell.lower()->poly) || std::find_if(ordering.below().begin(), ordering.below().end(), [&poly](const auto& rel) { return rel.second.poly == poly; }) != ordering.below().end();
            bool has_upper = (poly == cell.upper()->poly) || std::find_if(ordering.above().begin(), ordering.above().end(), [&poly](const auto& rel) { return rel.second.poly == poly; }) != ordering.above().end();
            if (!(has_lower && has_upper)) {
                // TODO later: implement further checks
                deriv.insert(properties::poly_sgn_inv{ deriv.proj().ldcf(poly) });
            }
        }
    }
}

template<typename P>
void covering_holds(datastructures::DelineatedDerivation<P>& deriv, const datastructures::CoveringDescription& covering) {
    SMTRAT_LOG_TRACE("smtrat.cadcells.operators.rules", "holds(" << covering << ")");
    for (auto it = covering.cells().begin(); it != covering.cells().end()-1; it++) {
        assert(deriv.contains(properties::root_well_def{ *it->upper_defining() }));
        assert(deriv.contains(properties::root_well_def{ *std::next(it)->lower_defining() }));
        if (it->upper_defining()->poly != std::next(it)->lower_defining()->poly) {
            deriv.insert(properties::poly_ord_inv{ deriv.proj().res(it->upper_defining()->poly, std::next(it)->lower_defining()->poly) });
        }
    }
}

}