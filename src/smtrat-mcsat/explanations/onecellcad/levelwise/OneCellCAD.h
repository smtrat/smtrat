#pragma once

/**
 * @file
 * Construct a single open CAD Cell IN A LEVEL-WISE MANNER around a given point that is sign-invariant
 * on a given set of polynomials. This is an adaptation of the already existing onecellcad in this project.
 *
 * @author Philippe Specht
 * Contact: philippe.specht@rwth-aachen.de
 *
 * References:
 * [brown15] Brown, Christopher W., and Marek Košta.
 * "Constructing a single cell in cylindrical algebraic decomposition."
 * Journal of Symbolic Computation 70 (2015): 14-48.
 * [mccallum84] Scott McCallum.
 * "An Improved Projection Operation for Cylindrical Algebraic Decomposition"
 * Ph.D. Dissertation. 1984. The University of Wisconsin - Madison
 */

#include "../Assertables.h"
#include "../utils.h"


namespace smtrat {
namespace mcsat {
namespace onecellcad {
namespace levelwise {

/**
* @param p Polynomial to get degree from
* @param v Rootvariable for degree calc
* @return
*/
inline int getDegree(TagPoly p, carl::Variable v) {
    return (int) carl::total_degree(carl::to_univariate_polynomial(p.poly, v));
}

class LevelwiseCAD : public OneCellCAD {
    public:
    using OneCellCAD::OneCellCAD;

    /**
      * Construct a single CADCell that contains the given 'point' and is
      * sign-invariant for the given polynomials in 'polys'.  The construction
      * fails if a polynomial vanishes ( p(a_1,...,a_i-1,x_i) ).  Note that
      * this cell is cylindrical only with respect to the given 'variableOrder'.
      *
      * @param variableOrder must contain unique variables and at least one,
      * because constant polynomials (without a variable) are prohibited.
      * @param point point.size() >= variables.size().
      * @param polys must contain only non-constant, irreducible tagged polynomials that
      * mention only variables that appear in 'variableOrder'.
      *
      */
    std::optional<CADCell> constructCADCellEnclosingPoint(
            std::vector<std::vector<TagPoly>> &polys, int sectionHeuristic, int sectorHeuristic) {
        SMTRAT_LOG_INFO("smtrat.cad", "Build CADcell enclosing point ");
        SMTRAT_LOG_DEBUG("smtrat.cad", "Variable order: " << variableOrder);
        SMTRAT_LOG_DEBUG("smtrat.cad", "Point: " << point);
        for (auto &poly : polys) {
            auto pVec = asMultiPolys(poly);
            SMTRAT_LOG_DEBUG("smtrat.cad", "Polys: " << pVec);
            assert(hasOnlyNonConstIrreducibles(pVec));
            assert(polyVarsAreAllInList(pVec, variableOrder));
        }

        CADCell cell = fullSpaceCell(point.dim());
        SMTRAT_LOG_DEBUG("smtrat.cad", "Cell: " << cell);

        for (int i = (int) point.dim() - 1; i >= 0; i--) {
            carl::Variable rootVariable = variableOrder[i];

            bool sector = true;
            TagPoly t;
            int deg = -1;
            for (auto &poly : polys[i]) {
                if (vanishesEarly(poly.level, poly.poly)) {
                    SMTRAT_LOG_WARN("smtrat.cad",
                                    "Building failed, " << poly.poly << " vanishes early at "
                                                        << point[i]);
                    return std::nullopt;
                } else if (isPointRootOfPoly(poly)) {
                    sector = false;
                    int locDeg = (int) carl::total_degree(
                            carl::to_univariate_polynomial(poly.poly, rootVariable));
                    assert(locDeg >= 1);
                    // for section: find defining polynomial with smallest degree in i-th variable
                    if (locDeg > deg) {
                        poly.tag = InvarianceType::ORD_INV;
                        t = poly;
                        deg = locDeg;
                    }
                }
            }

            const RAN pointComp = point[i];
            if (!sector) {
                /** Current level is a Section */
                SMTRAT_LOG_DEBUG("smtrat.cad", "Shrink cell sector at lvl " << i + 1);
                SMTRAT_LOG_TRACE("smtrat.cad", "Transform to section");
                SMTRAT_LOG_TRACE("smtrat.cad", "Defining poly: " << t.poly);
                SMTRAT_LOG_TRACE("smtrat.cad", "Lvl-Var: " << variableOrder[i]);
                SMTRAT_LOG_DEBUG("smtrat.cad", "PointComp: " << pointComp);

                assert(isNonConstIrreducible(t.poly));
                assert(t.level == (size_t) i);

                auto isolatedRoots = isolateLastVariableRoots(t.level, t.poly);
                assert(!isolatedRoots.empty());
                SMTRAT_LOG_TRACE("smtrat.cad", "Isolated roots: " << isolatedRoots);

                std::size_t rootIdx = 0;
                bool found = false;
                for (const auto &root : isolatedRoots) {
                    rootIdx++;
                    if (root == pointComp) {
                        SMTRAT_LOG_TRACE("smtrat.cad", "Equal: " << root);
                        cell[i] = Section{asRootExpr(rootVariable, t.poly, rootIdx), root};
                        SMTRAT_LOG_TRACE("smtrat.cad", "Resulting section: "
                                << (Section{asRootExpr(rootVariable, t.poly, rootIdx), root}));
                        found = true;
                        break;
                    }
                }
                assert(found);

                /** Projection part for section case*/
                if (i != 0) {
                    //Project polynomial that defines section
                    SMTRAT_LOG_TRACE("smtrat.cad", "Begin projection in section case");

                    Poly disc = discriminant(variableOrder[i], t.poly);
                    SMTRAT_LOG_TRACE("smtrat.cad", "Add discriminant: " << disc << " (if not const)");
                    appendOnCorrectLevel(disc, InvarianceType::ORD_INV, polys, variableOrder);


                    Poly ldcf = leadcoefficient(variableOrder[i], t.poly);
                    SMTRAT_LOG_TRACE("smtrat.cad",
                                     "Add leadcoefficient: " << ldcf << " (if not const)");
                    appendOnCorrectLevel(ldcf, InvarianceType::SIGN_INV, polys, variableOrder);


                    std::vector<std::tuple<RAN, TagPoly>> upper2;
                    std::vector<std::tuple<RAN, TagPoly>> lower2;
                    std::vector<std::tuple<RAN, TagPoly, int>> upper3;
                    std::vector<std::tuple<RAN, TagPoly, int>> lower3;
                    std::vector<std::pair<Poly, Poly>> resultants;
                    std::vector<Poly> noProjection1;
                    std::vector<Poly> noProjection2;
                    //project rest of polynomials
                    for (auto &poly : polys[i]) {

                        if (sectionHeuristic == 1) {
                            //Heuristic 1: calculate resultant between defining pol t and every pol that has root above or below t
                            if (!isolateLastVariableRoots(poly.level, poly.poly).empty()) {
                                if (poly.poly != t.poly) {
                                    resultants.emplace_back(std::make_pair(t.poly, poly.poly));
                                }
                            }

                        } else if (sectionHeuristic == 2) {
                            //Heuristic 2: calculate resultant in chain-form over lower2 and upper2
                            SMTRAT_LOG_TRACE("smtrat.cad", "Poly: " << poly.poly);
                            std::vector<RAN> isolatedRoots = isolateLastVariableRoots(poly.level,
                                                                                      poly.poly);

                            if (isolatedRoots.empty()) {
                                SMTRAT_LOG_TRACE("smtrat.cad", "No isolatable isolatedRoots");
                                continue;
                            } else {
                                SMTRAT_LOG_TRACE("smtrat.cad", "Isolated roots: " << isolatedRoots);
                            }

                            //guaranteed at least one root
                            //find closest root above and below sample (if existent) and put into upper2 and lower2 respectively
                            if (isolatedRoots.front() >= pointComp) {
                                //poly only has roots above pointComp
                                SMTRAT_LOG_DEBUG("smtrat.cad", "Smallest root above PointComp(1): "
                                        << isolatedRoots.front());
                                upper2.emplace_back(std::make_tuple(isolatedRoots.front(), poly));
                            } else if (isolatedRoots.back() <= pointComp) {
                                //poly only has roots below pointComp
                                SMTRAT_LOG_DEBUG("smtrat.cad", "Biggest root below PointComp(1): "
                                        << isolatedRoots.back());
                                lower2.emplace_back(std::make_tuple(isolatedRoots.back(), poly));
                            } else {
                                auto lb = std::lower_bound(isolatedRoots.begin(), isolatedRoots.end(),
                                                           pointComp);
                                if (*(lb - 1) == std::get<Section>(cell[i]).isolatedRoot) {
                                    SMTRAT_LOG_DEBUG("smtrat.cad", "Root at PointComp: " << *(lb - 1));
                                    lower2.emplace_back(std::make_tuple(*(lb - 1), poly));
                                } else {
                                    //poly has root above and below pointComp
                                    SMTRAT_LOG_DEBUG("smtrat.cad",
                                                     "Smallest root above PointComp(2): " << *lb);
                                    upper2.emplace_back(std::make_tuple(*lb, poly));
                                    SMTRAT_LOG_DEBUG("smtrat.cad",
                                                     "Biggest root below PointComp(2): " << *(lb - 1));
                                    lower2.emplace_back(std::make_tuple(*(lb - 1), poly));
                                }
                            }

                            //Additionally calculate disc and ldcf
                            disc = discriminant(variableOrder[i], poly.poly);
                            SMTRAT_LOG_TRACE("smtrat.cad",
                                             "Add discriminant: " << disc << " (if not const)");
                            appendOnCorrectLevel(disc, InvarianceType::ORD_INV, polys, variableOrder);

                            ldcf = leadcoefficient(variableOrder[i], poly.poly);
                            SMTRAT_LOG_TRACE("smtrat.cad",
                                             "Add leadcoefficient: " << ldcf << " (if not const)");
                            appendOnCorrectLevel(ldcf, InvarianceType::SIGN_INV, polys, variableOrder);

                        } else if (sectionHeuristic == 3) {
                            //Heuristic 3: smart
                            SMTRAT_LOG_TRACE("smtrat.cad", "Poly: " << poly.poly);
                            std::vector<RAN> isolatedRoots = isolateLastVariableRoots(poly.level, poly.poly);

                            if (isolatedRoots.empty()) {
                                SMTRAT_LOG_TRACE("smtrat.cad", "No isolatable isolatedRoots");
                                continue;
                            } else {
                                SMTRAT_LOG_TRACE("smtrat.cad", "Isolated roots: " << isolatedRoots);
                            }

                            //guaranteed at least one root
                            //find closest root above and below sample (if existent) and put into upper2 and lower2 respectively
                            int deg = (int) carl::total_degree(carl::to_univariate_polynomial(poly.poly, rootVariable));
                            if (isolatedRoots.front() >= pointComp) {
                                //poly only has roots above pointComp
                                SMTRAT_LOG_DEBUG("smtrat.cad", "Smallest root above PointComp(1): "
                                        << isolatedRoots.front());
                                upper3.emplace_back(std::make_tuple(isolatedRoots.front(), poly, deg));
                            } else if (isolatedRoots.back() <= pointComp) {
                                //poly only has roots below pointComp
                                SMTRAT_LOG_DEBUG("smtrat.cad", "Biggest root below PointComp(1): "
                                        << isolatedRoots.back());
                                lower3.emplace_back(std::make_tuple(isolatedRoots.back(), poly, deg));
                            } else {
                                auto lb = std::lower_bound(isolatedRoots.begin(), isolatedRoots.end(), pointComp);
                                if (*(lb - 1) == std::get<Section>(cell[i]).isolatedRoot) {
                                    SMTRAT_LOG_DEBUG("smtrat.cad", "Root at PointComp: " << *(lb - 1));
                                    lower3.emplace_back(std::make_tuple(*(lb - 1), poly, deg));
                                } else {
                                    //poly has root above and below pointComp
                                    SMTRAT_LOG_DEBUG("smtrat.cad",
                                                     "Smallest root above PointComp(2): " << *lb);
                                    upper3.emplace_back(std::make_tuple(*lb, poly, deg));
                                    SMTRAT_LOG_DEBUG("smtrat.cad",
                                                     "Biggest root below PointComp(2): " << *(lb - 1));
                                    lower3.emplace_back(std::make_tuple(*(lb - 1), poly, deg));
                                }
                            }
                        } else {
                            SMTRAT_LOG_WARN("smtrat.cad", "Building failed: Incorrect heuristic input");
                            return std::nullopt;
                        }

                        if (poly.poly != t.poly) {
                            if (isPointRootOfPoly(poly)) {
                                if (poly.tag == InvarianceType::ORD_INV) {
                                    SMTRAT_LOG_TRACE("smtrat.cad", "Check for vanishing coefficient");
                                    auto coeff = coeffNonNull(poly);
                                    if (coeff.has_value()) {
                                        SMTRAT_LOG_TRACE("smtrat.cad",
                                                         "Add result of coeffNonNull: " << coeff.value()
                                                                                        << " (if not const)");
                                        appendOnCorrectLevel(coeff.value(), InvarianceType::SIGN_INV,
                                                             polys, variableOrder);
                                    }
                                    if (sectionHeuristic == 1) {
                                        //otherwise the discriminant has already been calculated
                                        disc = discriminant(variableOrder[i], poly.poly);
                                        SMTRAT_LOG_TRACE("smtrat.cad", "Add discriminant: " << disc
                                                                                            << " (if not const)");
                                        appendOnCorrectLevel(disc, InvarianceType::ORD_INV, polys,
                                                             variableOrder);
                                    }
                                }
                            } else {
                                poly.tag = InvarianceType::ORD_INV;
                            }
                        }
                    }

                    if (sectionHeuristic == 2) {
                        //sort closest roots of pols below and above sample
                        std::sort(lower2.begin(), lower2.end(), [](auto const &t1, auto const &t2) {
                            return std::get<0>(t1) < std::get<0>(t2);
                        });
                        std::sort(upper2.begin(), upper2.end(), [](auto const &t1, auto const &t2) {
                            return std::get<0>(t1) < std::get<0>(t2);
                        });

                        //calculate resultants
                        if (!lower2.empty()) {
                            for (auto it = lower2.begin(); it != lower2.end() - 1; it++) {
                                resultants.emplace_back(std::make_pair(std::get<1>(*it).poly,
                                                                       std::get<1>(*(it + 1)).poly));
                            }
                        }

                        if (!lower2.empty() && !upper2.empty()) {
                            resultants.emplace_back(std::make_pair(std::get<1>(lower2.back()).poly,
                                                                   std::get<1>(upper2.front()).poly));
                        }

                        if (!upper2.empty()) {
                            for (auto it = upper2.begin(); it != upper2.end() - 1; it++) {
                                resultants.emplace_back(std::make_pair(std::get<1>(*it).poly,
                                                                       std::get<1>(*(it + 1)).poly));
                            }
                        }
                    } else if (sectionHeuristic == 3) {
                        //sort closest roots of pols below and above sample
                        std::sort(lower3.begin(), lower3.end(), [](auto const &t1, auto const &t2) {
                            return std::get<0>(t1) < std::get<0>(t2);
                        });
                        std::sort(upper3.begin(), upper3.end(), [](auto const &t1, auto const &t2) {
                            return std::get<0>(t1) < std::get<0>(t2);
                        });

                        //calculate resultants
                        if (!lower3.empty()) {
                            //optimization: for multiple entries with the same root in lower3, sort the one with the
                            //  lowest degree to the smallest possible position for optimal resultant calculation
                            for (auto it = lower3.begin() + 1; it != lower3.end(); it++) {
                                if (std::get<0>(*(it - 1)) == std::get<0>(*it) &&
                                    std::get<2>(*(it - 1)) < std::get<2>(*it)) {
                                    std::iter_swap(it - 1, it);
                                }
                            }

                            while (lower3.size() != 1) {
                                auto cur = std::min_element(lower3.rbegin(), lower3.rend() - 1,
                                                            [](auto const &t1, auto const &t2) {
                                                                return std::get<2>(t1) <
                                                                       std::get<2>(t2);
                                                            });

                                auto it = cur + 1;
                                while (it != lower3.rend()) {
                                    resultants.emplace_back(std::make_pair(std::get<1>(*cur).poly,
                                                                           std::get<1>(*it).poly));
                                    it++;
                                }
                                //Reverse the reverse iterator to use erase
                                lower3.erase(lower3.begin(), cur.base() - 1);
                            }

                            // optimization: find polynomials only connected to bound t because they dont need disc and ldcf
                            if(!resultants.empty()) {
                                resultants = duplicateElimination(resultants);
                                for (auto &res : resultants) {
                                    if (res.first == t.poly) {
                                        noProjection1.push_back(res.second);
                                    }
                                    if (res.second == t.poly) {
                                        noProjection1.push_back(res.first);
                                    }
                                }
                                if(!noProjection1.empty()) {
                                    noProjection1 = duplicateElimination(noProjection1);
                                    bool inc;
                                    for (auto it = noProjection1.begin(); it != noProjection1.end();) {
                                        inc = true;
                                        for (auto &res : resultants) {
                                            if ((res.first == *it && res.second != t.poly) ||
                                                (res.second == *it && res.first != t.poly)) {
                                                it = noProjection1.erase(it);
                                                inc = false;
                                                break;
                                            }
                                        }
                                        if(inc){it++;}
                                    }
                                }
                            }
                        }

                        std::vector<std::pair<Poly, Poly>> tmpResultants;
                        if (!upper3.empty()) {
                            //optimization: for multiple entries with the same root in upper3, sort the one with the
                            //  lowest degree to the smallest possible position for optimal resultant calculation
                            for (auto it = upper3.begin() + 1; it != upper3.end(); it++) {
                                if (std::get<0>(*(it - 1)) == std::get<0>(*it) &&
                                    std::get<2>(*(it - 1)) > std::get<2>(*it)) {
                                    std::iter_swap(it - 1, it);
                                }
                            }

                            while (upper3.size() != 1) {
                                auto cur = std::min_element(upper3.begin(), upper3.end() - 1,
                                                            [](auto const &t1, auto const &t2) { return std::get<2>(t1) < std::get<2>(t2); });

                                auto it = cur + 1;
                                while (it != upper3.end()) {
                                    tmpResultants.emplace_back(std::make_pair(std::get<1>(*cur).poly, std::get<1>(*it).poly));
                                    it++;
                                }

                                upper3.erase(cur + 1, upper3.end());
                            }

                            // optimization: find polynomials only connected to bound t because they dont need disc and ldcf
                            if(!tmpResultants.empty()) {
                                tmpResultants = duplicateElimination(tmpResultants);
                                for (auto &res : tmpResultants) {
                                    if (res.first == t.poly) {
                                        noProjection2.push_back(res.second);
                                    }
                                    if (res.second == t.poly) {
                                        noProjection2.push_back(res.first);
                                    }
                                }
                                if(!noProjection2.empty()) {
                                    noProjection2 = duplicateElimination(noProjection2);
                                    bool inc;
                                    for (auto it = noProjection2.begin(); it != noProjection2.end();) {
                                        inc = true;
                                        for (auto &res : tmpResultants) {
                                            if ((res.first == *it && res.second != t.poly) ||
                                                (res.second == *it && res.first != t.poly)) {
                                                it = noProjection2.erase(it);
                                                inc = false;
                                                break;
                                            }
                                        }
                                        if(inc){it++;}
                                    }
                                }
                                resultants.insert(resultants.end(), tmpResultants.begin(), tmpResultants.end());
                                tmpResultants.clear();
                            }
                        }

                        if (!lower3.empty() && !upper3.empty()) {
                            resultants.emplace_back(std::make_pair(std::get<1>(lower3.back()).poly,
                                                                   std::get<1>(upper3.front()).poly));
                        }

                        //Additionally calculate disc and ldcf (if necessary)
                        for (auto &poly : polys[i]) {
                            if (!contains(noProjection1, poly.poly) && !contains(noProjection2, poly.poly)) {
                                disc = discriminant(variableOrder[i], poly.poly);
                                SMTRAT_LOG_TRACE("smtrat.cad",
                                                 "Add discriminant: " << disc << " (if not const)");
                                appendOnCorrectLevel(disc, InvarianceType::ORD_INV, polys,
                                                     variableOrder);

                                ldcf = leadcoefficient(variableOrder[i], poly.poly);
                                SMTRAT_LOG_TRACE("smtrat.cad",
                                                 "Add leadcoefficient: " << ldcf << " (if not const)");
                                appendOnCorrectLevel(ldcf, InvarianceType::SIGN_INV, polys,
                                                     variableOrder);
                            }
                        }
                    }


                    // Add all calculate resultants (independent from heuristic)
                    addResultants(resultants, polys, variableOrder[i], variableOrder);

                }
            } else {
                /** Current level is a Sector */
                Sector &sector = std::get<Sector>(cell[i]);
                SMTRAT_LOG_DEBUG("smtrat.cad", "Shrink cell sector at lvl " << i + 1);
                SMTRAT_LOG_DEBUG("smtrat.cad", "Lvl-var: " << variableOrder[i]);
                SMTRAT_LOG_DEBUG("smtrat.cad", "PointComp: " << pointComp);
                SMTRAT_LOG_DEBUG("smtrat.cad", "Determine sector, currently: " << sector);

                std::vector<TagPoly> upper1;
                std::vector<TagPoly> lower1;
                std::vector<std::tuple<RAN, TagPoly, int>> upper2;
                std::vector<std::tuple<RAN, TagPoly, int>> lower2;
                std::vector<std::tuple<RAN, TagPoly, int, int>> upper3;
                std::vector<std::tuple<RAN, TagPoly, int, int>> lower3;
                std::vector<Poly> needsNoLdcf;
                TagPoly curUp;
                TagPoly curLow;

                // Different heuristics for resultant calculation need different setups of control data
                // Level 1 does not need control data at all
                if (i == 0) {
                    for (const auto &poly : polys[i]) {
                        SMTRAT_LOG_TRACE("smtrat.cad", "Poly: " << poly.poly);

                        auto isolatedRoots = isolateLastVariableRoots(poly.level, poly.poly);

                        if (isolatedRoots.empty()) {
                            SMTRAT_LOG_TRACE("smtrat.cad", "No isolatable isolatedRoots");
                            continue;
                        } else {
                            SMTRAT_LOG_TRACE("smtrat.cad", "Isolated roots: " << isolatedRoots);
                        }

                        // Search for closest isolatedRoots/boundPoints to pointComp, i.e.
                        // someRoot ... < closestLower < pointComp < closestUpper < ... someOtherRoot
                        std::optional<RAN> closestLower, closestUpper;
                        int rootIdx = 0, lowerRootIdx = 0, upperRootIdx = 0;

                        for (const auto &root : isolatedRoots) {
                            rootIdx++;
                            if (root < pointComp) {
                                SMTRAT_LOG_TRACE("smtrat.cad", "Smaller: " << root);
                                if (!closestLower || *closestLower < root) {
                                    closestLower = root;
                                    lowerRootIdx = rootIdx;
                                    curLow = poly;
                                }
                            } else { // pointComp < root
                                SMTRAT_LOG_TRACE("smtrat.cad", "Bigger: " << root);
                                if (!closestUpper || root < *closestUpper) {
                                    closestUpper = root;
                                    upperRootIdx = rootIdx;
                                    curUp = poly;
                                }
                                //break out of loop since isolatedRoots is sorted
                                break;
                            }
                        }

                        if (closestLower) {
                            if (!sector.lowBound || (*(sector.lowBound)).isolatedRoot < closestLower) {
                                sector.lowBound = Section{
                                        asRootExpr(rootVariable, poly.poly, lowerRootIdx),
                                        *closestLower};
                                SMTRAT_LOG_TRACE("smtrat.cad", "New lower bound: "
                                        << " " << sector);
                            }
                        }

                        if (closestUpper) {
                            if (!sector.highBound ||
                                closestUpper < (*(sector.highBound)).isolatedRoot) {
                                sector.highBound = Section{
                                        asRootExpr(rootVariable, poly.poly, upperRootIdx),
                                        *closestUpper};
                                SMTRAT_LOG_TRACE("smtrat.cad", "New upper bound: "
                                        << " " << sector);
                            }
                        }
                    }

                } else if (sectorHeuristic == 1) {
                    // for convenience, upper3 (lower3 respectively) is used here to save all possible upper bounds
                    // => poly with smallest degree in i-th variable is then chosen
                    std::optional<RAN> closestLower, closestUpper;
                    for (const auto &poly : polys[i]) {
                        SMTRAT_LOG_TRACE("smtrat.cad", "Poly: " << poly.poly);

                        auto isolatedRoots = isolateLastVariableRoots(poly.level, poly.poly);

                        if (isolatedRoots.empty()) {
                            SMTRAT_LOG_TRACE("smtrat.cad", "No isolatable isolatedRoots");
                            needsNoLdcf.emplace_back(poly.poly);
                            continue;
                        } else {
                            SMTRAT_LOG_TRACE("smtrat.cad", "Isolated roots: " << isolatedRoots);
                        }

                        // Search for closest isolatedRoots/boundPoints to pointComp, i.e.
                        // someRoot ... < closestLower < pointComp < closestUpper < ... someOtherRoot
                        bool up = false, low = false;
                        int rootIdx = 0;
                        for (const auto &root : isolatedRoots) {
                            rootIdx++;
                            if (root < pointComp) {
                                SMTRAT_LOG_TRACE("smtrat.cad", "Smaller: " << root);
                                low = true;
                                if (!closestLower || *closestLower < root) {
                                    closestLower = root;
                                    lower3.clear();
                                    lower3.emplace_back(std::make_tuple(root, poly, rootIdx, 0));
                                } else if (*closestLower == root) {
                                    lower3.emplace_back(std::make_tuple(root, poly, rootIdx, 0));
                                }
                            } else { // pointComp < root
                                SMTRAT_LOG_TRACE("smtrat.cad", "Bigger: " << root);
                                up = true;
                                if (!closestUpper || root < *closestUpper) {
                                    closestUpper = root;
                                    upper3.clear();
                                    upper3.emplace_back(std::make_tuple(root, poly, rootIdx, 0));
                                } else if (*closestUpper == root) {
                                    upper3.emplace_back(std::make_tuple(root, poly, rootIdx, 0));
                                } else {
                                    // Optimization: break out of loop since isolatedRoots is sorted
                                    break;
                                }
                            }
                        }

                        if (low) {
                            lower1.push_back(poly);
                        }

                        if (up) {
                            upper1.push_back(poly);
                        }
                    }

                    // Set bounds according to degree that is first calculated
                    if (!lower3.empty()) {
                        for (auto elem : lower3) {
                            std::get<3>(elem) = getDegree(std::get<1>(elem), rootVariable);
                        }
                        auto smallest = *std::min_element(lower3.begin(), lower3.end(),
                                                          [](auto const &t1, auto const &t2) {
                                                              return std::get<3>(t1) < std::get<3>(t2);
                                                          });

                        curLow = std::get<1>(smallest);
                        sector.lowBound = Section{
                                asRootExpr(rootVariable, curLow.poly, std::get<2>(smallest)),
                                std::get<0>(smallest)};
                        SMTRAT_LOG_TRACE("smtrat.cad", "New lower bound: "
                                << " " << sector);
                    }

                    if (!upper3.empty()) {
                        for (auto elem : upper3) {
                            std::get<3>(elem) = getDegree(std::get<1>(elem), rootVariable);
                        }
                        auto smallest = *std::min_element(upper3.begin(), upper3.end(),
                                                          [](auto const &t1, auto const &t2) {
                                                              return std::get<3>(t1) < std::get<3>(t2);
                                                          });

                        curUp = std::get<1>(smallest);
                        sector.highBound = Section{
                                asRootExpr(rootVariable, curUp.poly, std::get<2>(smallest)),
                                std::get<0>(smallest)};
                        SMTRAT_LOG_TRACE("smtrat.cad", "New upper bound: "
                                << " " << sector);
                    }


                } else if (sectorHeuristic == 2) {
                    //while determining bounds create lists upper2 and lower2 which are sorted by their order around the sample
                    for (const auto &poly : polys[i]) {
                        SMTRAT_LOG_TRACE("smtrat.cad", "Poly: " << poly.poly);
                        std::vector<RAN> isolatedRoots = isolateLastVariableRoots(poly.level,
                                                                                  poly.poly);

                        if (isolatedRoots.empty()) {
                            SMTRAT_LOG_TRACE("smtrat.cad", "No isolatable isolatedRoots");
                            needsNoLdcf.emplace_back(poly.poly);
                            continue;
                        } else {
                            SMTRAT_LOG_TRACE("smtrat.cad", "Isolated roots: " << isolatedRoots);
                        }

                        //guaranteed at least one root
                        //find closest root above and below sample (if existent) and put into upper2 and lower2 respectively
                        if (isolatedRoots.front() > pointComp) {
                            //poly only has roots above pointComp
                            SMTRAT_LOG_DEBUG("smtrat.cad", "Smallest root above PointComp(1): "
                                    << isolatedRoots.front());
                            upper2.emplace_back(std::make_tuple(isolatedRoots.front(), poly, 1));
                        } else if (isolatedRoots.back() < pointComp) {
                            //poly only has roots below pointComp
                            SMTRAT_LOG_DEBUG("smtrat.cad", "Biggest root below PointComp(1): "
                                    << isolatedRoots.back());
                            lower2.emplace_back(std::make_tuple(isolatedRoots.back(), poly,
                                                                isolatedRoots.end() -
                                                                isolatedRoots.begin()));
                        } else {
                            auto lb = std::lower_bound(isolatedRoots.begin(), isolatedRoots.end(),
                                                       pointComp);
                            //poly has root above and below pointComp
                            SMTRAT_LOG_DEBUG("smtrat.cad", "Smallest root above PointComp(2): " << *lb);
                            upper2.emplace_back(
                                    std::make_tuple(*lb, poly, (int) (lb - isolatedRoots.begin()) + 1));
                            SMTRAT_LOG_DEBUG("smtrat.cad",
                                             "Biggest root below PointComp(2): " << *(lb - 1));
                            lower2.emplace_back(std::make_tuple(*(lb - 1), poly,
                                                                (int) (lb - isolatedRoots.begin())));
                        }
                    }

                    //sort closest roots of pols below and above sample
                    std::sort(lower2.begin(), lower2.end(), [](auto const &t1, auto const &t2) {
                        return std::get<0>(t1) < std::get<0>(t2);
                    });
                    std::sort(upper2.begin(), upper2.end(), [](auto const &t1, auto const &t2) {
                        return std::get<0>(t1) < std::get<0>(t2);
                    });

                    if (!lower2.empty()) {
                        // find lower bound with smallest degree in i-th variable
                        auto curPos = lower2.rbegin();
                        int curDeg = -1;
                        auto it = lower2.rbegin() + 1;
                        while (it != lower2.rend()) {
                            if (std::get<0>(*it) == std::get<0>(*curPos)) {
                                if (curDeg == -1) {
                                    curDeg = getDegree(std::get<1>(*curPos), rootVariable);
                                }
                                int degree = getDegree(std::get<1>(*it), rootVariable);
                                if (degree < curDeg) {
                                    curPos = it;
                                    curDeg = degree;
                                }
                                it++;
                            } else {
                                break;
                            }
                        }
                        std::iter_swap(curPos.base() - 1, lower2.end() - 1);


                        //set lower bound
                        curLow = std::get<1>(lower2.back());
                        sector.lowBound = Section{
                                asRootExpr(rootVariable, curLow.poly, std::get<2>(lower2.back())),
                                std::get<0>(lower2.back())};
                        SMTRAT_LOG_TRACE("smtrat.cad", "Lower bound: "
                                << " " << *sector.lowBound);

                    } else {
                        SMTRAT_LOG_TRACE("smtrat.cad", "Open lower bound");
                    }

                    if (!upper2.empty()) {
                        // find upper bound with smallest degree in i-th variable
                        auto curPos = upper2.begin();
                        int curDeg = -1;
                        auto it = upper2.begin() + 1;
                        while (it != upper2.end()) {
                            if (std::get<0>(*it) == std::get<0>(*curPos)) {
                                if (curDeg == -1) {
                                    curDeg = getDegree(std::get<1>(*curPos), rootVariable);
                                }
                                int degree = getDegree(std::get<1>(*it), rootVariable);
                                if (degree < curDeg) {
                                    curPos = it;
                                    curDeg = degree;
                                }
                                it++;
                            } else {
                                break;
                            }
                        }
                        std::iter_swap(curPos, upper2.begin());

                        //set upper bound
                        curUp = std::get<1>(upper2.front());
                        sector.highBound = Section{
                                asRootExpr(rootVariable, curUp.poly, std::get<2>(upper2.front())),
                                std::get<0>(upper2.front())};
                        SMTRAT_LOG_TRACE("smtrat.cad", "Upper bound: "
                                << " " << *sector.highBound);
                    } else {
                        SMTRAT_LOG_TRACE("smtrat.cad", "Open upper bound");
                    }

                } else if (sectorHeuristic == 3) {
                    //while determining bounds create lists upper3 and lower3 which are sorted by their order around the sample
                    for (const auto &poly : polys[i]) {
                        SMTRAT_LOG_TRACE("smtrat.cad", "Poly: " << poly.poly);
                        std::vector<RAN> isolatedRoots = isolateLastVariableRoots(poly.level,
                                                                                  poly.poly);

                        if (isolatedRoots.empty()) {
                            SMTRAT_LOG_TRACE("smtrat.cad", "No isolatable isolatedRoots");
                            needsNoLdcf.emplace_back(poly.poly);
                            continue;
                        } else {
                            SMTRAT_LOG_TRACE("smtrat.cad", "Isolated roots: " << isolatedRoots);
                        }

                        //guaranteed at least one root
                        //find closest root above and below sample (if existent) and put into upper3 and lower3 respectively
                        int deg = (int) carl::total_degree(
                                carl::to_univariate_polynomial(poly.poly, rootVariable));
                        if (isolatedRoots.front() > pointComp) {
                            //poly only has roots above pointComp
                            SMTRAT_LOG_DEBUG("smtrat.cad", "Smallest root above PointComp(1): "
                                    << isolatedRoots.front());
                            upper3.emplace_back(std::make_tuple(isolatedRoots.front(), poly, 1, deg));
                        } else if (isolatedRoots.back() < pointComp) {
                            //poly only has roots below pointComp
                            SMTRAT_LOG_DEBUG("smtrat.cad", "Biggest root below PointComp(1): "
                                    << isolatedRoots.back());
                            lower3.emplace_back(std::make_tuple(isolatedRoots.back(), poly,
                                                                isolatedRoots.end() -
                                                                isolatedRoots.begin(), deg));
                        } else {
                            auto lb = std::lower_bound(isolatedRoots.begin(), isolatedRoots.end(),
                                                       pointComp);
                            //poly has root above and below pointComp
                            SMTRAT_LOG_DEBUG("smtrat.cad", "Smallest root above PointComp(2): " << *lb);
                            upper3.emplace_back(
                                    std::make_tuple(*lb, poly, (int) (lb - isolatedRoots.begin()) + 1,
                                                    deg));
                            SMTRAT_LOG_DEBUG("smtrat.cad",
                                             "Biggest root below PointComp(2): " << *(lb - 1));
                            lower3.emplace_back(
                                    std::make_tuple(*(lb - 1), poly, (int) (lb - isolatedRoots.begin()),
                                                    deg));
                        }
                    }

                    //sort closest roots of pols below and above sample
                    std::sort(lower3.begin(), lower3.end(), [](auto const &t1, auto const &t2) {
                        return std::get<0>(t1) < std::get<0>(t2);
                    });
                    std::sort(upper3.begin(), upper3.end(), [](auto const &t1, auto const &t2) {
                        return std::get<0>(t1) < std::get<0>(t2);
                    });


                    if (!lower3.empty()) {
                        //optimization: for multiple entries with the same root in lower3, sort the one with the
                        //  lowest degree to the smallest possible position for optimal resultant calculation
                        for (auto it = lower3.begin() + 1; it != lower3.end(); it++) {
                            if (std::get<0>(*(it - 1)) == std::get<0>(*it) &&
                                std::get<2>(*(it - 1)) < std::get<2>(*it)) {
                                std::iter_swap(it - 1, it);
                            }
                        }

                        //set lower bound
                        curLow = std::get<1>(lower3.back());
                        sector.lowBound = Section{
                                asRootExpr(rootVariable, curLow.poly, std::get<2>(lower3.back())),
                                std::get<0>(lower3.back())};
                        SMTRAT_LOG_TRACE("smtrat.cad", "Lower bound: "
                                << " " << sector);

                    } else {
                        SMTRAT_LOG_TRACE("smtrat.cad", "Open lower bound");
                    }

                    if (!upper3.empty()) {
                        //optimization: for multiple entries with the same root in upper3, sort the one with the
                        //  lowest degree to the smallest possible position for optimal resultant calculation
                        for (auto it = upper3.begin() + 1; it != upper3.end(); it++) {
                            if (std::get<0>(*(it - 1)) == std::get<0>(*it) &&
                                std::get<2>(*(it - 1)) > std::get<2>(*it)) {
                                std::iter_swap(it - 1, it);
                            }
                        }

                        //set upper bound
                        curUp = std::get<1>(upper3.front());
                        sector.highBound = Section{
                                asRootExpr(rootVariable, curUp.poly, std::get<2>(upper3.front())),
                                std::get<0>(upper3.front())};
                        SMTRAT_LOG_TRACE("smtrat.cad", "Upper bound: "
                                << " " << sector);

                    } else {
                        SMTRAT_LOG_TRACE("smtrat.cad", "Open upper bound");
                    }
                } else {
                    SMTRAT_LOG_WARN("smtrat.cad", "Building failed: Incorrect heuristic input");
                    return std::nullopt;
                }
                SMTRAT_LOG_TRACE("smtrat.cad", "Determined bounds of sector: " << sector);


                /** Projection part for sector case*/
                if (i != 0) {
                    SMTRAT_LOG_TRACE("smtrat.cad", "Begin projection in sector case");
                    for (auto &poly : polys[i]) {
                        //Add discriminant
                        Poly disc = discriminant(variableOrder[i], poly.poly);
                        SMTRAT_LOG_TRACE("smtrat.cad",
                                         "Add discriminant(" << poly.poly << ") = " << disc
                                                             << " (if not const)");
                        appendOnCorrectLevel(disc, InvarianceType::ORD_INV, polys, variableOrder);

                        //Add leadcoefficient if necessary
                        if (!sector.highBound.has_value() || !sector.lowBound.has_value() ||
                            !contains(needsNoLdcf, poly.poly)) {
                            Poly ldcf = leadcoefficient(variableOrder[i], poly.poly);
                            SMTRAT_LOG_TRACE("smtrat.cad",
                                             "Add leadcoefficient(" << poly.poly << ") = " << ldcf
                                                                    << " (if not const)");
                            appendOnCorrectLevel(ldcf, InvarianceType::SIGN_INV, polys, variableOrder);
                        }

                        SMTRAT_LOG_TRACE("smtrat.cad", "Check for vanishing coefficient");
                        auto coeff = coeffNonNull(poly);
                        if (coeff.has_value()) {
                            SMTRAT_LOG_TRACE("smtrat.cad",
                                             "Add result of coeffNonNull: " << coeff.value()
                                                                            << " (if not const)");
                            appendOnCorrectLevel(coeff.value(), InvarianceType::SIGN_INV, polys,
                                                 variableOrder);
                        }

                        poly.tag = InvarianceType::ORD_INV;
                    }

                    //Calculate and comulatively append resultants
                    std::vector<std::pair<Poly, Poly>> resultants;

                    if (sector.lowBound.has_value() && sector.highBound.has_value() &&
                        sector.lowBound->boundFunction.poly() !=
                        sector.highBound->boundFunction.poly()) {

                        resultants.emplace_back(std::make_pair(curLow.poly, curUp.poly));
                    }

                    if (sectorHeuristic == 1) {
                        //Heuristic 1: calculate resultant between upper/lower bound and every polynomial above/below
                        if (sector.lowBound.has_value()) {
                            for (const auto &l : lower1) {
                                if (l.poly != curLow.poly) {
                                    resultants.emplace_back(std::make_pair(l.poly, curLow.poly));
                                }
                            }
                        }

                        if (sector.highBound.has_value()) {
                            for (const auto &u : upper1) {
                                if (u.poly != curUp.poly) {
                                    resultants.emplace_back(std::make_pair(u.poly, curUp.poly));
                                }
                            }
                        }

                        addResultants(resultants, polys, variableOrder[i], variableOrder);

                        lower1.clear();
                        upper1.clear();
                        lower3.clear();
                        upper3.clear();
                    } else if (sectorHeuristic == 2) {
                        //Heuristic 2: calculate resultant in chain-form over lower2 and upper2
                        if (sector.lowBound.has_value()) {
                            for (auto it = lower2.begin(); it != lower2.end() - 1; it++) {
                                resultants.emplace_back(std::make_pair(std::get<1>(*it).poly,
                                                                       std::get<1>(*(it + 1)).poly));
                            }
                        }

                        if (sector.highBound.has_value()) {
                            for (auto it = upper2.begin(); it != upper2.end() - 1; it++) {
                                resultants.emplace_back(std::make_pair(std::get<1>(*it).poly,
                                                                       std::get<1>(*(it + 1)).poly));
                            }
                        }

                        addResultants(resultants, polys, variableOrder[i], variableOrder);

                        lower2.clear();
                        upper2.clear();
                    } else if (sectorHeuristic == 3) {
                        //heuristic 3: smart
                        if (!lower3.empty()) {
                            while (lower3.size() != 1) {
                                auto cur = std::min_element(lower3.rbegin(), lower3.rend() - 1,
                                                            [](auto const &t1, auto const &t2) {
                                                                return std::get<3>(t1) <
                                                                       std::get<3>(t2);
                                                            });

                                auto it = cur + 1;
                                while (it != lower3.rend()) {
                                    resultants.emplace_back(std::get<1>(*cur).poly,
                                                            std::get<1>(*it).poly);
                                    it++;
                                }
                                //Reverse the reverse iterator to use erase
                                lower3.erase(lower3.begin(), cur.base() - 1);
                            }
                        }

                        if (!upper3.empty()) {
                            while (upper3.size() != 1) {
                                auto cur = std::min_element(upper3.begin(), upper3.end() - 1,
                                                            [](auto const &t1, auto const &t2) {
                                                                return std::get<3>(t1) <
                                                                       std::get<3>(t2);
                                                            });

                                auto it = cur + 1;
                                while (it != upper3.end()) {
                                    resultants.emplace_back(std::get<1>(*cur).poly,
                                                            std::get<1>(*it).poly);
                                    it++;
                                }

                                upper3.erase(cur + 1, upper3.end());
                            }
                        }

                        addResultants(resultants, polys, variableOrder[i], variableOrder);

                        lower3.clear();
                        upper3.clear();
                    } else {
                        SMTRAT_LOG_WARN("smtrat.cad", "Building failed: Incorrect heuristic input");
                        return std::nullopt;
                    }
                    /** optimize memory*/
                    resultants.clear();
                    needsNoLdcf.clear();
                } else {
                    SMTRAT_LOG_TRACE("smtrat.cad", "Level 1, so no projection");
                }
            }
            /** optimize memory*/
            polys[i].clear();
        }

        SMTRAT_LOG_DEBUG("smtrat.cad", "Finished Cell: " << cell);
        assert(isMainPointInsideCell(cell));
        return cell;
    }
};


} // namespace levelwise
} // namespace onecellcad
} // namespace mcsat
} // namespace smtrat