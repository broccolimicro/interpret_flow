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

		auto always = make_shared<parse_verilog::trigger>();
		always->valid = true;
		always->condition.valid = true;
		always->condition.level = parse_verilog::expression::get_level("posedge");
		always->condition.arguments.push_back(parse_verilog::export_expression(i->clk, mod));
		always->condition.operations.push_back("posedge");

		// Create the main always block body
		auto always_body = make_shared<parse_verilog::block_statement>();
		always_body->valid = true;

		auto always_if = make_shared<parse_verilog::if_statement>();
		always_if->valid = true;

		// Add trigger reset condition
		always_if->condition.push_back(parse_verilog::export_expression(Operand::varOf(mod.reset), mod));

		auto reset_body = make_shared<parse_verilog::block_statement>();
		reset_body->valid = true;
		for (const auto& reset_assign : i->reset) {
			reset_body->sub.push_back(
				make_shared<parse_verilog::assignment_statement>(
					export_assign(mod, reset_assign)));
		}
		always_if->body.push_back(*reset_body);

		// Add handshake for each branch
		for (const auto& rule : i->rules) {
			//if (rule.guard.isValid()) {
			always_if->condition.push_back(parse_verilog::export_expression(rule.guard, mod));
			//}

			auto rule_body = make_shared<parse_verilog::block_statement>();
			rule_body->valid = true;

			for (const auto& assign : rule.assign) {
				rule_body->sub.push_back(
					make_shared<parse_verilog::assignment_statement>(
						export_assign(mod, assign)));
			}

			always_if->body.push_back(*rule_body);
		}

		// For each _else rule, append it as an independent condition within always_if's terminal "else" body
		if (!i->_else.empty()) {
			auto else_body = make_shared<parse_verilog::block_statement>();
			else_body->valid = true;

			for (const auto& else_rule : i->_else) {
				auto rule_if = make_shared<parse_verilog::if_statement>();
				rule_if->valid = true;

				//if (else_rule.guard.isValid()) {
				rule_if->condition.push_back(parse_verilog::export_expression(else_rule.guard, mod));
				//}

				auto rule_body = make_shared<parse_verilog::block_statement>();
				rule_body->valid = true;

				for (const auto& assign : else_rule.assign) {
					rule_body->sub.push_back(
						make_shared<parse_verilog::assignment_statement>(
							export_assign(mod, assign)));
				}

				rule_if->body.push_back(*rule_body);
				else_body->sub.push_back(rule_if);
			}

			always_if->body.push_back(*else_body);
		}

		always_body->sub.push_back(always_if);
		always->body = *always_body;
		result.items.push_back(always);
	}

	return result;
}

}
