#pragma once

#include <optional>

#include "../common.h"

#include "polynomials.h"

#include <carl/core/polynomialfunctions/Factorization.h>

namespace smtrat::cadcells::datastructures {

    // TODO refactor ??

namespace detail {

struct poly_properties {
    std::map<poly_ref, poly_ref> res;
    std::optional<poly_ref> disc;
    std::optional<poly_ref> ldcf;
    std::vector<poly_ref> factors_nonconst;
};

}

/**
 * Encapsulates all computations on polynomials. 
 * Computations are cached with respect to a poly_pool.
 */
class projections {
    poly_pool& m_pool;
    std::vector<std::vector<detail::poly_properties>> m_poly_cache;

    auto& cache(poly_ref p) {
        assert(p.level > 0);
        m_poly_cache.resize(p.level);
        m_poly_cache[p.level-1].resize(p.id+1);
        return m_poly_cache[p.level-1][p.id];
    }
    const auto& cache(poly_ref p) const {
        assert(p.level > 0);
        assert(p.level-1 < m_poly_cache.size());
        assert(p.id < m_poly_cache[p.level-1].size());
        return m_poly_cache[p.level-1][p.id];
    }

public:
    auto main_var(poly_ref p) const {
        return m_pool.var_order()[p.level-1];
    }

private:
    auto as_univariate(poly_ref p) const {
        return carl::to_univariate_polynomial(m_pool(p), main_var(p));
    }

public:
    projections(poly_pool& pool) : m_pool(pool) {}

    auto& polys() { return m_pool; }
    const auto& polys() const { return m_pool; }

    void clear_cache(size_t level) {
        assert(level > 0);
        m_pool.clear_levels(level);
        if (level <= m_poly_cache.size()) {
            m_poly_cache.erase(m_poly_cache.begin() + (level - 1), m_poly_cache.end());
        }
    }
    
    poly_ref res(poly_ref p, poly_ref q) {
        assert(p.level == q.level && p.id != q.id);

        if (p.id > q.id) return res(q,p);
        assert(p.id < q.id);

        if (cache(p).res.find(q) != cache(p).res.end()) {
            return cache(p).res[q];
        } else {
            auto upoly = carl::resultant(as_univariate(p), as_univariate(q));
            assert(carl::is_constant(upoly));
            auto result = m_pool(Poly(upoly));
            assert(!is_zero(result));
            cache(p).res.emplace(q, result);
            return result;
        }
    }

    bool know_disc(poly_ref p) const {
        return (bool) cache(p).disc;
    }

    bool known(const Poly& p) const {
        return m_pool.known(p);
    } 

    poly_ref disc(poly_ref p) {
        if (cache(p).disc) {
            return *cache(p).disc;
        } else {
            auto upoly = carl::discriminant(as_univariate(p));
            assert(carl::is_constant(upoly));
            auto result = m_pool(Poly(upoly));
            assert(!is_zero(result));
            cache(p).disc = result;
            return result;
        }
    }

    poly_ref ldcf(poly_ref p) {
        if (cache(p).ldcf) {
            return *cache(p).ldcf;
        } else {
            auto result = m_pool(m_pool(p).lcoeff(main_var(p)));
            assert(!is_zero(result));
            cache(p).ldcf = result;
            return result;
        }
    }

    const std::vector<poly_ref>& factors_nonconst(poly_ref p) {
        if (cache(p).factors_nonconst.empty()) {
            for (const auto& factor : carl::irreducibleFactors(m_pool(p), false)) {
                cache(p).factors_nonconst.emplace_back(m_pool(factor));
            }
        }
        return cache(p).factors_nonconst;
    }

    size_t num_roots(const assignment& sample, poly_ref p) { // TODO cache
        auto rrs = carl::real_roots(as_univariate(p), sample);
        assert(rrs.is_univariate());
        return rrs.roots().size();
    }

    std::vector<ran> real_roots(const assignment& sample, poly_ref p) { // TODO cache
        auto rrs = carl::real_roots(as_univariate(p), sample);
        assert(rrs.is_univariate());
        return rrs.roots();
    }

    bool is_nullified(const assignment& sample, poly_ref p) { // TODO cache
        auto poly = m_pool(p);
		assert(!poly.isConstant());
		if (poly.isLinear()) return false;
		return carl::real_roots(as_univariate(p), sample).is_nullified();
    }

    bool is_zero(const assignment& sample, poly_ref p) { // TODO cache
        auto mv = carl::evaluate(ConstraintT(m_pool(p), carl::Relation::EQ), sample);
        assert(!indeterminate(mv));
        return (bool) mv;
    }

    bool is_ldcf_zero(const assignment& sample, poly_ref p) {
        return is_zero(sample, ldcf(p));
    }

    bool is_disc_zero(const assignment& sample, poly_ref p) {
        return is_zero(sample, disc(p));
    }

    bool is_const(poly_ref p) {
        return carl::is_constant(m_pool(p));
    }

    bool is_zero(poly_ref p) {
        return carl::is_zero(m_pool(p));
    }

    bool has_const_coeff(poly_ref p) const {
        auto poly = as_univariate(p);
        for (const auto& coeff :  poly.coefficients()) {
            if (coeff.isConstant() && !carl::isZero(coeff)) return true;
        }
        return false;
    }

    poly_ref simplest_nonzero_coeff(const assignment& sample, poly_ref p, std::function<bool(const Poly&,const Poly&)> compare) const {
        std::optional<Poly> result;
        auto poly = as_univariate(p);
        for (const auto& coeff : poly.coefficients()) {
            auto mv = carl::evaluate(ConstraintT(coeff, carl::Relation::NEQ), sample);
            assert(!indeterminate(mv));
            if (mv) {
                if (!result || compare(coeff,*result)) {
                    result = coeff;
                }
            }
        }
        assert(result);
        return m_pool(*result);
    }

};

}