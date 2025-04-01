#include "export.h"

#include <parse_verilog/if_statement.h>
#include <parse_verilog/block_statement.h>
#include <parse_verilog/continuous.h>
#include <parse_verilog/trigger.h>

#include <interpret_arithmetic/export_verilog.h>

parse_verilog::assignment_statement export_assign(arithmetic::ConstNetlist nets, int name, arithmetic::expression expr, bool blocking) {
	parse_verilog::assignment_statement result;
	result.valid = true;
	result.name = parse_verilog::export_variable_name(name, nets);
	result.blocking = blocking;
	result.expr = parse_verilog::export_expression(expr, nets);
	return result;
}

parse_verilog::continuous export_continuous(arithmetic::ConstNetlist nets, int name, arithmetic::expression expr, bool blocking, bool force) {
	parse_verilog::continuous result;
	result.valid = true;
	result.force = force;
	if (expr.is_null()) {
		result.deassign = parse_verilog::export_variable_name(name, nets);
	} else {
		result.assign = export_assign(nets, name, expr, blocking);
	}
	return result;
}

parse_verilog::declaration export_declaration(string type, string name, int msb, int lsb, bool input, bool output) {
	parse_verilog::declaration result;
	result.valid = true;
	result.input = input;
	result.output = output;
	if (msb > lsb) {
		result.msb = parse_verilog::export_expression(arithmetic::value(msb));
		result.lsb = parse_verilog::export_expression(arithmetic::value(lsb));
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

	for (int i = 0; i < (int)mod.nets.size(); i++) {
		if (not mod.nets[i].rules.empty() and mod.nets[i].purpose == clocked::Net::WIRE) {
			if (mod.nets[i].rules.size() > 1u or not mod.nets[i].rules[0].guard.is_valid()) {
				printf("error: found stateful assignments on wire '%s'\n", mod.nets[i].name.c_str());
			}
			result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::continuous(export_continuous(mod, i, mod.nets[i].rules[0].assign))));
		}
	}

	for (int i = 0; i < (int)mod.nets.size(); i++) {
		if (not mod.nets[i].rules.empty() and mod.nets[i].purpose == clocked::Net::REG) {
			parse_verilog::if_statement *cond = new parse_verilog::if_statement();
			cond->valid = true;
			cond->condition.push_back(parse_verilog::export_expression(operand(mod.reset, operand::variable), mod));
			parse_verilog::block_statement reset;
			reset.valid = true;
			reset.sub.push_back(shared_ptr<parse::syntax>(new parse_verilog::assignment_statement(export_assign(mod, i, value(0), true))));
			cond->body.push_back(reset);

			for (int j = 0; j < (int)mod.nets[i].rules.size(); j++) {
				if (not mod.nets[i].rules[j].guard.is_valid()) {
					cond->condition.push_back(parse_verilog::export_expression(mod.nets[i].rules[j].guard, mod));
				}

				parse_verilog::block_statement body;
				body.valid = true;
				body.sub.push_back(shared_ptr<parse::syntax>(new parse_verilog::assignment_statement(export_assign(mod, i, mod.nets[i].rules[j].assign, true))));
				cond->body.push_back(body);

				if (mod.nets[i].rules[j].guard.is_valid()) {
					if (j != (int)mod.nets[i].rules.size()-1) {
						printf("warning: ineffective conditions found in stateful assignment for net '%s'\n", mod.nets[i].name.c_str());
					}
					break;
				}
			}

			parse_verilog::trigger *always = new parse_verilog::trigger();
			always->valid = true;
			always->condition.valid = true;
			always->condition.arguments.push_back(parse_verilog::export_expression(operand(mod.clk, operand::variable), mod));
			always->condition.operations.push_back("posedge");
			always->body.valid = true;
			always->body.sub.push_back(shared_ptr<parse::syntax>(cond));
			result.items.push_back(shared_ptr<parse::syntax>(always));
		}
	}

	return result;
}

