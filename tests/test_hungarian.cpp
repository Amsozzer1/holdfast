#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numeric>

#include "holdfast/hungarian.hpp"
#include "holdfast/rng.hpp"

using namespace holdfast;

namespace {

// Brute force over all partial matchings: maximize #feasible pairs, then minimize cost.
std::pair<int, double> brute_force(const CostMatrix& C) {
    const bool tr = C.rows > C.cols;
    const int n = tr ? C.cols : C.rows, m = tr ? C.rows : C.cols;
    auto at = [&](int i, int j) { return tr ? C(j, i) : C(i, j); };
    std::vector<int> cols(m);
    std::iota(cols.begin(), cols.end(), 0);
    int best_n = -1;
    double best_c = 0;
    do {
        int cnt = 0;
        double c = 0;
        for (int i = 0; i < n; ++i) {
            if (std::isfinite(at(i, cols[i]))) { ++cnt; c += at(i, cols[i]); }
        }
        if (cnt > best_n || (cnt == best_n && c < best_c - 1e-9)) { best_n = cnt; best_c = c; }
    } while (std::next_permutation(cols.begin(), cols.end()));
    return {best_n, best_c};
}

std::pair<int, double> score(const CostMatrix& C, const std::vector<std::pair<int, int>>& a) {
    double c = 0;
    for (auto [r, k] : a) c += C(r, k);
    return {static_cast<int>(a.size()), c};
}

}  // namespace

TEST(Hungarian, EmptyAndTrivial) {
    EXPECT_TRUE(solve_assignment(CostMatrix(0, 3)).empty());
    CostMatrix one(1, 1, 0.3);
    auto a = solve_assignment(one);
    ASSERT_EQ(a.size(), 1u);
    EXPECT_EQ(a[0], std::make_pair(0, 0));
}

TEST(Hungarian, MatchesBruteForceOnRandomMatrices) {
    Rng rng(7);
    for (int trial = 0; trial < 600; ++trial) {
        const int r = rng.uniform_int(1, 7), c = rng.uniform_int(1, 7);
        CostMatrix C(r, c);
        const double p_inf = trial % 3 == 0 ? 0.4 : 0.0;
        for (auto& v : C.c) v = rng.bernoulli(p_inf) ? kInfeasible : rng.uniform(0, 10);
        const auto a = solve_assignment(C);
        // No row or column used twice.
        std::vector<int> ru(r, 0), cu(c, 0);
        for (auto [i, j] : a) { ASSERT_EQ(ru[i]++, 0); ASSERT_EQ(cu[j]++, 0); ASSERT_TRUE(std::isfinite(C(i, j))); }
        const auto [bn, bc] = brute_force(C);
        const auto [sn, sc] = score(C, a);
        ASSERT_EQ(sn, bn) << "trial " << trial;
        ASSERT_NEAR(sc, bc, 1e-6) << "trial " << trial;
    }
}

TEST(Hungarian, AllInfeasibleRowIsLeftOut) {
    CostMatrix C(3, 3, 1.0);
    for (int j = 0; j < 3; ++j) C(1, j) = kInfeasible;
    const auto a = solve_assignment(C);
    EXPECT_EQ(a.size(), 2u);
    for (auto [i, j] : a) EXPECT_NE(i, 1);
}

TEST(Hungarian, MaxCostThresholdsPairs) {
    CostMatrix C(2, 2);
    C(0, 0) = 0.1; C(0, 1) = 0.9;
    C(1, 0) = 0.9; C(1, 1) = 0.95;
    const auto a = solve_assignment(C, 0.5);
    ASSERT_EQ(a.size(), 1u);
    EXPECT_EQ(a[0], std::make_pair(0, 0));
}
