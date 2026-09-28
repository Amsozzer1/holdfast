#pragma once

#include <limits>
#include <utility>
#include <vector>

namespace holdfast {

inline constexpr double kInfeasible = std::numeric_limits<double>::infinity();

// Row-major rows x cols cost matrix.
struct CostMatrix {
    int rows = 0, cols = 0;
    std::vector<double> c;

    CostMatrix(int r, int k, double fill = kInfeasible) : rows(r), cols(k), c(static_cast<size_t>(r) * k, fill) {}
    double& operator()(int r, int k) { return c[static_cast<size_t>(r) * cols + k]; }
    double operator()(int r, int k) const { return c[static_cast<size_t>(r) * cols + k]; }
};

// Optimal assignment (Kuhn-Munkres with potentials, O(n^2 m)).
// Entries that are infinite, or above max_cost, are never assigned. Among assignments,
// it first maximizes the number of feasible pairs, then minimizes their total cost.
// Returns (row, col) pairs.
std::vector<std::pair<int, int>> solve_assignment(const CostMatrix& cost, double max_cost = kInfeasible);

}  // namespace holdfast
