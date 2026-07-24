#pragma once

#include "types.hpp"

#include "db/pool.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace minioj::db {

std::vector<ProblemSummary> listProblems(ConnectionPool& pool, const ProblemFilters& filters);
std::optional<ProblemDetail> getProblemDetail(ConnectionPool& pool, std::uint64_t problem_id);

}