#include "import_verilog.h"

#include <common/standard.h>
#include <parse_expression/import.h>
#include <interpret_arithmetic/import_default.h>

#include <arithmetic/expression.h>
#include <arithmetic/algorithm.h>

namespace parse_verilog {

ExpressionImporter::ExpressionImporter(ucs::Netlist symbols, bool autoDefine) : symbols(symbols) {
	this->autoDefine = autoDefine;
}

ExpressionImporter::~ExpressionImporter() {
}

arithmetic::Expression ExpressionImporter::import_term(const parse_expression::expression::argument &syntax, tokenizer *tokens) const {
	if (syntax.type < 0 or syntax.type >= (int)config::cfg->literals.size() or not syntax.ptr) {
		return arithmetic::Expression::undef();
	}

	std::string type = config::cfg->literals[syntax.type].first;

	if (type == "constant") {
		std::string value = syntax.ptr->get<constant>().value;
		return arithmetic::import_constant(value, tokens);
	} else if (type == "literal") {
		std::string name = syntax.ptr->get<literal>().value;
		return arithmetic::import_literal(name, symbols, tokens, autoDefine);
	} else if (type == "type") {
		std::string name = syntax.ptr->get<type_name>().value;
		return arithmetic::Expression::typeOf(name);
	}
	internal("", "unsupported literal type '" + type + "'", __FILE__, __LINE__);
	return arithmetic::Expression::undef();
}

arithmetic::Expression ExpressionImporter::import_unary(parse_expression::operation op, arithmetic::Expression expr, tokenizer *tokens) const {
	if (op.is("posedge", "", "", "")) {
		return arithmetic::call("posedge", {expr});
	} else if (op.is("negedge", "", "", "")) {
		return arithmetic::call("negedge", {expr});
	} else if (op.is("+", "", "", "")) {
		return expr;
	} else if (op.is("-", "", "", "")) {
		return -expr;
	} else if (op.is("!", "", "", "")) {
		return !expr;
	} else if (op.is("~", "", "", "")) {
		return ~expr;

	/* TODO(edward.bingham) These require me to unpack encodings and they assume a particular encoding.
	} else if (op.is("&", "", "", "")) {
		return ~expr;
	} else if (op.is("~&", "", "", "")) {
		return ~expr;
	} else if (op.is("|", "", "", "")) {
		return ~expr;
	} else if (op.is("~|", "", "", "")) {
		return ~expr;
	} else if (op.is("^", "", "", "")) {
		return ~expr;
	} else if (op.is("~^", "", "", "")) {
		return ~expr;
	} else if (op.is("^~", "", "", "")) {
		return ~expr;
	*/
	}
	return expr;
}

arithmetic::Expression ExpressionImporter::import_binary(parse_expression::operation op, arithmetic::Expression left, arithmetic::Expression right, tokenizer *tokens) const {
	if (op.is("", "", "or", "")) {
		return left | right; // event-or in verilog is just a wire-or in QDI
	} else if (op.is("", "", "||", "")) {
		return left || right;
	} else if (op.is("", "", "&&", "")) {
		return left && right;
	} else if (op.is("", "", "|", "")) {
		return left | right;
	} else if (op.is("", "", "^", "")) {
		return left ^ right;
	} else if (op.is("", "", "~^", "")) {
		return (~left) ^ right;
	} else if (op.is("", "", "^~", "")) {
		return left ^ (~right);
	} else if (op.is("", "", "&", "")) {
		return left & right;	
	} else if (op.is("", "", "==", "")) {
		return left == right;
	} else if (op.is("", "", "!=", "")) {
		return left != right;
	} else if (op.is("", "", "<", "")) {
		return left < right;
	} else if (op.is("", "", ">", "")) {
		return left > right;
	} else if (op.is("", "", "<=", "")) {
		return left <= right;
	} else if (op.is("", "", ">=", "")) {
		return left >= right;

	// TODO(edward.bingham) I think I might need a typecast somewhere in here
	} else if (op.is("", "", "<<", "")) {
		return left << right;
	} else if (op.is("", "", ">>", "")) {
		return left >> right;
	} else if (op.is("", "", "<<<", "")) {
		return left << right;
	} else if (op.is("", "", ">>>", "")) {
		return left >> right;

	} else if (op.is("", "", "+", "")) {
		return left + right;
	} else if (op.is("", "", "-", "")) {
		return left - right;
	} else if (op.is("", "", "*", "")) {
		return left * right;
	} else if (op.is("", "", "/", "")) {
		return left / right;
	} else if (op.is("", "", "%", "")) {
		return left % right;
	} else if (op.is("", "", "**", "")) {
		return arithmetic::call("pow", {left, right});
	}
	internal("", "unrecognized operation", __FILE__, __LINE__);
	return left;
}

/*
TODO(edward.bingham) we don't have arithmetic support for conditionals inside expressions

arithmetic::Expression ExpressionImporter::import_ternary(parse_expression::operation op, std::vector<arithmetic::Expression> args, tokenizer *tokens) const {
	if (op.is("", "?", ":", "")) {
		return left | right;
	}
	internal("", "unrecognized operation", __FILE__, __LINE__);
	return left;
}*/


arithmetic::Expression ExpressionImporter::import_group(parse_expression::operation op, vector<arithmetic::Expression> args, tokenizer *tokens) const {
	if (op.is("'{", "", ",", "}")) {
		return arithmetic::array(args);
	}
	internal("", "unrecognized operation", __FILE__, __LINE__);
	return arithmetic::Expression();
}

arithmetic::Expression ExpressionImporter::import_modifier(parse_expression::operation op, vector<arithmetic::Expression> args, tokenizer *tokens) const {
	// TODO(edward.bingham) See the parser, operator :: is not yet implemented

	if (op.is("", ".", "", "")) { // Member
		return arithmetic::Expression(arithmetic::Operation::MEMBER, args);
	// DESIGN(edward.bingham) Move "this" into the first argument of the
	// function. So "a.b.c(d, e) becomes c(a.b, d, e). This seems like a
	// reasonable way to simplify things, and follows the early style of c++
	// function names.
	} else if (op.is("$", "(", ",", ")")) { // Call, Validity, Truthiness
		if (args.empty()) {
			error("", "function call expects function name", __FILE__, __LINE__);
			return arithmetic::Expression();
		}

		// Replace member calls
		if (args[0].top.isExpr() and args[0].getExpr(args[0].top.index)->func == arithmetic::Operation::MEMBER) {
			arithmetic::Operation op = *args[0].getExpr(args[0].top.index);

			arithmetic::Operand name = op.operands.back();
			op.operands.pop_back();

			if (op.operands.size() == 1u) {
				op.func = arithmetic::Operation::IDENTITY;
			}
			args[0].setExpr(op);
			args.insert(args.begin(), name);

			return arithmetic::Expression(arithmetic::Operation::MEMBER_CALL, args);
		// Replace built-in functions
		} else if (args[0].top.isConst() and args[0].top.cnst.type == arithmetic::Value::STRING and args[0].top.cnst.sval == "valid") {
			if      (args.size() == 1u) { return arithmetic::Expression::vdd(); }
			else if (args.size() == 2u) { return arithmetic::isValid(args[1]); }
			else { error("", "valid() function expects 1 argument, found " + ::to_string(args.size()-1), __FILE__, __LINE__); }
		} else if (args[0].top.isConst() and args[0].top.cnst.type == arithmetic::Value::STRING and args[0].top.cnst.sval == "true") {
			if      (args.size() == 1u) { return arithmetic::Expression::boolOf(true); }
			else if (args.size() == 2u) { return arithmetic::isTrue(args[1]); }
			else { error("", "true() function expects 1 argument, found " + ::to_string(args.size()-1), __FILE__, __LINE__); }
		} else {
			return arithmetic::Expression(arithmetic::Operation::CALL, args);
		}
	// END DESIGN
	} else if (op.is("", "[", ":", "]")) {
		return arithmetic::Expression(arithmetic::Operation::INDEX, args);
	} else if (op.is("", "'(", "", ")")) {
		if (args.size() != 2u or not args[0].isConstant()) {
			error("", "typecast expects type and value", __FILE__, __LINE__);
			return arithmetic::Expression();
		}
		arithmetic::Value type = arithmetic::evaluateConstExpr(args[0], args[0].top);
		if (type.type != arithmetic::Value::TYPE) {
			error("", "typecast expects type and value", __FILE__, __LINE__);
		}
		return arithmetic::cast(type.sval, args[1]);
	}
	internal("", "unrecognized operation", __FILE__, __LINE__);
	return arithmetic::Expression();
}

arithmetic::Expression import_expression(const parse_expression::expression &syntax, ucs::Netlist nets, tokenizer *tokens, bool auto_define) {
	return ExpressionImporter(nets, auto_define).import_expression(syntax, tokens);
}

}
