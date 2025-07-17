#include "export.h"

#include <parse_verilog/if_statement.h>
#include <parse_verilog/block_statement.h>
#include <parse_verilog/continuous.h>
#include <parse_verilog/trigger.h>

#include <interpret_arithmetic/export_verilog.h>

namespace flow {

parse_verilog::assignment_statement export_assign(ucs::ConstNetlist nets, clocked::Assign assign) {
	parse_verilog::assignment_statement result;
	result.valid = true;
	result.name = ucs::Net(nets.netAt(assign.net));
	result.blocking = assign.blocking;
	result.expr = parse_verilog::export_expression(assign.expr, nets);
	return result;
}

parse_verilog::continuous export_continuous(ucs::ConstNetlist nets, clocked::Assign assign, bool force) {
	parse_verilog::continuous result;
	result.valid = true;
	result.force = force;
	if (assign.expr.isNull()) {
		result.deassign = ucs::Net(nets.netAt(assign.net));
	} else {
		result.assign = export_assign(nets, assign);
	}
	return result;
}

parse_verilog::declaration export_declaration(string type, ucs::Net name, int msb, int lsb, bool input, bool output) {
	parse_verilog::declaration result;
	result.valid = true;
	result.input = input;
	result.output = output;
	if (msb > lsb) {
		result.msb = parse_verilog::export_expression(arithmetic::Value::intOf(msb));
		result.lsb = parse_verilog::export_expression(arithmetic::Value::intOf(lsb));
	}
	result.type = type;
	result.name = name.to_string();
	return result;
}

parse_verilog::block_statement export_block(const vector<clocked::Statement> &stmts, ucs::ConstNetlist nets) {
	parse_verilog::block_statement body;
	body.valid = true;
	for (auto k = stmts.begin(); k != stmts.end(); k++) {
		if (k->is(Statement::ASSIGN)) {
			if (mod.nets[k->net].purpose != clocked::Net::REG) {
				printf("error: found stateful assignments on wire '%s'\n", mod.nets[k->net].name.c_str());
			}
			body.sub.push_back(shared_ptr<parse::syntax>(new parse_verilog::assignment_statement(export_assign(mod, *k))));
		} else if (k->is(Statement::IF) or k->is(Statement::ELIF)) {
			parse_verilog::if_statement *cond = nullptr;
			if (i->is(statement::IF)) {
				cond = new parse_verilog::if_statement();
				cond->valid = true;
				body.sub.push_back(shared_ptr<parse::syntax>(cond));
			} else {
				if (not body.sub.back()->is_a<parse_verilog::if_statement>()) {
					printf("error:%s:%d: else if expected preceding if statement\n", __FILE__, __LINE__);
					continue;
				}

				cond = (parse_verilog::if_statement*)body.sub.back();
			}

			if (k->expr.isValid()) {
				cond->condition.push_back(parse_verilog::export_expression(k->expr, nets));
			}

			cond->body.push_back(export_block(k->stmt, nets));
		} else {
			printf("error:%s:%d: unrecognized statement type %d\n", __FILE__, __LINE__, k->kind);
			continue;
		}
	}

	return body;
}

parse_verilog::trigger export_trigger(const clocked::Block &block, ucs::ConstNetlist nets) {
	parse_verilog::trigger *always = new parse_verilog::trigger();
	always->valid = true;
	always->condition.valid = true;
	always->condition.level = parse_verilog::expression::get_level("posedge");
	always->condition.arguments.push_back(parse_verilog::export_expression(i->clk, mod));
	always->condition.operations.push_back("posedge");
	always->body.valid = true;
	always->body.sub.push_back(shared_ptr<parse::syntax>(export_block(block.stmt, nets)));
	return always;
}

parse_verilog::module_def export_module(const clocked::Module &mod) {
	parse_verilog::module_def result;
	result.valid = true;
	result.name = mod.name;

	for (int i = 0; i < (int)mod.nets.size(); i++) {
		if (mod.nets[i].purpose == clocked::Net::IN) {
			result.ports.push_back(export_declaration("wire", mod.nets[i].name, mod.nets[i].type.width-1, 0, true, false));
		} else if (mod.nets[i].purpose == clocked::Net::OUT) {
			result.ports.push_back(export_declaration("wire", mod.nets[i].name, mod.nets[i].type.width-1, 0, false, true));
		} else if (mod.nets[i].purpose == clocked::Net::WIRE) {
			result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::declaration(export_declaration("wire", mod.nets[i].name, mod.nets[i].type.width-1, 0, false, false))));
		} else if (mod.nets[i].purpose == clocked::Net::REG) {
			result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::declaration(export_declaration("reg", mod.nets[i].name, mod.nets[i].type.width-1, 0, false, false))));
		}
	}

	for (auto i = mod.stmt.begin(); i != mod.stmt.end(); i++) {
		if (i->is(Statement::ASSIGN)) {
			result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::continuous(export_continuous(mod, *i))));
		} else {
			internal(__FILE__, __LINE__, "sequential statements must be inside a block");
			continue;
		}
	}

	for (auto i = mod.blocks.begin(); i != mod.blocks.end(); i++) {
		result.items.push_back(shared_ptr<parse::syntax>(export_trigger(*i, mod)));	
	}
	
	return result;
}

}
