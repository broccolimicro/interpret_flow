#include "export.h"

#include <parse_verilog/if_statement.h>
#include <parse_verilog/block_statement.h>
#include <parse_verilog/continuous.h>
#include <parse_verilog/trigger.h>

#include <interpret_arithmetic/export_verilog.h>

parse_verilog::assignment_statement export_assign(arithmetic::ConstNetlist nets, clocked::Assign assign) {
	parse_verilog::assignment_statement result;
	result.valid = true;
	result.name = parse_verilog::export_variable_name(assign.net, nets);
	result.blocking = assign.blocking;
	result.expr = parse_verilog::export_expression(assign.expr, nets);
	return result;
}

parse_verilog::continuous export_continuous(arithmetic::ConstNetlist nets, clocked::Assign assign, bool force) {
	parse_verilog::continuous result;
	result.valid = true;
	result.force = force;
	if (assign.expr.isNull()) {
		result.deassign = parse_verilog::export_variable_name(assign.net, nets);
	} else {
		result.assign = export_assign(nets, assign);
	}
	return result;
}

parse_verilog::declaration export_declaration(string type, string name, int msb, int lsb, bool input, bool output) {
	parse_verilog::declaration result;
	result.valid = true;
	result.input = input;
	result.output = output;
	if (msb > lsb) {
		result.msb = parse_verilog::export_expression(arithmetic::Value::intOf(msb));
		result.lsb = parse_verilog::export_expression(arithmetic::Value::intOf(lsb));
	}
	result.type = type;
	result.name = name;
	return result;
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

	for (auto i = mod.assign.begin(); i != mod.assign.end(); i++) {
		result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::continuous(export_continuous(mod, *i))));
	}

	for (auto i = mod.blocks.begin(); i != mod.blocks.end(); i++) {
		parse_verilog::if_statement *cond = new parse_verilog::if_statement();
		cond->valid = true;
		cond->condition.push_back(parse_verilog::export_expression(Operand::varOf(mod.reset), mod));
		parse_verilog::block_statement reset;
		reset.valid = true;
		for (auto j = i->reset.begin(); j != i->reset.end(); j++) {
			reset.sub.push_back(shared_ptr<parse::syntax>(new parse_verilog::assignment_statement(export_assign(mod, *j))));
		}
		cond->body.push_back(reset);

		bool done = false;
		for (auto j = i->rules.begin(); j != i->rules.end() and not done; j++) {
			if (not j->guard.isValid()) {
				cond->condition.push_back(parse_verilog::export_expression(j->guard, mod));
			}

			parse_verilog::block_statement body;
			body.valid = true;
			for (auto k = j->assign.begin(); k != j->assign.end(); k++) {
				if (mod.nets[k->net].purpose != clocked::Net::REG) {
					printf("error: found stateful assignments on wire '%s'\n", mod.nets[k->net].name.c_str());
				}

				body.sub.push_back(shared_ptr<parse::syntax>(new parse_verilog::assignment_statement(export_assign(mod, *k))));
			}
			cond->body.push_back(body);

			if (j->guard.isValid()) {
				if (std::next(j) != i->rules.end()) {
					printf("warning: ineffective conditions found in stateful assignment\n");
				}
				done = true;
			}
		}

		parse_verilog::trigger *always = new parse_verilog::trigger();
		always->valid = true;
		always->condition.valid = true;
		always->condition.level = parse_verilog::expression::get_level("posedge");
		always->condition.arguments.push_back(parse_verilog::export_expression(i->clk, mod));
		always->condition.operations.push_back("posedge");
		always->body.valid = true;
		always->body.sub.push_back(shared_ptr<parse::syntax>(cond));
		result.items.push_back(shared_ptr<parse::syntax>(always));	
	}
	
	return result;
}

