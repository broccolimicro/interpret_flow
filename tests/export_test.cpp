#include <gtest/gtest.h>
#include <flow/graph.h>
#include <interpret_flow/export.h>

using namespace flow;
using namespace arithmetic;

TEST(ExportTest, Buffer) {
	Func buffer;
	buffer.name = "buffer";
	int L = buffer.pushFrom("L", Type(Type::FIXED, 16));
	int R = buffer.pushTo("R", Type(Type::FIXED, 16));

	int case0 = buffer.pushCond(
		is_valid(operand(L, operand::variable)),
		is_valid(operand(R, operand::variable)));

	// If we send on R, then we receive on L
	buffer.portAt(L).token = operand(case0, operand::variable);
	buffer.portAt(L).value = true;

	// We should forward the value on L to R
	buffer.portAt(R).token = operand(case0, operand::variable);
	buffer.portAt(R).value = operand(L, operand::variable);

	cout << export_module(buffer).to_string() << endl;
}


