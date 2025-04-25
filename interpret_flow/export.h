#pragma once

#include <parse_verilog/module.h>
#include <parse_verilog/assignment_statement.h>
#include <parse_verilog/continuous.h>
#include <flow/module.h>
#include <interpret_arithmetic/interface.h>

namespace flow {

parse_verilog::assignment_statement export_assign(arithmetic::ConstNetlist nets, clocked::Assign assign);
parse_verilog::continuous export_continuous(arithmetic::ConstNetlist nets, clocked::Assign assign, bool force=false);
parse_verilog::declaration export_declaration(string type, string name, int msb=0, int lsb=0, bool input=false, bool output=false);
parse_verilog::module_def export_module(const clocked::Module &mod);

}
