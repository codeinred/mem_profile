#pragma once

#include <algorithm>
#include <cli/cstr.h>
#include <vector>

namespace cli {

/// Computes the levenshtein distance between two strings
inline size_t levenshtein_dist(string_view a, string_view b) {
    const size_t m = a.length();
    const size_t n = b.length();

    // Handle edge cases
    if (m == 0) return n;
    if (n == 0) return m;

    // Create two work vectors for the algorithm
    std::vector<size_t> v0(n + 1);
    std::vector<size_t> v1(n + 1);

    // Initialize v0 (the previous row of distances)
    // This row represents the edit distance from an empty string to b
    for (size_t i = 0; i <= n; ++i) {
        v0[i] = i;
    }

    // Main algorithm loop
    for (size_t i = 0; i < m; ++i) {
        // First element of v1 is the edit distance from a[0..i] to empty string
        // which is just deleting (i + 1) characters
        v1[0] = i + 1;

        // Fill in the rest of the current row
        for (size_t j = 0; j < n; ++j) {
            // Calculate the three possible costs
            size_t deletion_cost     = v0[j + 1] + 1;
            size_t insertion_cost    = v1[j] + 1;
            size_t substitution_cost = (a[i] == b[j]) ? v0[j] : v0[j] + 1;

            // Take the minimum of the three costs
            v1[j + 1] = std::min({deletion_cost, insertion_cost, substitution_cost});
        }

        // Swap the rows for the next iteration
        std::swap(v0, v1);
    }

    // After the last swap, the result is in v0[n]
    return v0[n];
}
} // namespace cli
