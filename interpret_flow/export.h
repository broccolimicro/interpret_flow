#pragma once

#include <common/net.h>

#include <parse_verilog/module.h>
#include <parse_verilog/assignment_statement.h>
#include <parse_verilog/continuous.h>
#include <flow/module.h>

namespace flow {

parse_verilog::assignment_statement export_assign(ucs::ConstNetlist nets, clocked::Assign assign);
parse_verilog::continuous export_continuous(ucs::ConstNetlist nets, clocked::Assign assign, bool force=false);
parse_verilog::declaration export_declaration(string type, ucs::Net name, int msb=0, int lsb=0, bool input=false, bool output=false);
parse_verilog::module_def export_module(const clocked::Module &mod);

}
