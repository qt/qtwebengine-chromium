// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/parser.h"

#include "base/logging.h"
#include "tools/gn/functions.h"
#include "tools/gn/operators.h"
#include "tools/gn/token.h"

// grammar:
//
// file       := (statement)*
// statement  := block | if | assignment
// block      := '{' statement* '}'
// if         := 'if' '(' expr ')' statement [ else ]
// else       := 'else' (if | statement)*
// assignment := ident {'=' | '+=' | '-='} expr

namespace {

// Returns true if the two tokens are on the same line. We assume they're in
// the same file.
bool IsSameLine(const Token& a, const Token& b) {
  DCHECK(a.location().file() == b.location().file());
  return a.location().line_number() == b.location().line_number();
}

}  // namespace

enum Precedence {
  PRECEDENCE_ASSIGNMENT = 1,
  PRECEDENCE_OR = 2,
  PRECEDENCE_AND = 3,
  PRECEDENCE_EQUALITY = 4,
  PRECEDENCE_RELATION = 5,
  PRECEDENCE_SUM = 6,
  PRECEDENCE_PREFIX = 7,
  PRECEDENCE_CALL = 8,
};

// The top-level for blocks/ifs is still recursive descent, the expression
// parser is a Pratt parser. The basic idea there is to have the precedences
// (and associativities) encoded relative to each other and only parse up
// until you hit something of that precedence. There's a dispatch table in
// expressions_ at the top of parser.cc that describes how each token
// dispatches if it's seen as either a prefix or infix operator, and if it's
// infix, what its precedence is.
//
// Refs:
// - http://javascript.crockford.com/tdop/tdop.html
// - http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/

// Indexed by Token::Type.
ParserHelper Parser::expressions_[] = {
  {NULL, NULL, -1},                                             // INVALID
  {&Parser::Literal, NULL, -1},                                 // INTEGER
  {&Parser::Literal, NULL, -1},                                 // STRING
  {&Parser::Literal, NULL, -1},                                 // TRUE_TOKEN
  {&Parser::Literal, NULL, -1},                                 // FALSE_TOKEN
  {NULL, &Parser::Assignment, PRECEDENCE_ASSIGNMENT},           // EQUAL
  {NULL, &Parser::BinaryOperator, PRECEDENCE_SUM},              // PLUS
  {NULL, &Parser::BinaryOperator, PRECEDENCE_SUM},              // MINUS
  {NULL, &Parser::Assignment, PRECEDENCE_ASSIGNMENT},           // PLUS_EQUALS
  {NULL, &Parser::Assignment, PRECEDENCE_ASSIGNMENT},           // MINUS_EQUALS
  {NULL, &Parser::BinaryOperator, PRECEDENCE_EQUALITY},         // EQUAL_EQUAL
  {NULL, &Parser::BinaryOperator, PRECEDENCE_EQUALITY},         // NOT_EQUAL
  {NULL, &Parser::BinaryOperator, PRECEDENCE_RELATION},         // LESS_EQUAL
  {NULL, &Parser::BinaryOperator, PRECEDENCE_RELATION},         // GREATER_EQUAL
  {NULL, &Parser::BinaryOperator, PRECEDENCE_RELATION},         // LESS_THAN
  {NULL, &Parser::BinaryOperator, PRECEDENCE_RELATION},         // GREATER_THAN
  {NULL, &Parser::BinaryOperator, PRECEDENCE_AND},              // BOOLEAN_AND
  {NULL, &Parser::BinaryOperator, PRECEDENCE_OR},               // BOOLEAN_OR
  {&Parser::Not, NULL, -1},                                     // BANG
  {&Parser::Group, NULL, -1},                                   // LEFT_PAREN
  {NULL, NULL, -1},                                             // RIGHT_PAREN
  {&Parser::List, &Parser::Subscript, PRECEDENCE_CALL},         // LEFT_BRACKET
  {NULL, NULL, -1},                                             // RIGHT_BRACKET
  {NULL, NULL, -1},                                             // LEFT_BRACE
  {NULL, NULL, -1},                                             // RIGHT_BRACE
  {NULL, NULL, -1},                                             // IF
  {NULL, NULL, -1},                                             // ELSE
  {&Parser::Name, &Parser::IdentifierOrCall, PRECEDENCE_CALL},  // IDENTIFIER
  {NULL, NULL, -1},                                             // COMMA
  {NULL, NULL, -1},                                             // COMMENT
};

Parser::Parser(const std::vector<Token>& tokens, Err* err)
    : tokens_(tokens), err_(err), cur_(0) {
}

Parser::~Parser() {
}

// static
scoped_ptr<ParseNode> Parser::Parse(const std::vector<Token>& tokens,
                                    Err* err) {
  Parser p(tokens, err);
  return p.ParseFile().PassAs<ParseNode>();
}

// static
scoped_ptr<ParseNode> Parser::ParseExpression(const std::vector<Token>& tokens,
                                              Err* err) {
  Parser p(tokens, err);
  return p.ParseExpression().Pass();
}

bool Parser::IsAssignment(const ParseNode* node) const {
  return node && node->AsBinaryOp() &&
         (node->AsBinaryOp()->op().type() == Token::EQUAL ||
          node->AsBinaryOp()->op().type() == Token::PLUS_EQUALS ||
          node->AsBinaryOp()->op().type() == Token::MINUS_EQUALS);
}

bool Parser::IsStatementBreak(Token::Type token_type) const {
  switch (token_type) {
    case Token::IDENTIFIER:
    case Token::LEFT_BRACE:
    case Token::RIGHT_BRACE:
    case Token::IF:
    case Token::ELSE:
      return true;
    default:
      return false;
  }
}

bool Parser::LookAhead(Token::Type type) {
  if (at_end())
    return false;
  return cur_token().type() == type;
}

bool Parser::Match(Token::Type type) {
  if (!LookAhead(type))
    return false;
  Consume();
  return true;
}

Token Parser::Consume(Token::Type type, const char* error_message) {
  Token::Type types[1] = { type };
  return Consume(types, 1, error_message);
}

Token Parser::Consume(Token::Type* types,
                      size_t num_types,
                      const char* error_message) {
  if (has_error()) {
    // Don't overwrite current error, but make progress through tokens so that
    // a loop that's expecting a particular token will still terminate.
    cur_++;
    return Token(Location(), Token::INVALID, base::StringPiece());
  }
  if (at_end()) {
    const char kEOFMsg[] = "I hit EOF instead.";
    if (tokens_.empty())
      *err_ = Err(Location(), error_message, kEOFMsg);
    else
      *err_ = Err(tokens_[tokens_.size() - 1], error_message, kEOFMsg);
    return Token(Location(), Token::INVALID, base::StringPiece());
  }

  for (size_t i = 0; i < num_types; ++i) {
    if (cur_token().type() == types[i])
      return tokens_[cur_++];
  }
  *err_ = Err(cur_token(), error_message);
  return Token(Location(), Token::INVALID, base::StringPiece());
}

Token Parser::Consume() {
  return tokens_[cur_++];
}

scoped_ptr<ParseNode> Parser::ParseExpression() {
  return ParseExpression(0);
}

scoped_ptr<ParseNode> Parser::ParseExpression(int precedence) {
  if (at_end())
    return scoped_ptr<ParseNode>();

  Token token = Consume();
  PrefixFunc prefix = expressions_[token.type()].prefix;

  if (prefix == NULL) {
    *err_ = Err(token,
                std::string("Unexpected token '") + token.value().as_string() +
                    std::string("'"));
    return scoped_ptr<ParseNode>();
  }

  scoped_ptr<ParseNode> left = (this->*prefix)(token);
  if (has_error())
    return left.Pass();

  while (!at_end() && !IsStatementBreak(cur_token().type()) &&
         precedence <= expressions_[cur_token().type()].precedence) {
    token = Consume();
    InfixFunc infix = expressions_[token.type()].infix;
    if (infix == NULL) {
      *err_ = Err(token,
                  std::string("Unexpected token '") +
                      token.value().as_string() + std::string("'"));
      return scoped_ptr<ParseNode>();
    }
    left = (this->*infix)(left.Pass(), token);
    if (has_error())
      return scoped_ptr<ParseNode>();
  }

  return left.Pass();
}

scoped_ptr<ParseNode> Parser::Literal(Token token) {
  return scoped_ptr<ParseNode>(new LiteralNode(token)).Pass();
}

scoped_ptr<ParseNode> Parser::Name(Token token) {
  return IdentifierOrCall(scoped_ptr<ParseNode>(), token).Pass();
}

scoped_ptr<ParseNode> Parser::Group(Token token) {
  scoped_ptr<ParseNode> expr = ParseExpression();
  if (has_error())
    return scoped_ptr<ParseNode>();
  Consume(Token::RIGHT_PAREN, "Expected ')'");
  return expr.Pass();
}

scoped_ptr<ParseNode> Parser::Not(Token token) {
  scoped_ptr<ParseNode> expr = ParseExpression(PRECEDENCE_PREFIX + 1);
  if (has_error())
    return scoped_ptr<ParseNode>();
  scoped_ptr<UnaryOpNode> unary_op(new UnaryOpNode);
  unary_op->set_op(token);
  unary_op->set_operand(expr.Pass());
  return unary_op.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::List(Token node) {
  scoped_ptr<ParseNode> list(ParseList(Token::RIGHT_BRACKET, true));
  if (!has_error() && !at_end())
    Consume(Token::RIGHT_BRACKET, "Expected ']'");
  return list.Pass();
}

scoped_ptr<ParseNode> Parser::BinaryOperator(scoped_ptr<ParseNode> left,
                                             Token token) {
  scoped_ptr<ParseNode> right =
      ParseExpression(expressions_[token.type()].precedence + 1);
  if (!right) {
    *err_ =
        Err(token,
            "Expected right hand side for '" + token.value().as_string() + "'");
    return scoped_ptr<ParseNode>();
  }
  scoped_ptr<BinaryOpNode> binary_op(new BinaryOpNode);
  binary_op->set_op(token);
  binary_op->set_left(left.Pass());
  binary_op->set_right(right.Pass());
  return binary_op.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::IdentifierOrCall(scoped_ptr<ParseNode> left,
                                               Token token) {
  scoped_ptr<ListNode> list(new ListNode);
  list->set_begin_token(token);
  list->set_end_token(token);
  scoped_ptr<BlockNode> block;
  bool has_arg = false;
  if (Match(Token::LEFT_PAREN)) {
    // Parsing a function call.
    has_arg = true;
    if (Match(Token::RIGHT_PAREN)) {
      // Nothing, just an empty call.
    } else {
      list = ParseList(Token::RIGHT_PAREN, false);
      if (has_error())
        return scoped_ptr<ParseNode>();
      Consume(Token::RIGHT_PAREN, "Expected ')' after call");
    }
    // Optionally with a scope.
    if (LookAhead(Token::LEFT_BRACE)) {
      block = ParseBlock();
      if (has_error())
        return scoped_ptr<ParseNode>();
    }
  }

  if (!left && !has_arg) {
    // Not a function call, just a standalone identifier.
    return scoped_ptr<ParseNode>(new IdentifierNode(token)).Pass();
  }
  scoped_ptr<FunctionCallNode> func_call(new FunctionCallNode);
  func_call->set_function(token);
  func_call->set_args(list.Pass());
  if (block)
    func_call->set_block(block.Pass());
  return func_call.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::Assignment(scoped_ptr<ParseNode> left,
                                         Token token) {
  if (left->AsIdentifier() == NULL) {
    *err_ = Err(left.get(), "Left-hand side of assignment must be identifier.");
    return scoped_ptr<ParseNode>();
  }
  scoped_ptr<ParseNode> value = ParseExpression(PRECEDENCE_ASSIGNMENT);
  scoped_ptr<BinaryOpNode> assign(new BinaryOpNode);
  assign->set_op(token);
  assign->set_left(left.Pass());
  assign->set_right(value.Pass());
  return assign.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::Subscript(scoped_ptr<ParseNode> left,
                                        Token token) {
  // TODO: Maybe support more complex expressions like a[0][0]. This would
  // require work on the evaluator too.
  if (left->AsIdentifier() == NULL) {
    *err_ = Err(left.get(), "May only subscript simple identifiers");
    return scoped_ptr<ParseNode>();
  }
  scoped_ptr<ParseNode> value = ParseExpression();
  Consume(Token::RIGHT_BRACKET, "Expecting ']' after subscript.");
  scoped_ptr<AccessorNode> accessor(new AccessorNode);
  accessor->set_base(left->AsIdentifier()->value());
  accessor->set_index(value.Pass());
  return accessor.PassAs<ParseNode>();
}

// Does not Consume the start or end token.
scoped_ptr<ListNode> Parser::ParseList(Token::Type stop_before,
                                       bool allow_trailing_comma) {
  scoped_ptr<ListNode> list(new ListNode);
  list->set_begin_token(cur_token());
  bool just_got_comma = false;
  while (!LookAhead(stop_before)) {
    just_got_comma = false;
    // Why _OR? We're parsing things that are higher precedence than the ,
    // that separates the items of the list. , should appear lower than
    // boolean expressions (the lowest of which is OR), but above assignments.
    list->append_item(ParseExpression(PRECEDENCE_OR));
    if (has_error())
      return scoped_ptr<ListNode>();
    if (at_end()) {
      *err_ =
          Err(tokens_[tokens_.size() - 1], "Unexpected end of file in list.");
      return scoped_ptr<ListNode>();
    }
    just_got_comma = Match(Token::COMMA);
  }
  if (just_got_comma && !allow_trailing_comma) {
    *err_ = Err(cur_token(), "Trailing comma");
    return scoped_ptr<ListNode>();
  }
  list->set_end_token(cur_token());
  return list.Pass();
}

scoped_ptr<ParseNode> Parser::ParseFile() {
  scoped_ptr<BlockNode> file(new BlockNode(false));
  for (;;) {
    if (at_end())
      break;
    scoped_ptr<ParseNode> statement = ParseStatement();
    if (!statement)
      break;
    file->append_statement(statement.Pass());
  }
  if (!at_end() && !has_error())
    *err_ = Err(cur_token(), "Unexpected here, should be newline.");
  if (has_error())
    return scoped_ptr<ParseNode>();
  return file.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::ParseStatement() {
  if (LookAhead(Token::LEFT_BRACE)) {
    return ParseBlock().PassAs<ParseNode>();
  } else if (LookAhead(Token::IF)) {
    return ParseCondition();
  } else {
    // TODO(scottmg): Is this too strict? Just drop all the testing if we want
    // to allow "pointless" expressions and return ParseExpression() directly.
    scoped_ptr<ParseNode> stmt = ParseExpression();
    if (stmt) {
      if (stmt->AsFunctionCall() || IsAssignment(stmt.get()))
        return stmt.Pass();
    }
    if (!has_error()) {
      Token token = at_end() ? tokens_[tokens_.size() - 1] : cur_token();
      *err_ = Err(token, "Expecting assignment or function call.");
    }
    return scoped_ptr<ParseNode>();
  }
}

scoped_ptr<BlockNode> Parser::ParseBlock() {
  Token begin_token =
      Consume(Token::LEFT_BRACE, "Expected '{' to start a block.");
  if (has_error())
    return scoped_ptr<BlockNode>();
  scoped_ptr<BlockNode> block(new BlockNode(true));
  block->set_begin_token(begin_token);

  for (;;) {
    if (LookAhead(Token::RIGHT_BRACE)) {
      block->set_end_token(Consume());
      break;
    }

    scoped_ptr<ParseNode> statement = ParseStatement();
    if (!statement)
      return scoped_ptr<BlockNode>();
    block->append_statement(statement.Pass());
  }
  return block.Pass();
}

scoped_ptr<ParseNode> Parser::ParseCondition() {
  scoped_ptr<ConditionNode> condition(new ConditionNode);
  Consume(Token::IF, "Expected 'if'");
  Consume(Token::LEFT_PAREN, "Expected '(' after 'if'.");
  condition->set_condition(ParseExpression());
  if (IsAssignment(condition->condition()))
    *err_ = Err(condition->condition(), "Assignment not allowed in 'if'.");
  Consume(Token::RIGHT_PAREN, "Expected ')' after condition of 'if'.");
  condition->set_if_true(ParseBlock().Pass());
  if (Match(Token::ELSE))
    condition->set_if_false(ParseStatement().Pass());
  if (has_error())
    return scoped_ptr<ParseNode>();
  return condition.PassAs<ParseNode>();
}
