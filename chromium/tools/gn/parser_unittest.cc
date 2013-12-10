// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parser.h"
#include "tools/gn/tokenizer.h"

namespace {

bool GetTokens(const InputFile* input, std::vector<Token>* result) {
  result->clear();
  Err err;
  *result = Tokenizer::Tokenize(input, &err);
  return !err.has_error();
}

void DoParserPrintTest(const char* input, const char* expected) {
  std::vector<Token> tokens;
  InputFile input_file(SourceFile("/test"));
  input_file.SetContents(input);
  ASSERT_TRUE(GetTokens(&input_file, &tokens));

  Err err;
  scoped_ptr<ParseNode> result = Parser::Parse(tokens, &err);
  ASSERT_TRUE(result);

  std::ostringstream collector;
  result->Print(collector, 0);

  EXPECT_EQ(expected, collector.str());
}

void DoExpressionPrintTest(const char* input, const char* expected) {
  std::vector<Token> tokens;
  InputFile input_file(SourceFile("/test"));
  input_file.SetContents(input);
  ASSERT_TRUE(GetTokens(&input_file, &tokens));

  Err err;
  scoped_ptr<ParseNode> result = Parser::ParseExpression(tokens, &err);
  ASSERT_TRUE(result);

  std::ostringstream collector;
  result->Print(collector, 0);

  EXPECT_EQ(expected, collector.str());
}

// Expects the tokenizer or parser to identify an error at the given line and
// character.
void DoParserErrorTest(const char* input, int err_line, int err_char) {
  InputFile input_file(SourceFile("/test"));
  input_file.SetContents(input);

  Err err;
  std::vector<Token> tokens = Tokenizer::Tokenize(&input_file, &err);
  if (!err.has_error()) {
    scoped_ptr<ParseNode> result = Parser::Parse(tokens, &err);
    ASSERT_FALSE(result);
    ASSERT_TRUE(err.has_error());
  }

  EXPECT_EQ(err_line, err.location().line_number());
  EXPECT_EQ(err_char, err.location().char_offset());
}

// Expects the tokenizer or parser to identify an error at the given line and
// character.
void DoExpressionErrorTest(const char* input, int err_line, int err_char) {
  InputFile input_file(SourceFile("/test"));
  input_file.SetContents(input);

  Err err;
  std::vector<Token> tokens = Tokenizer::Tokenize(&input_file, &err);
  if (!err.has_error()) {
    scoped_ptr<ParseNode> result = Parser::ParseExpression(tokens, &err);
    ASSERT_FALSE(result);
    ASSERT_TRUE(err.has_error());
  }

  EXPECT_EQ(err_line, err.location().line_number());
  EXPECT_EQ(err_char, err.location().char_offset());
}

}  // namespace

TEST(Parser, Literal) {
  DoExpressionPrintTest("5", "LITERAL(5)\n");
  DoExpressionPrintTest("\"stuff\"", "LITERAL(\"stuff\")\n");
}

TEST(Parser, BinaryOp) {
  // TODO(scottmg): The tokenizer is dumb, and treats "5-1" as two integers,
  // not a binary operator between two positive integers.
  DoExpressionPrintTest("5 - 1",
      "BINARY(-)\n"
      " LITERAL(5)\n"
      " LITERAL(1)\n");
  DoExpressionPrintTest("5+1",
      "BINARY(+)\n"
      " LITERAL(5)\n"
      " LITERAL(1)\n");
  DoExpressionPrintTest("5 - 1 - 2",
      "BINARY(-)\n"
      " BINARY(-)\n"
      "  LITERAL(5)\n"
      "  LITERAL(1)\n"
      " LITERAL(2)\n");
}

TEST(Parser, FunctionCall) {
  DoExpressionPrintTest("foo()",
      "FUNCTION(foo)\n"
      " LIST\n");
  DoExpressionPrintTest("blah(1, 2)",
      "FUNCTION(blah)\n"
      " LIST\n"
      "  LITERAL(1)\n"
      "  LITERAL(2)\n");
  DoExpressionErrorTest("foo(1, 2,)", 1, 10);
}

TEST(Parser, ParenExpression) {
  const char* input = "(foo(1)) + (a + (b - c) + d)";
  const char* expected =
      "BINARY(+)\n"
      " FUNCTION(foo)\n"
      "  LIST\n"
      "   LITERAL(1)\n"
      " BINARY(+)\n"
      "  BINARY(+)\n"
      "   IDENTIFIER(a)\n"
      "   BINARY(-)\n"
      "    IDENTIFIER(b)\n"
      "    IDENTIFIER(c)\n"
      "  IDENTIFIER(d)\n";
  DoExpressionPrintTest(input, expected);
  DoExpressionErrorTest("(a +", 1, 4);
}

TEST(Parser, OrderOfOperationsLeftAssociative) {
  const char* input = "5 - 1 - 2\n";
  const char* expected =
      "BINARY(-)\n"
      " BINARY(-)\n"
      "  LITERAL(5)\n"
      "  LITERAL(1)\n"
      " LITERAL(2)\n";
  DoExpressionPrintTest(input, expected);
}

TEST(Parser, OrderOfOperationsEqualityBoolean) {
  const char* input =
      "if (a == \"b\" && is_stuff) {\n"
      "  print(\"hai\")\n"
      "}\n";
  const char* expected =
      "BLOCK\n"
      " CONDITION\n"
      "  BINARY(&&)\n"
      "   BINARY(==)\n"
      "    IDENTIFIER(a)\n"
      "    LITERAL(\"b\")\n"
      "   IDENTIFIER(is_stuff)\n"
      "  BLOCK\n"
      "   FUNCTION(print)\n"
      "    LIST\n"
      "     LITERAL(\"hai\")\n";
  DoParserPrintTest(input, expected);
}

TEST(Parser, UnaryOp) {
  DoExpressionPrintTest("!foo",
      "UNARY(!)\n"
      " IDENTIFIER(foo)\n");
}

TEST(Parser, List) {
  DoExpressionPrintTest("[]", "LIST\n");
  DoExpressionPrintTest("[1,asd,]",
      "LIST\n"
      " LITERAL(1)\n"
      " IDENTIFIER(asd)\n");
  DoExpressionPrintTest("[1, 2+3 - foo]",
      "LIST\n"
      " LITERAL(1)\n"
      " BINARY(-)\n"
      "  BINARY(+)\n"
      "   LITERAL(2)\n"
      "   LITERAL(3)\n"
      "  IDENTIFIER(foo)\n");
  DoExpressionPrintTest("[1,\n2,\n 3,\n  4]",
      "LIST\n"
      " LITERAL(1)\n"
      " LITERAL(2)\n"
      " LITERAL(3)\n"
      " LITERAL(4)\n");

  DoExpressionErrorTest("[a, 2+,]", 1, 6);
  DoExpressionErrorTest("[,]", 1, 2);
  DoExpressionErrorTest("[a,,]", 1, 4);
}

TEST(Parser, Assignment) {
  DoParserPrintTest("a=2",
                    "BLOCK\n"
                    " BINARY(=)\n"
                    "  IDENTIFIER(a)\n"
                    "  LITERAL(2)\n");
}

TEST(Parser, Accessor) {
  DoParserPrintTest("a=b[2]",
                    "BLOCK\n"
                    " BINARY(=)\n"
                    "  IDENTIFIER(a)\n"
                    "  ACCESSOR\n"
                    "   b\n"  // AccessorNode is a bit weird in that it holds
                              // a Token, not a ParseNode for the base.
                    "   LITERAL(2)\n");
  DoParserErrorTest("a = b[1][0]", 1, 5);
}

TEST(Parser, Condition) {
  DoParserPrintTest("if(1) { a = 2 }",
                    "BLOCK\n"
                    " CONDITION\n"
                    "  LITERAL(1)\n"
                    "  BLOCK\n"
                    "   BINARY(=)\n"
                    "    IDENTIFIER(a)\n"
                    "    LITERAL(2)\n");

  DoParserPrintTest("if(1) { a = 2 } else if (0) { a = 3 } else { a = 4 }",
                    "BLOCK\n"
                    " CONDITION\n"
                    "  LITERAL(1)\n"
                    "  BLOCK\n"
                    "   BINARY(=)\n"
                    "    IDENTIFIER(a)\n"
                    "    LITERAL(2)\n"
                    "  CONDITION\n"
                    "   LITERAL(0)\n"
                    "   BLOCK\n"
                    "    BINARY(=)\n"
                    "     IDENTIFIER(a)\n"
                    "     LITERAL(3)\n"
                    "   BLOCK\n"
                    "    BINARY(=)\n"
                    "     IDENTIFIER(a)\n"
                    "     LITERAL(4)\n");
}

TEST(Parser, OnlyCallAndAssignInBody) {
  DoParserErrorTest("[]", 1, 2);
  DoParserErrorTest("3 + 4", 1, 5);
  DoParserErrorTest("6 - 7", 1, 5);
  DoParserErrorTest("if (1) { 5 } else { print(4) }", 1, 12);
}

TEST(Parser, NoAssignmentInCondition) {
  DoParserErrorTest("if (a=2) {}", 1, 5);
}

TEST(Parser, CompleteFunction) {
  const char* input =
      "cc_test(\"foo\") {\n"
      "  sources = [\n"
      "    \"foo.cc\",\n"
      "    \"foo.h\"\n"
      "  ]\n"
      "  dependencies = [\n"
      "    \"base\"\n"
      "  ]\n"
      "}\n";
  const char* expected =
      "BLOCK\n"
      " FUNCTION(cc_test)\n"
      "  LIST\n"
      "   LITERAL(\"foo\")\n"
      "  BLOCK\n"
      "   BINARY(=)\n"
      "    IDENTIFIER(sources)\n"
      "    LIST\n"
      "     LITERAL(\"foo.cc\")\n"
      "     LITERAL(\"foo.h\")\n"
      "   BINARY(=)\n"
      "    IDENTIFIER(dependencies)\n"
      "    LIST\n"
      "     LITERAL(\"base\")\n";
  DoParserPrintTest(input, expected);
}

TEST(Parser, FunctionWithConditional) {
  const char* input =
      "cc_test(\"foo\") {\n"
      "  sources = [\"foo.cc\"]\n"
      "  if (OS == \"mac\") {\n"
      "    sources += \"bar.cc\"\n"
      "  } else if (OS == \"win\") {\n"
      "    sources -= [\"asd.cc\", \"foo.cc\"]\n"
      "  } else {\n"
      "    dependencies += [\"bar.cc\"]\n"
      "  }\n"
      "}\n";
  const char* expected =
      "BLOCK\n"
      " FUNCTION(cc_test)\n"
      "  LIST\n"
      "   LITERAL(\"foo\")\n"
      "  BLOCK\n"
      "   BINARY(=)\n"
      "    IDENTIFIER(sources)\n"
      "    LIST\n"
      "     LITERAL(\"foo.cc\")\n"
      "   CONDITION\n"
      "    BINARY(==)\n"
      "     IDENTIFIER(OS)\n"
      "     LITERAL(\"mac\")\n"
      "    BLOCK\n"
      "     BINARY(+=)\n"
      "      IDENTIFIER(sources)\n"
      "      LITERAL(\"bar.cc\")\n"
      "    CONDITION\n"
      "     BINARY(==)\n"
      "      IDENTIFIER(OS)\n"
      "      LITERAL(\"win\")\n"
      "     BLOCK\n"
      "      BINARY(-=)\n"
      "       IDENTIFIER(sources)\n"
      "       LIST\n"
      "        LITERAL(\"asd.cc\")\n"
      "        LITERAL(\"foo.cc\")\n"
      "     BLOCK\n"
      "      BINARY(+=)\n"
      "       IDENTIFIER(dependencies)\n"
      "       LIST\n"
      "        LITERAL(\"bar.cc\")\n";
  DoParserPrintTest(input, expected);
}

TEST(Parser, NestedBlocks) {
  const char* input = "{cc_test(\"foo\") {{foo=1}\n{}}}";
  const char* expected =
      "BLOCK\n"
      " BLOCK\n"
      "  FUNCTION(cc_test)\n"
      "   LIST\n"
      "    LITERAL(\"foo\")\n"
      "   BLOCK\n"
      "    BLOCK\n"
      "     BINARY(=)\n"
      "      IDENTIFIER(foo)\n"
      "      LITERAL(1)\n"
      "    BLOCK\n";
  DoParserPrintTest(input, expected);
  const char* input_with_newline = "{cc_test(\"foo\") {{foo=1}\n{}}}";
  DoParserPrintTest(input_with_newline, expected);
}

TEST(Parser, UnterminatedBlock) {
  DoParserErrorTest("stuff() {", 1, 9);
}

TEST(Parser, BadlyTerminatedNumber) {
  DoParserErrorTest("1234z", 1, 5);
}

TEST(Parser, NewlinesInUnusualPlaces) {
  DoParserPrintTest(
      "if\n"
      "(\n"
      "a\n"
      ")\n"
      "{\n"
      "}\n",
      "BLOCK\n"
      " CONDITION\n"
      "  IDENTIFIER(a)\n"
      "  BLOCK\n");
}

TEST(Parser, NewlinesInUnusualPlaces2) {
  DoParserPrintTest(
      "a\n=\n2\n",
      "BLOCK\n"
      " BINARY(=)\n"
      "  IDENTIFIER(a)\n"
      "  LITERAL(2)\n");
  DoParserPrintTest(
      "x =\ny if\n(1\n) {}",
      "BLOCK\n"
      " BINARY(=)\n"
      "  IDENTIFIER(x)\n"
      "  IDENTIFIER(y)\n"
      " CONDITION\n"
      "  LITERAL(1)\n"
      "  BLOCK\n");
  DoParserPrintTest(
      "x = 3\n+2",
      "BLOCK\n"
      " BINARY(=)\n"
      "  IDENTIFIER(x)\n"
      "  BINARY(+)\n"
      "   LITERAL(3)\n"
      "   LITERAL(2)\n"
      );
}

TEST(Parser, NewlineBeforeSubscript) {
  const char* input = "a = b[1]";
  const char* input_with_newline = "a = b\n[1]";
  const char* expected =
    "BLOCK\n"
    " BINARY(=)\n"
    "  IDENTIFIER(a)\n"
    "  ACCESSOR\n"
    "   b\n"
    "   LITERAL(1)\n";
  DoParserPrintTest(
      input,
      expected);
  DoParserPrintTest(
      input_with_newline,
      expected);
}

TEST(Parser, SequenceOfExpressions) {
  DoParserPrintTest(
      "a = 1 b = 2",
      "BLOCK\n"
      " BINARY(=)\n"
      "  IDENTIFIER(a)\n"
      "  LITERAL(1)\n"
      " BINARY(=)\n"
      "  IDENTIFIER(b)\n"
      "  LITERAL(2)\n");
}

TEST(Parser, BlockAfterFunction) {
  const char* input = "func(\"stuff\") {\n}";
  // TODO(scottmg): Do we really want these to mean different things?
  const char* input_with_newline = "func(\"stuff\")\n{\n}";
  const char* expected =
    "BLOCK\n"
    " FUNCTION(func)\n"
    "  LIST\n"
    "   LITERAL(\"stuff\")\n"
    "  BLOCK\n";
  DoParserPrintTest(input, expected);
  DoParserPrintTest(input_with_newline, expected);
}

TEST(Parser, LongExpression) {
  const char* input = "a = b + c && d || e";
  const char* expected =
    "BLOCK\n"
    " BINARY(=)\n"
    "  IDENTIFIER(a)\n"
    "  BINARY(||)\n"
    "   BINARY(&&)\n"
    "    BINARY(+)\n"
    "     IDENTIFIER(b)\n"
    "     IDENTIFIER(c)\n"
    "    IDENTIFIER(d)\n"
    "   IDENTIFIER(e)\n";
  DoParserPrintTest(input, expected);
}

TEST(Parser, HangingIf) {
  DoParserErrorTest("if", 1, 1);
}

TEST(Parser, NegatingList) {
  DoParserErrorTest("executable(\"wee\") { sources =- [ \"foo.cc\" ] }", 1, 30);
}
