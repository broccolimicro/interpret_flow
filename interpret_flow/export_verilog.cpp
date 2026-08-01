#include "export_verilog.h"

#include <common/message.h>

#include <parse_verilog/if_statement.h>
#include <parse_verilog/block_statement.h>
#include <parse_verilog/continuous.h>
#include <parse_verilog/trigger.h>
#include <parse_verilog/expression.h>

#include <arithmetic/algorithm.h>

#include <parse/wrapper.h>

namespace parse_verilog {

string export_value(const arithmetic::Value &v) {
	if (v.type == arithmetic::Value::TYPE) {
		return v.sval;
	} else if (v.type == arithmetic::Value::TERM) {
		return v.sval;
	} else if (v.isUnstable()) {
		return "X";
	} else if (v.isUnknown()) {
		return "U";
	} else if (v.isNeutral()) {
		return "0";
	} else if (v.type == arithmetic::Value::WIRE) {
		return "1";
	} else if (v.type == arithmetic::Value::STRING) {
		return "\"" + v.sval + "\"";
	} else if (v.type == arithmetic::Value::BOOL) {
		return v.bval ? "1'b1" : "1'b0";
	} else if (v.type == arithmetic::Value::INT) {
		return ::to_string(v.ival);
	} else if (v.type == arithmetic::Value::REAL) {
		return ::to_string(v.rval);
	}
	internal("", "unrecognized value in export_value()", __FILE__, __LINE__);
	return "";
}

ExpressionExporter::ExpressionExporter(ucs::ConstNetlist nets) : nets(nets) {
}

ExpressionExporter::~ExpressionExporter() {
}

parse_expression::operation ExpressionExporter::export_operator(int func) const {
	using OpType = arithmetic::Operation::OpType;
	using operation = parse_expression::operation;

	switch (func) {
	// VALIDITY - converted to CALL
	case OpType::WIRE_NOT: return operation("~", "", "", "");
	case OpType::WIRE_OR:  return operation("", "", "|", "");
	case OpType::WIRE_AND: return operation("", "", "&", "");
	case OpType::WIRE_XOR: return operation("", "", "^", "");
	// TRUTHINESS - converted to CALL
	case OpType::BOOLEAN_NOT: return operation("!", "", "", "");
	case OpType::BOOLEAN_OR: return operation("", "", "||", "");
	case OpType::BOOLEAN_AND: return operation("", "", "&&", "");
	// BOOLEAN_XOR
	case OpType::EQUAL: return operation("", "", "==", "");
	case OpType::NOT_EQUAL: return operation("", "", "!=", "");
	case OpType::LESS: return operation("", "", "<", "");
	case OpType::GREATER: return operation("", "", ">", "");
	case OpType::LESS_EQUAL: return operation("", "", "<=", "");
	case OpType::GREATER_EQUAL: return operation("", "", ">=", "");
	// NEGATIVE - converted to LESS
	case OpType::TERNARY: return operation("", "?", ":", "");
	case OpType::IDENTITY: return operation("+", "", "", "");
	case OpType::NEGATION: return operation("-", "", "", "");
	// INVERSE - converted to DIVIDE
	// TODO(edward.bingham) we need type information here to determine if we are using arithmetic or logical shift
	case OpType::SHIFT_LEFT: return operation("", "", "<<", "");
	case OpType::SHIFT_RIGHT: return operation("", "", ">>", "");
	case OpType::ADD: return operation("", "", "+", "");
	case OpType::SUBTRACT: return operation("", "", "-", "");
	case OpType::MULTIPLY: return operation("", "", "*", "");
	case OpType::DIVIDE: return operation("", "", "/", "");
	case OpType::MOD: return operation("", "", "%", "");
	case OpType::CALL: return operation("$", "(", ",", ")");
	// MEMBER_CALL
	case OpType::CAST: return operation("", "'(", "", ")");
	case OpType::ARRAY: return operation("'{", "", "", "}");
	case OpType::INDEX: return operation("", "[", ":", "]");
	case OpType::STRUCT: return operation("'{", "", "", "}");
	case OpType::MEMBER: return operation("", ".", "", "");
	}
	return operation();
}

const parse_expression::precedence_set &ExpressionExporter::precedence() const {
	return config::cfg->order;
}

parse_expression::expression::argument ExpressionExporter::export_constant(arithmetic::Value value) const {
	parse::wrapper<number> result;
	result.value = parse_verilog::export_value(value);
	return {0, std::shared_ptr<parse::syntax>(result.clone())};
}

parse_expression::expression::argument ExpressionExporter::export_literal(size_t index) const {
	parse::wrapper<parse::instance> result;
	result.value = nets.netAt(index);
	return {1, std::shared_ptr<parse::syntax>(result.clone())};
}

parse_expression::expression ExpressionExporter::export_special(int func, const vector<parse_expression::expression::argument> &args) const {
	using OpType = arithmetic::Operation::OpType;
	if (func == OpType::BOOLEAN_XOR) {
		return export_boolean_xor(args);
	}

	return arithmetic::ExpressionExporter::export_special(func, args);
}

parse_expression::expression ExpressionExporter::export_boolean_xor(const vector<parse_expression::expression::argument> &args) const {
	using OpType = arithmetic::Operation::OpType;
	const parse_expression::precedence_set &order = precedence();

	if (args.size() < 2u) {
		parse_expression::expression result;
		result.valid = true;

		result.level = -1;
		result.type = -1;

		result.arguments = args;
		return result;
	}

	auto orOp = export_operator(OpType::BOOLEAN_OR);
	auto andOp = export_operator(OpType::BOOLEAN_AND);
	auto notOp = export_operator(OpType::BOOLEAN_NOT);
	if (orOp.empty() or andOp.empty() or notOp.empty()) {
		internal("", "boolean operators not defined for verilog", __FILE__, __LINE__);
		return parse_expression::expression();
	}

	auto orIdx = order.find(-1, orOp);
	auto andIdx = order.find(-1, andOp);
	auto notIdx = order.find(-1, notOp);

	parse_expression::expression notA;
	notA.valid = true;
	notA.level = notIdx.level;
	notA.type = order.type(notA.level);
	notA.operators.push_back(notOp);
	notA.arguments.push_back(args[0]);

	parse_expression::expression notB;
	notB.valid = true;
	notB.level = notIdx.level;
	notB.type = order.type(notB.level);
	notB.operators.push_back(notOp);
	notB.arguments.push_back(args[1]);

	parse_expression::expression left;
	left.valid = true;
	left.level = andIdx.level;
	left.type = order.type(left.level);
	left.operators.push_back(andOp);
	left.arguments.push_back(args[0]);
	left.arguments.push_back({-1, std::shared_ptr<parse::syntax>(notB.clone())});

	parse_expression::expression right;
	right.valid = true;
	right.level = andIdx.level;
	right.type = order.type(right.level);
	right.operators.push_back(andOp);
	right.arguments.push_back({-1, std::shared_ptr<parse::syntax>(notA.clone())});
	right.arguments.push_back(args[1]);

	parse_expression::expression top;
	top.valid = true;
	top.level = orIdx.level;
	top.type = order.type(top.level);
	top.operators.push_back(orOp);
	top.arguments.push_back({-1, std::shared_ptr<parse::syntax>(left.clone())});
	top.arguments.push_back({-1, std::shared_ptr<parse::syntax>(right.clone())});
	
	if (args.size() > 2u) {
		vector<parse_expression::expression::argument> nextArgs;
		nextArgs.push_back({-1, std::shared_ptr<parse::syntax>(top.clone())});
		nextArgs.insert(nextArgs.end(), args.begin() + 2, args.end());
		return export_boolean_xor(nextArgs);
	}

	return top;
}

parse_expression::expression export_expression(const arithmetic::Expression &expr, ucs::ConstNetlist nets) {
	return ExpressionExporter(nets).export_expression(expr);
}



assignment_statement export_assign(ucs::ConstNetlist nets, clocked::Statement assign) {
	assignment_statement result;
	result.valid = true;
	result.left = export_expression(arithmetic::Expression::varOf(assign.net), nets);
	result.blocking = assign.blocking;
	result.expr = export_expression(assign.expr, nets);
	return result;
}

continuous export_continuous(ucs::ConstNetlist nets, clocked::Statement assign, bool force) {
	continuous result;
	result.valid = true;
	result.force = force;
	if (assign.expr.isNull()) {
		result.deassign = export_expression(arithmetic::Expression::varOf(assign.net), nets);
	} else {
		result.assign = export_assign(nets, assign);
	}
	return result;
}

declaration export_declaration(string type, std::string name, int msb, int lsb, bool input, bool output) {
	declaration result;
	result.valid = true;
	result.input = input;
	result.output = output;
	if (msb > lsb) {
		result.msb = export_expression(arithmetic::Expression::intOf(msb), ucs::ConstNetlist());
		result.lsb = export_expression(arithmetic::Expression::intOf(lsb), ucs::ConstNetlist());
	}
	result.type = type;
	result.name = name;
	return result;
}

block_statement export_block(ucs::ConstNetlist nets, const vector<clocked::Statement> &stmts) {
	block_statement body;
	body.valid = true;
	for (auto k = stmts.begin(); k != stmts.end(); k++) {
		if (k->type == clocked::Statement::ASSIGN) {
			/*if (mod.nets[k->net].purpose != clocked::Net::REG) {
				printf("error: found stateful assignments on wire '%s'\n", mod.nets[k->net].name.c_str());
			}*/
			body.sub.push_back(shared_ptr<parse::syntax>(new assignment_statement(export_assign(nets, *k))));
		} else if (k->type == clocked::Statement::IF or k->type == clocked::Statement::ELIF) {
			if_statement *cond = nullptr;
			if (k->type == clocked::Statement::IF) {
				cond = new if_statement();
				cond->valid = true;
				body.sub.push_back(shared_ptr<parse::syntax>(cond));
			} else {
				if (not body.sub.back()->is_a<if_statement>()) {
					printf("error:%s:%d: else if expected preceding if statement\n", __FILE__, __LINE__);
					continue;
				}

				cond = (if_statement*)body.sub.back().get();
			}

			if (not k->expr.isUndef() and not k->expr.isValid()) {
				cond->condition.push_back(export_expression(k->expr, nets));
			} else if (k->sub.empty()) {
				continue;
			} else {
				cond->condition.push_back(rvalue());
			}

			cond->body.push_back(export_block(nets, k->sub));
		} else {
			printf("error:%s:%d: unrecognized statement type %d\n", __FILE__, __LINE__, k->type);
			continue;
		}
	}

	return body;
}

trigger export_trigger(ucs::ConstNetlist nets, const clocked::Trigger &trig) {
	auto posedgeOp = parse_expression::operation("posedge", "", "", "");
	auto posedgeIdx = config::cfg->order.find(-1, posedgeOp);

	trigger always;
	always.valid = true;

	always.condition.valid = true;
	always.condition.level = posedgeIdx.level;
	always.condition.type = config::cfg->order.type(posedgeIdx.level);
	always.condition.arguments.push_back({-1, std::shared_ptr<parse::syntax>(export_expression(trig.clk, nets).clone())});
	always.condition.operators.push_back(posedgeOp);

	always.body = export_block(nets, trig.stmts);
	return always;
}

module_instance export_instance(ucs::ConstNetlist nets, const clocked::Instance &inst) {
	module_instance result;
	result.valid = true;
	result.module_type = inst.type;
	result.instance_name = inst.name;
	for (const Expression &port : inst.ports) {
		port_connection connect;
		connect.expr = export_expression(port, nets);
		result.connections.push_back(connect);
	}
	return result;
}

module_def export_module(const clocked::Module &mod) {
	module_def result;
	result.valid = true;
	result.name = mod.name;

	for (int i = 0; i < (int)mod.nets.size(); i++) {
		if (mod.nets[i].purpose == clocked::Net::IN) {
			result.ports.push_back(export_declaration("wire", mod.nets[i].name, mod.nets[i].type.width-1, 0, true, false));
		} else if (mod.nets[i].purpose == clocked::Net::OUT) {
			result.ports.push_back(export_declaration("wire", mod.nets[i].name, mod.nets[i].type.width-1, 0, false, true));
		} else if (mod.nets[i].purpose == clocked::Net::WIRE) {
			result.items.push_back(shared_ptr<parse::syntax>(new declaration(export_declaration("wire", mod.nets[i].name, mod.nets[i].type.width-1, 0, false, false))));
		} else if (mod.nets[i].purpose == clocked::Net::REG) {
			result.items.push_back(shared_ptr<parse::syntax>(new declaration(export_declaration("reg", mod.nets[i].name, mod.nets[i].type.width-1, 0, false, false))));
		}
	}

	for (auto i = mod.stmts.begin(); i != mod.stmts.end(); i++) {
		result.items.push_back(shared_ptr<parse::syntax>(new continuous(export_continuous(mod, *i))));
	}

	for (auto i = mod.triggers.begin(); i != mod.triggers.end(); i++) {
		result.items.push_back(shared_ptr<parse::syntax>(new trigger(export_trigger(mod, *i))));
	}
	
	for (auto i = mod.inst.begin(); i != mod.inst.end(); i++) {
		result.items.push_back(shared_ptr<parse::syntax>(new module_instance(export_instance(mod, *i))));
	}

	return result;
}

}
