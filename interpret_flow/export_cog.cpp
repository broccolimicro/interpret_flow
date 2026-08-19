#include "export_cog.h"

#include <arithmetic/algorithm.h>
#include <common/message.h>

#include <parse/wrapper.h>

namespace parse_cog {

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
	case OpType::BOOLEAN_XOR: return operation("", "", "^^", "");
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
	// INVERSE - converted to INTDIV
	// TODO(edward.bingham) we need type information here to determine if we are using arithmetic or logical shift
	case OpType::SHIFT_LEFT: return operation("", "", "<<", "");
	case OpType::SHIFT_RIGHT: return operation("", "", ">>", "");
	case OpType::ADD: return operation("", "", "+", "");
	case OpType::SUBTRACT: return operation("", "", "-", "");
	case OpType::MULTIPLY: return operation("", "", "*", "");
	case OpType::INTDIV: return operation("", "", "/", "");
	case OpType::INTMOD: return operation("", "", "%", "");
	case OpType::CALL: return operation("", "(", ",", ")");
	// MEMBER_CALL - converted to MEMBER and CALL
	case OpType::CAST: return operation("(", ")", "", "");
	case OpType::ARRAY: return operation("[", "", ",", "]");
	case OpType::INDEX: return operation("", "[", ":", "]");
	//case OpType::STRUCT: return operation("'{", "", "", "}");
	case OpType::MEMBER: return operation("", ".", "", "");
	}
	return operation();
}

const parse_expression::precedence_set &ExpressionExporter::precedence() const {
	return expression_config::cfg->order;
}

parse_expression::expression::argument ExpressionExporter::export_constant(arithmetic::Value value) const {
	if (value.type == arithmetic::Value::LABEL) {
		label result;
		result.value = arithmetic::export_value(value);
		return {2, std::shared_ptr<parse::syntax>(result.clone())};
	}
	constant result;
	result.value = arithmetic::export_value(value);
	return {0, std::shared_ptr<parse::syntax>(result.clone())};
}

parse_expression::expression::argument ExpressionExporter::export_literal(size_t index) const {
	literal result;
	result.name = nets.netAt(index);
	return {1, std::shared_ptr<parse::syntax>(result.clone())};
}

parse_expression::expression export_expression(const arithmetic::Expression &expr, ucs::ConstNetlist nets) {
	return ExpressionExporter(nets).export_expression(expr);
}

parse_expression::assignment export_assignment(const arithmetic::Action &expr, ucs::ConstNetlist nets) {
	parse_expression::assignment result;
	result.valid = true;

	if (not expr.lvalue.isUndef()) {
		result.left.push_back(export_expression(expr.lvalue, nets));
	}

	// TODO(edward.bingham) we need type information about the lvalue here
	arithmetic::Operand top = expr.rvalue.top;
	if (top.isConst() and top.cnst.isNeutral()) {
		result.operation = "-";
	} else if (top.isConst() and top.cnst.isUnstable()) {
		result.operation = "~";
	} else if (top.isConst() and top.cnst.type == arithmetic::Value::WIRE and top.cnst.isValid()) {
		result.operation = "+";
	} else {
		result.right = export_expression(expr.rvalue, nets);
		result.operation = "=";
	}

	return result;
}

CompositionExporter::CompositionExporter(ucs::ConstNetlist nets) : nets(nets) {
}

CompositionExporter::~CompositionExporter() {
}

parse_expression::operation CompositionExporter::export_operator(int func) const {
	using OpType = arithmetic::Operation::OpType;
	using operation = parse_expression::operation;

	switch (func) {
	// VALIDITY - converted to CALL
	case OpType::WIRE_OR:  return operation("", "", ":", "");
	case OpType::WIRE_AND: return operation("", "", ",", "");
	}
	return operation();
}

const parse_expression::precedence_set &CompositionExporter::precedence() const {
	return composition_config::cfg->order;
}

parse_expression::expression::argument CompositionExporter::export_action(const arithmetic::Action &expr) const {
	return {1, std::shared_ptr<parse::syntax>(export_assignment(expr, nets).clone())};
}

parse_expression::expression export_composition(const arithmetic::Parallel &expr, ucs::ConstNetlist nets) {
	return CompositionExporter(nets).export_expression(expr);
}

parse_expression::expression export_composition(const arithmetic::Choice &expr, ucs::ConstNetlist nets) {
	return CompositionExporter(nets).export_expression(expr);
}

parse_cog::assignment export_in(int uid, ucs::ConstNetlist nets) {
	return export_assignment(
		arithmetic::Action(
			arithmetic::memberCall(
				arithmetic::Expression::varOf(uid), "recv", {})),
		nets);
}

parse_cog::composition export_ins(const std::vector<int> &ins, ucs::ConstNetlist nets) {
	parse_cog::composition result;
	result.valid = true;
	result.level = parse_cog::composition::PARALLEL;
	for (int in : ins) {
		if (not result.branches.empty()) {
			result.comp.push_back("and");
		}
		result.branches.push_back(std::shared_ptr<parse::syntax>(export_in(in, nets).clone()));
	}
	return result;
}

parse_cog::assignment export_reg(int uid, arithmetic::Expression expr, ucs::ConstNetlist nets) {
	return export_assignment(
		arithmetic::Action(
			arithmetic::Expression::varOf(uid), expr),
		nets);
}

parse_cog::composition export_regs(const std::vector<std::pair<int, arithmetic::Expression> > &regs, ucs::ConstNetlist nets) {
	parse_cog::composition result;
	result.valid = true;
	result.level = parse_cog::composition::PARALLEL;
	for (const auto &reg : regs) {
		if (not result.branches.empty()) {
			result.comp.push_back("and");
		}
		result.branches.push_back(std::shared_ptr<parse::syntax>(export_reg(reg.first, reg.second, nets).clone()));
	}
	return result;
}

parse_cog::assignment export_out(int uid, arithmetic::Expression expr, ucs::ConstNetlist nets) {
	arithmetic::Action action(
		arithmetic::memberCall(
			arithmetic::Expression::varOf(uid),
			"send", {expr}));
	return export_assignment(action, nets);
}

parse_cog::composition export_outs(const std::vector<std::pair<int, arithmetic::Expression> > &outs, ucs::ConstNetlist nets) {
	parse_cog::composition result;
	result.valid = true;
	result.level = parse_cog::composition::PARALLEL;
	for (const auto &out : outs) {
		if (not result.branches.empty()) {
			result.comp.push_back("and");
		}
		result.branches.push_back(std::shared_ptr<parse::syntax>(export_out(out.first, out.second, nets).clone()));
	}
	return result;
}

parse_cog::composition export_branch(const flow::Condition &cond, ucs::ConstNetlist nets) {
	parse_cog::composition result;
	result.valid = true;
	result.level = parse_cog::composition::SEQUENCE;

	if (not cond.outs.empty()) {
		result.branches.push_back(std::shared_ptr<parse::syntax>(export_outs(cond.outs, nets).clone()));
	}
	if (not cond.regs.empty()) {
		if (not result.branches.empty()) {
			result.comp.push_back("\n");
		}
		result.branches.push_back(std::shared_ptr<parse::syntax>(export_regs(cond.regs, nets).clone()));
	}
	if (not cond.ins.empty()) {
		if (not result.branches.empty()) {
			result.comp.push_back("\n");
		}
		result.branches.push_back(std::shared_ptr<parse::syntax>(export_ins(cond.ins, nets).clone()));
	}
	return result;

}

parse_cog::control export_condition(const flow::Condition &cond, ucs::ConstNetlist nets) {
	parse_cog::control result;
	result.valid = true;
	result.kind = "await";
	result.guard = export_expression(cond.valid, nets);
	result.action = export_branch(cond, nets);
	return result;
}

parse_cog::composition export_conditions(const std::vector<flow::Condition> &conds, ucs::ConstNetlist nets) {
	if (conds.size() == 1u and conds[0].valid.isValid()) {
		return export_branch(conds[0], nets);
	}

	parse_cog::composition result;
	result.valid = true;
	result.level = parse_cog::composition::CHOICE;
	for (const auto &cond : conds) {
		if (not result.branches.empty()) {
			result.comp.push_back("or");
		}
		result.branches.push_back(std::shared_ptr<parse::syntax>(export_condition(cond, nets).clone()));
	}
	return result;
}

parse_cog::control export_func(const flow::Func &func) {
	parse_cog::control result;
	result.valid = true;
	result.kind = "while";
	result.guard = export_expression(arithmetic::Expression::boolOf(true), func);
	result.action = export_conditions(func.conds, func);
	return result;
}

}
