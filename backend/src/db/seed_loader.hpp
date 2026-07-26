#pragma once

#include "db/pool.hpp"

#include <string>
#include <string_view>

namespace minioj::db {

// 清空题库数据（problem_tags / testcases / problems / tags）；
// 保留 users / sessions 不动。表清空顺序按外键级联方向：
// 先子表（problem_tags / testcases），后父表（problems / tags）。
void clearProblemBank(ConnectionPool& pool);

// 从 JSON 文件读入题库并写入 DB。
// JSON 顶层为数组，每个元素是一个 ProblemInput 结构（字段：
// title / description_md / difficulty / time_limit_ms / memory_limit_mb
// / tags / testcases），字段约束同 admin::parseProblemInput。
//
// 任意一个 problem 解析失败会抛出 std::runtime_error；
// 调用方应在事务外整体包裹（失败时回滚）。
void loadProblemsFromJson(ConnectionPool& pool, std::string_view json_path);

// 一键重置：clearProblemBank + loadProblemsFromJson。
void resetProblemBank(ConnectionPool& pool, std::string_view seed_json_path);

}