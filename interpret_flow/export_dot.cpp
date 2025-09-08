#include "export_dot.h"

#include <algorithm>
#include <sstream>

//using namespace parse_dot;

namespace flow {

// Helper function to escape special characters in HTML
std::string escape_html(const std::string& input) {
	std::string output;
	for (char c : input) {
		switch (c) {
			case '&':  output += "&amp;"; break;
			case '<':  output += "&lt;"; break;
			case '>':  output += "&gt;"; break;
			case '"': output += "&quot;"; break;
			case '\'': output += "&#39;"; break;
			default:   output += c;
		}
	}
	return output;
}

// Helper function to escape special characters in DOT labels
std::string escape_label(const std::string& input) {
	std::string output;
	for (char c : input) {
		switch (c) {
			case '<': output += "\\<"; break;
			case '>': output += "\\>"; break;
			case '{': output += "\\{"; break;
			case '}': output += "\\}"; break;
			case '|': output += "\\|"; break;
			case '\"': output += "\\\""; break;
			case '\n': output += "\\n"; break;
			default: output += c;
		}
	}
	return output;
}

// Helper function to format expression as HTML table with proper escaping
std::string format_expression_table(const std::string& expr_str) {
	std::istringstream iss(expr_str);
	std::string line;
	std::stringstream html;

	html << "<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">" << endl;

	bool first_line = true;
	while (std::getline(iss, line)) {
		if (line.empty()) continue;
		if (first_line) {
			html << "<tr><td>" << escape_html(line) << "</td></tr>" << endl;
			first_line = false;
		} else {
			html << "<tr><td align=\"left\">" << escape_html(line) << "</td></tr>" << endl;
		}
	}

	html << "</table>>";

	return html.str();
	//// Escape only the special characters that would break the DOT format
	//std::string result = html.str();
	//std::string escaped_result;

	//for (char c : result) {
	//	switch (c) {
	//		case '"': escaped_result += "\\\""; break;
	//		case '\'': escaped_result += "\\'"; break;
	//		case '\\': escaped_result += "\\\\"; break;
	//		case '\n': escaped_result += "\\n"; break;
	//		case '&': 
	//							 // Only escape & if it's not part of an HTML entity
	//							 if (escaped_result.size() > 0 && escaped_result.back() == '&') {
	//								 escaped_result += "&amp;";
	//							 } else {
	//								 escaped_result += '&';
	//							 }
	//							 break;
	//		default: escaped_result += c;
	//	}
	//}

	//return escaped_result;
}

// Helper function to create an assignment statement
//static statement create_assignment_stmt(const std::string& key, const std::string& value) {
//	statement stmt;
//	stmt.statement_type = "";
//	stmt.valid = true;
//
//	assignment_list attrs;
//	attrs.valid = true;
//	attrs.as.push_back(assignment(key, value));
//
//	attribute_list attr_list;
//	attr_list.valid = true;
//	attr_list.attributes.push_back(attrs);
//
//	stmt.attributes = attr_list;
//	return stmt;
//}

void append_statement(parse_dot::graph &g, const std::string &s, size_t indent=1) {
	parse_dot::statement stmt;
	stmt.statement_type = "";
	stmt.valid = true;

	std::string indented_s;
	for (size_t i = 0; i < indent; i++) {
		indented_s += "\t";
	}
	indented_s += s;

	stmt.nodes.push_back(new parse_dot::node_id(indented_s));
	g.statements.push_back(stmt);
}

parse_dot::graph export_func(const Func &f) {
	parse_dot::graph g;
	g.id = "flow_" + f.name;
	g.valid = true;
	g.type = "digraph";

	// Add graph attributes
	std::string graph_attr = "rankdir=LR labeljust=l labelloc=t fontsize=24";

	//// Add net information as graph label
	std::ostringstream net_oss;
	size_t net_idx = 0;
	for (const auto& net : f.nets) {
		net_oss << "v" << net_idx << ": " << net << endl;
		net_idx++;
	}
	std::string net_info = net_oss.str();

	//// Add the graph label with proper escaping
	std::string graph_label = f.name + "\\n\\n" + net_info;
	graph_attr += " label=\"" + escape_label(graph_label) + "\"";

	//// Add the graph attributes as a raw statement
	append_statement(g, graph_attr);
	append_statement(g, "graph[fontname=\"Courier\" ranksep=2]");
	append_statement(g, "node[fontname=\"Courier\", shape=record, fillcolor=\"beige\", fontsize=24]");

	// Add input channel nodes (rank=min)
	std::vector<parse_dot::node_id*> input_nodes;
	std::vector<parse_dot::node_id*> output_nodes;

	// First pass: Create all channel nodes
	for (const auto& net : f.nets) {
		if (net.purpose == Net::IN || net.purpose == Net::OUT) {
			parse_dot::statement node_stmt;
			node_stmt.statement_type = "node";

			// Create node with name and bit width
			std::string label = net.name + " :" + std::to_string(net.type.width);
			parse_dot::node_id* node = new parse_dot::node_id();
			node->id = net.name;
			node->valid = true;
			node_stmt.nodes.push_back(node);

			// Add node attributes
			parse_dot::assignment_list node_attrs;
			node_attrs.valid = true;
			node_attrs.as.push_back(parse_dot::assignment("shape", "cylinder"));
			node_attrs.as.push_back(parse_dot::assignment("label", label));
			node_attrs.as.push_back(parse_dot::assignment("fontname", "Courier"));
			node_attrs.as.push_back(parse_dot::assignment("fontsize", "24"));

			parse_dot::attribute_list attr_list;
			attr_list.valid = true;
			attr_list.attributes.push_back(node_attrs);
			node_stmt.attributes = attr_list;

			g.statements.push_back(node_stmt);

			// Track for rank assignment
			if (net.purpose == Net::IN) {
				input_nodes.push_back(node);
			} else {
				output_nodes.push_back(node);
			}
		}
	}

	// Set ranks for input and output nodes using rank statements
	if (!input_nodes.empty()) {
		std::string min_rank_stmt = "{rank=min; ";
		for (auto* node : input_nodes) {
			min_rank_stmt += node->id + "; ";
		}
		min_rank_stmt += "}";

		append_statement(g, min_rank_stmt);
	}

	if (!output_nodes.empty()) {
		std::string max_rank_stmt = "{rank=max; ";
		for (auto* node : output_nodes) {
			max_rank_stmt += node->id + "; ";
		}
		max_rank_stmt += "};";

		append_statement(g, max_rank_stmt);
	}

	// Process each branch (condition)
	for (size_t i = 0; i < f.conds.size(); ++i) {
		const auto& cond = f.conds[i];
		std::string branch_name = "branch_" + std::to_string(i);
		append_statement(g, "subgraph cluster_" + branch_name + " {");

		// Add subgraph attributes
		std::string subgraph_attrs = "\t\tgraph[label=\"" + branch_name
			+ "\" labeljust=l style=dashed fontsize=24];";
		append_statement(g, subgraph_attrs, 2);

		// Add branch head (predicate) as a raw node definition
		std::string branch_head_name = branch_name + "_head";

		std::string predicate_html = format_expression_table(cond.valid.to_string());
		std::string branch_head_attrs = std::string("[shape=doubleoctagon style=filled")
			+ " fillcolor=crimson fontcolor=white fontsize=20 label=" + predicate_html + "];";

		append_statement(g, branch_head_name + branch_head_attrs, 2);


		// Add input channels
		std::string ins_rank = "{rank=same;";
		for (int in : cond.ins) {
			std::string in_net_name = f.netAt(in);

			// Local-only duplicate input channel to show which branch uses which channels
			std::string input_channel_copy = branch_name + "_" + in_net_name;
			ins_rank += input_channel_copy + "; ";
			std::string input_channel_decl = input_channel_copy
				+ "[shape=cylinder label=\"" + in_net_name + "\"];";
			append_statement(g, input_channel_decl, 2);

			// Create input channel connection with color
			std::string input_route = in_net_name + "->" + input_channel_copy;
			//TODO: more sophisticated auto-coloring palettes (for now, red if "control" ...perhaps dataless?)
			//std::string input_route_color = (in_net_name.find("c") != std::string::npos) ? "red" : "blue";
			size_t channel_width = f.nets[in].type.width;
			std::string input_route_color = (channel_width < 2) ? "red" : "blue";
			input_route += "[color=" + input_route_color + "];";
			append_statement(g, input_route, 2);

			// Connect input channel to predicate
			std::string internal_input_route = input_channel_copy + "->" + branch_head_name
				+ "[arrowhead=tee];";
			append_statement(g, internal_input_route, 2);
		}
		append_statement(g, ins_rank + "}", 2);


		// Add internal registers (w/ "mem[ory]" assignment expressions)
		std::string regs_rank = "{rank=same;";
		//TODO: rank=same all mem & req exprs
		std::string exprs_rank = "{rank=same;";
		for (const auto& reg : cond.regs) {
			// Add register expression as a raw node definition
			std::string reg_expr_name = branch_name + "_" + std::to_string(reg.first) + "_mem_expr";
			exprs_rank += reg_expr_name + "; ";
			std::string reg_expr_attrs = std::string("[shape=note style=filled fillcolor=beige")
				+ " fontname=Courier fontsize=16 label="
				+ format_expression_table(reg.second.to_string()) + "];";
			append_statement(g, reg_expr_name + reg_expr_attrs, 2);

			// Add register folder node as a raw node definition
			std::string reg_name = f.netAt(reg.first);
			std::string reg_node_name = branch_name + "_" + reg_name;
			regs_rank += reg_node_name + "; ";
			size_t reg_width = f.nets[reg.first].type.width;
			std::string reg_route_color = (reg_width < 2) ? "red" : "blue";
			std::string reg_node_attrs = "[shape=folder label=\"" + reg_name
				+ " :" + std::to_string(reg_width) + "\"];";

			append_statement(g, reg_node_name + reg_node_attrs, 2);

			// Create edge from branch head to expression
			append_statement(g, branch_head_name + "->" + reg_expr_name + ";", 2);

			// Create edge from expression to register
			std::string reg_route = reg_expr_name + "->" + reg_node_name
				+ "[color=" + reg_route_color + "];";
			append_statement(g, reg_route, 2);

			//TODO: soft invis ordering relative to outputs?
		}
		append_statement(g, regs_rank + "}", 2);


		// Add output channels (w/ "req[uest]" assignment expressions
		std::string outs_rank = "{rank=same;";
		for (const auto& out : cond.outs) {
			// Format the output expression as a table
			std::string out_expr_label = format_expression_table(out.second.to_string());

			// Add request expression as a raw node definition
			std::string out_expr_name = branch_name + "_" + std::to_string(out.first) + "_out_expr";
			std::string out_expr_attrs = "[shape=note style=filled label="
				+ out_expr_label + "fillcolor=palegreen fontsize=16];";
			append_statement(g, out_expr_name + out_expr_attrs, 2);
			append_statement(g, branch_head_name + "->" + out_expr_name, 2);
			exprs_rank += out_expr_name + "; ";

			// Local-only duplicate output channel to show which branch uses which channels
			std::string out_net_name = f.netAt(out.first);
			std::string output_channel_copy = branch_name + "_" + out_net_name;
			outs_rank += output_channel_copy + "; ";
			std::string output_channel_decl = output_channel_copy
				+ "[shape=cylinder label=\"" + out_net_name + "\"];";
			append_statement(g, output_channel_decl, 2);
			append_statement(g, out_expr_name + "->" + output_channel_copy, 2);

			// Connect local-only output channel duplicate to actual external channel
			std::string internal_output_route = output_channel_copy + "->" + out_net_name;

			size_t channel_width = f.nets[out.first].type.width;
			std::string output_route_color = (channel_width < 2) ? "red" : "blue";
			internal_output_route += "[color=" + output_route_color + "];";
			append_statement(g, internal_output_route, 2);
		}
		append_statement(g, exprs_rank + "}", 2);
		append_statement(g, outs_rank + "}", 2);

		// Add invisible node for alignment as a raw node definition
		append_statement(g, branch_name + "_align[style=invis];", 2);

		// Enclose branch's subgraph
		append_statement(g, "}");
	}


	// Add rank=same for all branch align nodes as a raw statement
	if (!f.conds.empty()) {
		std::string rank_stmt = "{rank=same; ";
		for (size_t i = 0; i < f.conds.size(); ++i) {
			if (i > 0) rank_stmt += " ";
			rank_stmt += "branch_" + std::to_string(i) + "_align;";
		}
		rank_stmt += "};";
		append_statement(g, rank_stmt);
	}  
	return g;
}

}
