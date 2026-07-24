#pragma once

#include "http/admin_request.hpp"
#include "types.hpp"

#include "db/pool.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace minioj::db {

std::vector<ProblemSummary> listProblems(ConnectionPool& pool, const ProblemFilters& filters);
std::optional<ProblemDetail> getProblemDetail(ConnectionPool& pool, std::uint64_t problem_id);

std::vector<AdminProblemDetail> listFullProblems(ConnectionPool& pool);
std::optional<AdminProblemDetail> getFullProblem(ConnectionPool& pool, std::uint64_t problem_id);

std::uint64_t createProblem(ConnectionPool& pool, const admin::ProblemInput& input);
bool updateProblem(ConnectionPool& pool, std::uint64_t problem_id, const admin::ProblemInput& input);
bool deleteProblem(ConnectionPool& pool, std::uint64_t problem_id);

}