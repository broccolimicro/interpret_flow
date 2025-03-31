#pragma once

#include <parse_verilog/module.h>
#include <parse_verilog/assignment_statement.h>
#include <parse_verilog/continuous.h>
#include <flow/graph.h>

parse_verilog::continuous export_continuous(const ucs::variable_set &variables, ucs::variable name, arithmetic::expression expr=arithmetic::expression(), bool blocking=false, bool force=false);
parse_verilog::assignment_statement export_assign(const ucs::variable_set &variables, ucs::variable name, arithmetic::expression expr, bool blocking=false);
parse_verilog::declaration export_declaration(string type, string name, int msb=0, int lsb=0, bool input=false, bool output=false);
void export_port(parse_verilog::module_def &dst, const flow::Port &port, bool out, const ucs::variable_set &vars);
parse_verilog::module_def export_module(const flow::Func &func);
