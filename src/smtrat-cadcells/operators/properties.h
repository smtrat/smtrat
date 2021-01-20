#pragma once

#include <functional>
#include "../datastructures/polynomials.h"

namespace smtrat::cadcells::operators::properties {

struct poly_sgn_inv {
    datastructures::poly_ref poly;
    size_t level() const {
        return poly.level;
    }
    std::size_t hash_on_level() const {
        return std::hash<std::size_t>()(poly.id);
    }
};
bool operator==(const poly_sgn_inv& lhs, const poly_sgn_inv& rhs) {
    return lhs.poly == rhs.poly;
}
std::ostream& operator<<(std::ostream& os, const poly_sgn_inv& data) {
    os << data.poly << " si";
    return os;
}

struct poly_irreducible_sgn_inv {
    datastructures::poly_ref poly;   
    size_t level() const {
        return poly.level;
    }
     std::size_t hash_on_level() const {
        return std::hash<std::size_t>()(poly.id);
    }
};
bool operator==(const poly_irreducible_sgn_inv& lhs, const poly_irreducible_sgn_inv& rhs) {
    return lhs.poly == rhs.poly;
}
std::ostream& operator<<(std::ostream& os, const poly_irreducible_sgn_inv& data) {
    os << data.poly << " si and irreducible";
    return os;
}

struct poly_ord_inv {
    datastructures::poly_ref poly;
    size_t level() const {
        return poly.level;
    }
    std::size_t hash_on_level() const {
        return std::hash<std::size_t>()(poly.id);
    }
};
bool operator==(const poly_ord_inv& lhs, const poly_ord_inv& rhs) {
    return lhs.poly == rhs.poly;
}
std::ostream& operator<<(std::ostream& os, const poly_ord_inv& data) {
    os << data.poly << " oi";
    return os;
}

struct root_well_def {
    datastructures::indexed_root root;
    size_t level() const {
        return root.poly.level-1;
    }
    std::size_t hash_on_level() const {
        return std::hash<std::size_t>()(root.poly.id);
    }
};
bool operator==(const root_well_def& lhs, const root_well_def& rhs) {
    return lhs.root == rhs.root;
}
std::ostream& operator<<(std::ostream& os, const root_well_def& data) {
    os << data.root.poly << " " << data.root.index << " well-def";
    return os;
}

struct poly_pdel {
    datastructures::poly_ref poly;
    size_t level() const {
        return poly.level-1;
    }
    std::size_t hash_on_level() const {
        return std::hash<std::size_t>()(poly.id);
    }
};
bool operator==(const poly_pdel& lhs, const poly_pdel& rhs) {
    return lhs.poly == rhs.poly;
}
std::ostream& operator<<(std::ostream& os, const poly_pdel& data) {
    os << data.poly << " projectively delineable";
    return os;
}

}