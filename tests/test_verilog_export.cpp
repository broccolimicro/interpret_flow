#include <gtest/gtest.h>
#include <parse/default/line_comment.h>
#include <parse/default/block_comment.h>
#include <string>

#include <common/mock_netlist.h>

#include <parse_verilog/expression.h>
#include <interpret_flow/import_verilog.h>
#include <interpret_flow/export_verilog.h>

using namespace std;

TEST(VerilogExportParser, BasicBooleanOperations) {
	// Test exporting boolean operations to Verilog
	string test_code = "a & b | ~c";
	
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_verilog::rvalue::register_syntax(tokens);
	tokens.insert("basic_boolean", test_code);

	MockNetlist v;
	
	parse_verilog::rvalue in(tokens);
	arithmetic::Expression expr = parse_verilog::import_expression(in, v, &tokens, true);
	
	// Export to Verilog expression
	parse_expression::expression verilog_expr = parse_verilog::export_expression(expr, v);
	
	EXPECT_TRUE(tokens.is_clean());
	EXPECT_TRUE(verilog_expr.valid);
	
	// Verilog should use & and | for boolean operations
	string result = verilog_expr.to_string();
	EXPECT_TRUE(result.find("&") != string::npos);
	EXPECT_TRUE(result.find("|") != string::npos);
	EXPECT_TRUE(result.find("~") != string::npos || result.find("!") != string::npos);
}

TEST(VerilogExportParser, ArithmeticOperations) {
	// Test exporting arithmetic operations to Verilog
	string test_code = "a + b * c";
	
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_verilog::rvalue::register_syntax(tokens);
	tokens.insert("arithmetic_ops", test_code);

	MockNetlist v;
	
	parse_verilog::rvalue in(tokens);
	arithmetic::Expression expr = parse_verilog::import_expression(in, v, &tokens, true);
	
	// Export to Verilog expression
	parse_expression::expression verilog_expr = parse_verilog::export_expression(expr, v);
	
	EXPECT_TRUE(tokens.is_clean());
	EXPECT_TRUE(verilog_expr.valid);
	
	// Check for arithmetic operators
	string result = verilog_expr.to_string();
	EXPECT_TRUE(result.find("+") != string::npos);
	EXPECT_TRUE(result.find("*") != string::npos);
}

TEST(VerilogExportParser, ComparisonOperations) {
	// Test exporting comparison operations to Verilog
	string test_code = "a < b && c == d";
	
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_verilog::rvalue::register_syntax(tokens);
	tokens.insert("comparison_ops", test_code);

	MockNetlist v;
	
	parse_verilog::rvalue in(tokens);
	arithmetic::Expression expr = parse_verilog::import_expression(in, v, &tokens, true);
	
	// Export to Verilog expression
	parse_expression::expression verilog_expr = parse_verilog::export_expression(expr, v);
	
	EXPECT_TRUE(tokens.is_clean());
	EXPECT_TRUE(verilog_expr.valid);
	EXPECT_EQ(verilog_expr.to_string(), "a<b&&c==d");
}

TEST(VerilogExportParser, ExportValue) {
	// Test exporting constant values to Verilog
	
	// Boolean true value
	string value_true = parse_verilog::export_value(arithmetic::Value(true));
	EXPECT_TRUE(value_true == "1" || value_true == "1'b1");
	
	// Boolean false value
	string value_false = parse_verilog::export_value(arithmetic::Value(false));
	EXPECT_TRUE(value_false == "0" || value_false == "1'b0");
	
	// Numeric value
	string value_num = parse_verilog::export_value(arithmetic::Value(42));
	EXPECT_TRUE(value_num == "42" || value_num.find("'d42") != string::npos);
}

TEST(VerilogExportParser, ComplexExpression) {
	// Test exporting complex expressions to Verilog
	string test_code = "(a && b) || (c && !d) || (e < f)";
	
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_verilog::rvalue::register_syntax(tokens);
	tokens.insert("complex_expr", test_code);

	MockNetlist v;
	
	parse_verilog::rvalue in(tokens);
	arithmetic::Expression expr = parse_verilog::import_expression(in, v, &tokens, true);
	
	// Export to Verilog expression
	parse_expression::expression verilog_expr = parse_verilog::export_expression(expr, v);
	
	EXPECT_TRUE(tokens.is_clean());
	EXPECT_TRUE(verilog_expr.valid);
	EXPECT_EQ(verilog_expr.to_string(), "a&&b||c&&!d||e<f");
}
