#include "export_verilog.h"

#include <parse_verilog/if_statement.h>
#include <parse_verilog/block_statement.h>
#include <parse_verilog/continuous.h>
#include <parse_verilog/trigger.h>

#include <interpret_arithmetic/export_verilog.h>
#include <interpret_arithmetic/export.h>

namespace flow {

parse_verilog::assignment_statement export_assign(ucs::ConstNetlist nets, clocked::Statement assign) {
	parse_verilog::setup_expressions();

	parse_verilog::assignment_statement result;
	result.valid = true;
	result.lvalue = arithmetic::export_net<parse_verilog::expression>(assign.net, nets);
	result.blocking = assign.blocking;
	result.expr = parse_verilog::export_expression(assign.expr, nets);
	return result;
}

parse_verilog::continuous export_continuous(ucs::ConstNetlist nets, clocked::Statement assign, bool force) {
	parse_verilog::setup_expressions();

	parse_verilog::continuous result;
	result.valid = true;
	result.force = force;
	if (assign.expr.isNull()) {
		result.deassign = arithmetic::export_net<parse_verilog::expression>(assign.net, nets);
	} else {
		result.assign = export_assign(nets, assign);
	}
	return result;
}

parse_verilog::declaration export_declaration(string type, ucs::Net name, int msb, int lsb, bool input, bool output) {
	parse_verilog::setup_expressions();

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

parse_verilog::block_statement export_block(ucs::ConstNetlist nets, const vector<clocked::Statement> &stmts) {
	parse_verilog::block_statement body;
	body.valid = true;
	for (auto k = stmts.begin(); k != stmts.end(); k++) {
		if (k->type == clocked::Statement::ASSIGN) {
			/*if (mod.nets[k->net].purpose != clocked::Net::REG) {
				printf("error: found stateful assignments on wire '%s'\n", mod.nets[k->net].name.c_str());
			}*/
			body.sub.push_back(shared_ptr<parse::syntax>(new parse_verilog::assignment_statement(export_assign(nets, *k))));
		} else if (k->type == clocked::Statement::IF or k->type == clocked::Statement::ELIF) {
			parse_verilog::if_statement *cond = nullptr;
			if (k->type == clocked::Statement::IF) {
				cond = new parse_verilog::if_statement();
				cond->valid = true;
				body.sub.push_back(shared_ptr<parse::syntax>(cond));
			} else {
				if (not body.sub.back()->is_a<parse_verilog::if_statement>()) {
					printf("error:%s:%d: else if expected preceding if statement\n", __FILE__, __LINE__);
					continue;
				}

				cond = (parse_verilog::if_statement*)body.sub.back().get();
			}

			if (not k->expr.isUndef() and not k->expr.isValid()) {
				cond->condition.push_back(parse_verilog::export_expression(k->expr, nets));
			}

			cond->body.push_back(export_block(nets, k->sub));
		} else {
			printf("error:%s:%d: unrecognized statement type %d\n", __FILE__, __LINE__, k->type);
			continue;
		}
	}

	return body;
}

parse_verilog::trigger export_trigger(ucs::ConstNetlist nets, const clocked::Trigger &trigger) {
	static const auto posedgeOp = parse_verilog::expression::precedence.find(parse_expression::operation_set::UNARY, "posedge", "", "", "");

	parse_verilog::trigger always;
	always.valid = true;

	if (posedgeOp.level < 0 or posedgeOp.index < 0) {
		internal("", "unable to find \"posedge\" operator", __FILE__, __LINE__);
	} else {
		always.condition.valid = true;
		always.condition.level = posedgeOp.level;
		always.condition.arguments.push_back(parse_verilog::export_expression(trigger.clk, nets));
		always.condition.operators.push_back(posedgeOp.index);
	}

	always.body = export_block(nets, trigger.stmts);
	return always;
}

parse_verilog::module_instance export_instance(ucs::ConstNetlist nets, const clocked::Instance &inst) {
	parse_verilog::module_instance result;
	result.valid = true;
	result.module_type = inst.type;
	result.instance_name = inst.name;
	for (const Expression &port : inst.ports) {
		parse_verilog::port_connection connect;
		connect.expr = parse_verilog::export_expression(port, nets);
		result.connections.push_back(connect);
	}
	return result;
}

parse_verilog::module_def export_module(const clocked::Module &mod) {
	parse_verilog::setup_expressions();

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

	for (auto i = mod.stmts.begin(); i != mod.stmts.end(); i++) {
		result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::continuous(export_continuous(mod, *i))));
	}

	for (auto i = mod.triggers.begin(); i != mod.triggers.end(); i++) {
		result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::trigger(export_trigger(mod, *i))));
	}
	
	for (auto i = mod.inst.begin(); i != mod.inst.end(); i++) {
		result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::module_instance(export_instance(mod, *i))));
	}

	return result;
}

}
