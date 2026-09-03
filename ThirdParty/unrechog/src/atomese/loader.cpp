/**
 * @file loader.cpp
 * @brief Atomese S-expression parser and AtomSpace loader implementation
 */

#include <opencog/atomese/loader.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace opencog::atomese {

// ============================================================================
// Tokenizer
// ============================================================================

enum class TokenType { LPAREN, RPAREN, STRING, SYMBOL, END_OF_INPUT };

struct Token {
    TokenType type;
    std::string value;
    size_t line;
};

class Tokenizer {
public:
    explicit Tokenizer(std::string_view source) : src_(source) {}

    Token next() {
        skip_whitespace_and_comments();
        if (pos_ >= src_.size()) {
            return {TokenType::END_OF_INPUT, "", line_};
        }

        char c = src_[pos_];
        if (c == '(') { ++pos_; return {TokenType::LPAREN, "(", line_}; }
        if (c == ')') { ++pos_; return {TokenType::RPAREN, ")", line_}; }
        if (c == '"') return read_string();
        return read_symbol();
    }

private:
    std::string_view src_;
    size_t pos_ = 0;
    size_t line_ = 1;

    void skip_whitespace_and_comments() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == '\n') {
                ++line_;
                ++pos_;
            } else if (c == ' ' || c == '\t' || c == '\r') {
                ++pos_;
            } else if (c == ';') {
                // Skip to end of line (Scheme comment)
                while (pos_ < src_.size() && src_[pos_] != '\n') ++pos_;
            } else {
                break;
            }
        }
    }

    Token read_string() {
        size_t start_line = line_;
        ++pos_; // skip opening "
        std::string result;
        while (pos_ < src_.size() && src_[pos_] != '"') {
            if (src_[pos_] == '\\' && pos_ + 1 < src_.size()) {
                ++pos_;
                switch (src_[pos_]) {
                    case 'n':  result += '\n'; break;
                    case 't':  result += '\t'; break;
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    default:   result += src_[pos_]; break;
                }
            } else {
                if (src_[pos_] == '\n') ++line_;
                result += src_[pos_];
            }
            ++pos_;
        }
        if (pos_ < src_.size()) ++pos_; // skip closing "
        return {TokenType::STRING, std::move(result), start_line};
    }

    Token read_symbol() {
        size_t start = pos_;
        size_t start_line = line_;
        while (pos_ < src_.size() && !is_delimiter(src_[pos_])) ++pos_;
        return {TokenType::SYMBOL, std::string(src_.substr(start, pos_ - start)), start_line};
    }

    static bool is_delimiter(char c) {
        return c == '(' || c == ')' || c == '"' || c == ';' ||
               c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }
};

// ============================================================================
// Parser
// ============================================================================

class Parser {
public:
    explicit Parser(std::string_view source) : tok_(source) { advance(); }

    std::vector<SExpr> parse_all() {
        std::vector<SExpr> result;
        while (current_.type != TokenType::END_OF_INPUT) {
            if (current_.type == TokenType::LPAREN) {
                result.push_back(parse_list());
            } else {
                // Skip unexpected top-level tokens
                advance();
            }
        }
        return result;
    }

private:
    Tokenizer tok_;
    Token current_;

    void advance() { current_ = tok_.next(); }

    SExpr parse_list() {
        size_t ln = current_.line;
        advance(); // consume '('

        std::vector<SExpr> children;
        while (current_.type != TokenType::RPAREN &&
               current_.type != TokenType::END_OF_INPUT) {
            if (current_.type == TokenType::LPAREN) {
                children.push_back(parse_list());
            } else {
                // STRING or SYMBOL — both become atoms
                children.push_back(SExpr::make_atom(current_.value, current_.line));
                advance();
            }
        }

        if (current_.type == TokenType::RPAREN) advance(); // consume ')'
        return SExpr::make_list(std::move(children), ln);
    }
};

// ============================================================================
// Type Classification Helpers
// ============================================================================

namespace {

/// Check if an Atomese type name denotes a node (convention: ends with "Node")
bool is_node_type_name(std::string_view name) {
    return name.size() >= 4 &&
           name.substr(name.size() - 4) == "Node";
}

/// Check if an Atomese type name denotes a link (convention: ends with "Link")
bool is_link_type_name(std::string_view name) {
    return name.size() >= 4 &&
           name.substr(name.size() - 4) == "Link";
}

} // anonymous namespace

// ============================================================================
// Evaluator
// ============================================================================

Handle evaluate(AtomSpace& as, const SExpr& expr, LoadResult& result) {
    // Must be a list with at least a type-name head
    if (!expr.is_list || expr.children.empty()) return {};

    const auto& head = expr.children[0];
    if (head.is_list) {
        result.errors++;
        result.error_messages.push_back(
            "Line " + std::to_string(expr.line) +
            ": Expected type name, got nested list");
        return {};
    }

    const std::string& type_name_str = head.atom;

    // Resolve type
    AtomType type = type_from_name(type_name_str);
    if (type == AtomType::INVALID) {
        result.errors++;
        result.error_messages.push_back(
            "Line " + std::to_string(expr.line) +
            ": Unknown type '" + type_name_str + "'");
        return {};
    }

    // Determine node vs link from naming convention
    if (is_node_type_name(type_name_str)) {
        // Node: (TypeName "name")
        if (expr.children.size() < 2) {
            result.errors++;
            result.error_messages.push_back(
                "Line " + std::to_string(expr.line) +
                ": Node '" + type_name_str + "' missing name argument");
            return {};
        }

        const auto& name_expr = expr.children[1];
        if (name_expr.is_list) {
            result.errors++;
            result.error_messages.push_back(
                "Line " + std::to_string(expr.line) +
                ": Node name must be a string, not a list");
            return {};
        }

        auto h = as.add_node(type, name_expr.atom);
        if (h.valid()) result.nodes_created++;
        return h;
    }

    if (is_link_type_name(type_name_str)) {
        // Link: (TypeName child1 child2 ...)
        std::vector<Handle> outgoing;
        outgoing.reserve(expr.children.size() - 1);

        for (size_t i = 1; i < expr.children.size(); ++i) {
            const auto& child = expr.children[i];
            Handle child_h;

            if (child.is_list) {
                // Recursively evaluate sub-expression
                child_h = evaluate(as, child, result);
            } else {
                // Bare symbol — treat as ConceptNode reference
                // This handles shorthand like (ImplicationLink "Pattern-0" "Pattern-1")
                child_h = as.add_node(AtomType::CONCEPT_NODE, child.atom);
                if (child_h.valid()) result.nodes_created++;
            }

            if (child_h.valid()) {
                outgoing.push_back(child_h);
            }
        }

        if (outgoing.empty()) {
            result.errors++;
            result.error_messages.push_back(
                "Line " + std::to_string(expr.line) +
                ": Link '" + type_name_str + "' has no valid children");
            return {};
        }

        auto h = as.add_link(type, std::span<const Handle>(outgoing));
        if (h.valid()) result.links_created++;
        return h;
    }

    // Type name doesn't match Node or Link convention
    result.errors++;
    result.error_messages.push_back(
        "Line " + std::to_string(expr.line) +
        ": Type '" + type_name_str + "' is neither a Node nor a Link");
    return {};
}

// ============================================================================
// Public API
// ============================================================================

std::vector<SExpr> parse(std::string_view source) {
    Parser parser(source);
    return parser.parse_all();
}

LoadResult load_string(AtomSpace& as, std::string_view source) {
    LoadResult result;

    auto expressions = parse(source);
    result.expressions_parsed = expressions.size();

    for (const auto& expr : expressions) {
        evaluate(as, expr, result);
    }

    return result;
}

LoadResult load_file(AtomSpace& as, const std::filesystem::path& path) {
    LoadResult result;

    std::ifstream file(path);
    if (!file.is_open()) {
        result.errors++;
        result.error_messages.push_back(
            "Failed to open file: " + path.string());
        return result;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    result = load_string(as, content);

    // Prefix error messages with filename
    for (auto& msg : result.error_messages) {
        msg = path.filename().string() + ": " + msg;
    }

    return result;
}

LoadResult load_directory(AtomSpace& as, const std::filesystem::path& dir) {
    LoadResult result;

    if (!std::filesystem::is_directory(dir)) {
        result.errors++;
        result.error_messages.push_back(
            "Not a directory: " + dir.string());
        return result;
    }

    // Collect and sort .scm files for deterministic load order
    std::vector<std::filesystem::path> scm_files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".scm") {
            scm_files.push_back(entry.path());
        }
    }
    std::sort(scm_files.begin(), scm_files.end());

    for (const auto& path : scm_files) {
        result.merge(load_file(as, path));
    }

    return result;
}

} // namespace opencog::atomese
