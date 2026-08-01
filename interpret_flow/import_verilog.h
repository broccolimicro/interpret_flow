#pragma once

#include <common/standard.h>

#include <parse_verilog/expression.h>

#include <parse_expression/import.h>

#include <arithmetic/expression.h>

namespace parse_verilog {

struct ExpressionImporter : parse_expression::Importer<arithmetic::Expression> {
	ucs::Netlist symbols;
	bool autoDefine;

	ExpressionImporter(ucs::Netlist symbols, bool autoDefine = false);
	~ExpressionImporter();

	arithmetic::Expression import_term(const parse_expression::expression::argument &syntax, tokenizer *tokens) const override;
	arithmetic::Expression import_unary(parse_expression::operation op, arithmetic::Expression expr, tokenizer *tokens) const override;
	arithmetic::Expression import_binary(parse_expression::operation op, arithmetic::Expression left, arithmetic::Expression right, tokenizer *tokens) const override;
	arithmetic::Expression import_group(parse_expression::operation op, vector<arithmetic::Expression> args, tokenizer *tokens) const override;
	arithmetic::Expression import_modifier(parse_expression::operation op, vector<arithmetic::Expression> args, tokenizer *tokens) const override;
};

arithmetic::Expression import_expression(const parse_expression::expression &syntax, ucs::Netlist nets, tokenizer *tokens, bool auto_define = false);

}
