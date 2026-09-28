#include "holdfast/hungarian.hpp"

#include <cmath>

namespace holdfast {

namespace {

// Classic shortest-augmenting-path Hungarian with row/column potentials.
// Requires n <= m. a is 1-indexed (n+1) x (m+1). Returns p[j] = row assigned to column j.
std::vector<int> hungarian_rows_le_cols(const std::vector<std::vector<double>>& a, int n, int m) {
    const double INF = std::numeric_limits<double>::max() / 4;
    std::vector<double> u(n + 1, 0), v(m + 1, 0);
    std::vector<int> p(m + 1, 0), way(m + 1, 0);
    for (int i = 1; i <= n; ++i) {
        p[0] = i;
        int j0 = 0;
        std::vector<double> minv(m + 1, INF);
        std::vector<char> used(m + 1, false);
        do {
            used[j0] = true;
            const int i0 = p[j0];
            double delta = INF;
            int j1 = 0;
            for (int j = 1; j <= m; ++j) {
                if (used[j]) continue;
                const double cur = a[i0][j] - u[i0] - v[j];
                if (cur < minv[j]) { minv[j] = cur; way[j] = j0; }
                if (minv[j] < delta) { delta = minv[j]; j1 = j; }
            }
            for (int j = 0; j <= m; ++j) {
                if (used[j]) { u[p[j]] += delta; v[j] -= delta; }
                else { minv[j] -= delta; }
            }
            j0 = j1;
        } while (p[j0] != 0);
        do {
            const int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }
    return p;
}

}  // namespace

std::vector<std::pair<int, int>> solve_assignment(const CostMatrix& cost, double max_cost) {
    std::vector<std::pair<int, int>> out;
    if (cost.rows == 0 || cost.cols == 0) return out;

    const bool transpose = cost.rows > cost.cols;
    const int n = transpose ? cost.cols : cost.rows;
    const int m = transpose ? cost.rows : cost.cols;

    // Infeasible pairs get a penalty larger than any feasible total, so the solver
    // maximizes the number of feasible matches first and only then minimizes cost.
    double max_feasible = 0.0;
    bool any_feasible = false;
    for (double c : cost.c) {
        if (std::isfinite(c) && c <= max_cost) { max_feasible = std::max(max_feasible, std::abs(c)); any_feasible = true; }
    }
    if (!any_feasible) return out;
    const double big = (max_feasible + 1.0) * (n + 1);

    std::vector<std::vector<double>> a(n + 1, std::vector<double>(m + 1, 0.0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            const double c = transpose ? cost(j, i) : cost(i, j);
            a[i + 1][j + 1] = (std::isfinite(c) && c <= max_cost) ? c : big;
        }
    }

    const auto p = hungarian_rows_le_cols(a, n, m);
    for (int j = 1; j <= m; ++j) {
        if (p[j] == 0) continue;
        const int i = p[j] - 1, k = j - 1;
        const int r = transpose ? k : i, c = transpose ? i : k;
        const double v = cost(r, c);
        if (std::isfinite(v) && v <= max_cost) out.emplace_back(r, c);
    }
    return out;
}

}  // namespace holdfast
