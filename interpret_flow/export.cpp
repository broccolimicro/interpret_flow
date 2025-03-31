#include "export.h"

#include <interpret_arithmetic/export_verilog.h>

parse_verilog::continuous export_continuous(const ucs::variable_set &variables, ucs::variable name, arithmetic::expression expr, bool blocking, bool force) {
	parse_verilog::continuous result;
	result.valid = true;
	result.force = force;
	if (expr.is_null()) {
		result.deassign = parse_verilog::export_variable_name(name, variables);
	} else {
		result.assign = export_assign(variables, name, expr, blocking);
	}
	return result;
}

parse_verilog::assignment_statement export_assign(const ucs::variable_set &variables, ucs::variable name, arithmetic::expression expr, bool blocking) {
	parse_verilog::assignment_statement result;
	result.valid = true;
	result.name = parse_verilog::export_variable_name(name, variables);
	result.blocking = blocking;
	result.expr = parse_verilog::export_expression(expr, variables);
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

void export_port(parse_verilog::module_def &dst, const flow::Port &port, bool output, const ucs::variable_set &vars) {
	string name = vars.nodes[port.index].to_string();
	dst.ports.push_back(export_declaration("wire", name+"_valid", 0, 0, not output, output));
	dst.ports.push_back(export_declaration("wire", name+"_ready", 0, 0, output, not output));
	dst.ports.push_back(export_declaration("wire", name+"_data", port.type.width-1, 0, not output, output));
}

parse_verilog::module_def export_module(const flow::Func &func) {
	parse_verilog::module_def result;
	result.valid = true;
	result.name = func.name;

	// Generate the port list
	result.ports.push_back(export_declaration("wire", "reset", 0, 0, true, false));
	result.ports.push_back(export_declaration("wire", "clk", 0, 0, true, false));

	for (int i = 0; i < (int)func.from.size(); i++) {
		export_port(result, func.from[i], false, func.vars);
	}

	for (int i = 0; i < (int)func.to.size(); i++) {
		export_port(result, func.to[i], true, func.vars);
	}

	// Create the state registers for each output channel
	for (int i = 0; i < (int)func.to.size(); i++) {
		string name = func.vars.nodes[func.to[i].index].to_string();
		result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::declaration(export_declaration("wire", name+"_state", func.to[i].type.width-1, 0))));
	}
	
	for (int i = 0; i < (int)func.cond.size(); i++) {
		string port = func.vars.nodes[func.cond[i].index].to_string();
		result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::declaration(export_declaration("reg", port))));
		result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::declaration(export_declaration("wire", port+"_ready"))));
	}

	for (int i = 0; i < (int)func.regs.size(); i++) {
		string port = func.vars.nodes[func.regs[i].index].to_string();
		result.items.push_back(shared_ptr<parse::syntax>(new parse_verilog::declaration(export_declaration("reg", port, func.regs[i].type.width-1, 0))));
	}

	for (int i = 0; i < (int)func.cond.size(); i++) {
		ucs::variable name = func.vars.nodes[func.cond[i].index];
		name.name.back().name += "_ready";
		
		result.items.push_back(shared_ptr<parse::syntax>(
			new parse_verilog::continuous(
				export_continuous(func.vars, name, ~operand(func.cond[i].index, operand::variable) | func.cond[i].en)
			)));
	}

	for (int i = 0; i < (int)func.to.size(); i++) {
		ucs::variable data = func.vars.nodes[func.to[i].index];
		data.name.back().name += "_data";
		ucs::variable valid = func.vars.nodes[func.to[i].index];
		valid.name.back().name += "_valid";

		/*result.items.push_back(shared_ptr<parse::syntax>(
			new parse_verilog::continuous(
				export_continuous(func.vars, data, )
			)));*/
		result.items.push_back(shared_ptr<parse::syntax>(
			new parse_verilog::continuous(
				export_continuous(func.vars, valid, func.to[i].token)
			)));


		//fprintf(fptr, "\tassign %s_data = %s_state;\n", port.c_str(), port.c_str());
	}

	/*for (int i = 0; i < (int)from.size(); i++) {
		string port = vars.nodes[from[i].index].to_string();
		expression ready = from[i].token;
		for (int i = 0; i < (int)cond.size(); i++) {
			// TODO(edward.bingham) need cond[i].ready
			ready.replace(cond[i].index, cond[i].ready);
		}

		fprintf(fptr, "\tassign %s_ready = %s;\n", port.c_str(), export_expression(~from[i].token | ready, vars).to_string().c_str());
	}

	arithmetic::expression ready = true;
	for (int i = 0; i < (int)to.size(); i++) {
		string port = vars.nodes[to[i].index].to_string();

		printf("\talways @(posedge clk) begin\n");
		printf("\t\tif (reset) begin\n");
		printf("\t\t\t%s_send <= 0;\n", port.c_str());
		printf("\t\t\t%s_state <= 0;\n", port.c_str());
		printf("\t\tend else if (!%s_valid || %s_ready) begin\n", port.c_str(), port.c_str());
		printf("\t\t\tif (%s) begin\n", export_expression(is_valid(to[i].token), vars).to_string().c_str());
		printf("\t\t\t\t%s_state <= %s;\n", port.c_str(), export_expression(to[i].value, vars).to_string().c_str());
		printf("\t\t\t\t%s_send <= 1;\n", port.c_str());
		printf("\t\t\tend else begin\n");
		printf("\t\t\t\t%s_send <= 0;\n", port.c_str());
		printf("\t\t\tend\n");
		printf("\t\tend\n");
		printf("\tend\n\n");
	}

	fprintf(fptr, "endmodule\n");*/

	return result;
}

