#pragma once

#include <common/standard.h>
#include <common/net.h>

#include <parse_verilog/module.h>
#include <parse_verilog/assignment_statement.h>
#include <parse_verilog/continuous.h>
#include <parse_verilog/block_statement.h>
#include <parse_verilog/trigger.h>
#include <parse_verilog/module_instance.h>
#include <parse_verilog/expression.h>

#include <arithmetic/expression.h>
#include <arithmetic/state.h>
#include <arithmetic/action.h>

#include <flow/module.h>

#include <interpret_arithmetic/export.h>

namespace parse_verilog {

string export_value(const arithmetic::Value &v);

struct ExpressionExporter : arithmetic::ExpressionExporter {
	ucs::ConstNetlist nets;

	ExpressionExporter(ucs::ConstNetlist nets);
	~ExpressionExporter();

	parse_expression::operation export_operator(int func) const override;
	const parse_expression::precedence_set &precedence() const override;
	parse_expression::expression::argument export_constant(arithmetic::Value value) const override;
	parse_expression::expression::argument export_literal(size_t index) const override;
	parse_expression::expression export_special(int func, const vector<parse_expression::expression::argument> &args) const override;

	parse_expression::expression export_boolean_xor(const vector<parse_expression::expression::argument> &args) const;
};

parse_expression::expression export_expression(const arithmetic::Expression &expr, ucs::ConstNetlist nets);

assignment_statement export_assign(ucs::ConstNetlist nets, clocked::Statement assign);
continuous export_continuous(ucs::ConstNetlist nets, clocked::Statement assign, bool force=false);
declaration export_declaration(string type, std::string name, int msb=0, int lsb=0, bool input=false, bool output=false);
block_statement export_block(ucs::ConstNetlist nets, const vector<clocked::Statement> &stmts);
trigger export_trigger(ucs::ConstNetlist nets, const clocked::Trigger &trigger);
module_instance export_instance(ucs::ConstNetlist nets, const clocked::Instance &inst);
module_def export_module(const clocked::Module &mod);

}
