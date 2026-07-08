#pragma once

#include <common/net.h>

#include <parse_verilog/module.h>
#include <parse_verilog/assignment_statement.h>
#include <parse_verilog/continuous.h>
#include <parse_verilog/block_statement.h>
#include <parse_verilog/trigger.h>
#include <parse_verilog/module_instance.h>
#include <flow/module.h>

namespace flow {

parse_verilog::assignment_statement export_assign(ucs::ConstNetlist nets, clocked::Statement assign);
parse_verilog::continuous export_continuous(ucs::ConstNetlist nets, clocked::Statement assign, bool force=false);
parse_verilog::declaration export_declaration(string type, std::string name, int msb=0, int lsb=0, bool input=false, bool output=false);
parse_verilog::block_statement export_block(ucs::ConstNetlist nets, const vector<clocked::Statement> &stmts);
parse_verilog::trigger export_trigger(ucs::ConstNetlist nets, const clocked::Trigger &trigger);
parse_verilog::module_instance export_instance(ucs::ConstNetlist nets, const clocked::Instance &inst);
parse_verilog::module_def export_module(const clocked::Module &mod);

}
