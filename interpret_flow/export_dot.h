#pragma once

#include <common/standard.h>
#include <flow/func.h>
#include <parse_dot/node_id.h>
#include <parse_dot/attribute_list.h>
#include <parse_dot/statement.h>
#include <parse_dot/graph.h>

namespace flow {

parse_dot::graph export_func(const Func &f, bool format_expressions_as_html_table=false);

}
