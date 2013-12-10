// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/operators.h"

#include "base/strings/string_number_conversions.h"
#include "tools/gn/err.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"

namespace {

const char kSourcesName[] = "sources";

// Applies the sources assignment filter from the given scope to each element
// of source (can be a list or a string), appending it to dest if it doesn't
// match.
void AppendFilteredSourcesToValue(const Scope* scope,
                                  const Value& source,
                                  Value* dest) {
  const PatternList* filter = scope->GetSourcesAssignmentFilter();

  const std::vector<Value>& source_list = source.list_value();

  if (source.type() == Value::STRING) {
    if (!filter || filter->is_empty() ||
        !filter->MatchesValue(source))
      dest->list_value().push_back(source);
    return;
  }

  // Otherwise source is a list.
  DCHECK(source.type() == Value::LIST);
  if (!filter || filter->is_empty()) {
    // No filter, append everything.
    for (size_t i = 0; i < source_list.size(); i++)
      dest->list_value().push_back(source_list[i]);
    return;
  }

  // Note: don't reserve() the dest vector here since that actually hurts
  // the allocation pattern when the build script is doing multiple small
  // additions.
  for (size_t i = 0; i < source_list.size(); i++) {
    if (!filter->MatchesValue(source_list[i]))
      dest->list_value().push_back(source_list[i]);
  }
}

void RemoveMatchesFromList(const BinaryOpNode* op_node,
                           Value* list,
                           const Value& to_remove,
                           Err* err) {
  std::vector<Value>& v = list->list_value();
  switch (to_remove.type()) {
    case Value::BOOLEAN:
    case Value::INTEGER:  // Filter out the individual int/string.
    case Value::STRING: {
      bool found_match = false;
      for (size_t i = 0; i < v.size(); /* nothing */) {
        if (v[i] == to_remove) {
          found_match = true;
          v.erase(v.begin() + i);
        } else {
          i++;
        }
      }
      if (!found_match) {
        *err = Err(to_remove.origin()->GetRange(), "Item not found",
            "You were trying to remove " + to_remove.ToString(true) +
            "\nfrom the list but it wasn't there.");
      }
      break;
    }

    case Value::LIST:  // Filter out each individual thing.
      for (size_t i = 0; i < to_remove.list_value().size(); i++) {
        // TODO(brettw) if the nested item is a list, we may want to search
        // for the literal list rather than remote the items in it.
        RemoveMatchesFromList(op_node, list, to_remove.list_value()[i], err);
        if (err->has_error())
          return;
      }
      break;

    default:
      break;
  }
}

// Assignment -----------------------------------------------------------------

Value ExecuteEquals(Scope* scope,
                    const BinaryOpNode* op_node,
                    const Token& left,
                    const Value& right,
                    Err* err) {
  const Value* old_value = scope->GetValue(left.value(), false);
  if (old_value) {
    if (scope->IsSetButUnused(left.value())) {
      // Throw an error for re-assigning without using the value first. The
      // exception is that you can overwrite an empty list with another list
      // since this is the way to get around the "can't overwrite a nonempty
      // list with another nonempty list" restriction.
      if (old_value->type() != Value::LIST ||
          !old_value->list_value().empty()) {
        *err = Err(op_node->left()->GetRange(), "Overwriting unused variable.",
            "This overwrites a previous assignment to \"" +
            left.value().as_string() + "\" that had no effect.");
        err->AppendSubErr(Err(*scope->GetValue(left.value()),
                              "Previously set here.",
                              "Maybe you wanted \"+=\" to append instead?"));
        return Value();
      }
    } else {
      // Throw an error when overwriting a nonempty list with another nonempty
      // list item. This is to detect the case where you write
      //   defines = ["FOO"]
      // and you overwrote inherited ones, when instead you mean to append:
      //   defines += ["FOO"]
      if (old_value->type() == Value::LIST &&
          !old_value->list_value().empty() &&
          right.type() == Value::LIST &&
          !right.list_value().empty()) {
        *err = Err(op_node->left()->GetRange(), "Replacing nonempty list.",
            std::string("This overwrites a previously-defined nonempty list ") +
            "(length " + base::IntToString(old_value->list_value().size()) +
            ").");
        err->AppendSubErr(Err(*old_value, "for previous definition",
            "with another one (length " +
            base::IntToString(right.list_value().size()) + "). Did you mean " +
            "\"+=\" to append instead? If you\nreally want to do this, do\n  " +
            left.value().as_string() + " = []\nbefore reassigning."));
        return Value();
      }
    }
  }
  if (err->has_error())
    return Value();

  if (right.type() == Value::LIST && left.value() == kSourcesName) {
    // Assigning to sources, filter the list. Here we do the filtering and
    // copying in one step to save an extra list copy (the lists may be
    // long).
    Value* set_value = scope->SetValue(left.value(),
                                       Value(op_node, Value::LIST), op_node);
    set_value->list_value().reserve(right.list_value().size());
    AppendFilteredSourcesToValue(scope, right, set_value);
  } else {
    // Normal value set, just copy it.
    scope->SetValue(left.value(), right, op_node->right());
  }
  return Value();
}

// allow_type_conversion indicates if we're allowed to change the type of the
// left value. This is set to true when doing +, and false when doing +=.
void ValuePlusEquals(const Scope* scope,
                     const BinaryOpNode* op_node,
                     const Token& left_token,
                     Value* left,
                     const Value& right,
                     bool allow_type_conversion,
                     Err* err) {
  switch (left->type()) {
    // Left-hand-side int.
    case Value::INTEGER:
      switch (right.type()) {
        case Value::INTEGER:  // int + int -> addition.
          left->int_value() += right.int_value();
          return;

        case Value::STRING:  // int + string -> string concat.
          if (allow_type_conversion) {
            *left = Value(op_node,
                base::Int64ToString(left->int_value()) + right.string_value());
            return;
          }
          break;

        default:
          break;
      }
      break;

    // Left-hand-side string.
    case Value::STRING:
      switch (right.type()) {
        case Value::INTEGER:  // string + int -> string concat.
          left->string_value().append(base::Int64ToString(right.int_value()));
          return;

        case Value::STRING:  // string + string -> string contat.
          left->string_value().append(right.string_value());
          return;

        default:
          break;
      }
      break;

    // Left-hand-side list.
    case Value::LIST:
      switch (right.type()) {
        case Value::INTEGER:  // list + integer -> list append.
        case Value::STRING:  // list + string -> list append.
          if (left_token.value() == kSourcesName)
            AppendFilteredSourcesToValue(scope, right, left);
          else
            left->list_value().push_back(right);
          return;

        case Value::LIST:  // list + list -> list concat.
          if (left_token.value() == kSourcesName) {
            // Filter additions through the assignment filter.
            AppendFilteredSourcesToValue(scope, right, left);
          } else {
            // Normal list concat.
            for (size_t i = 0; i < right.list_value().size(); i++)
              left->list_value().push_back(right.list_value()[i]);
          }
          return;

        default:
          break;
      }

    default:
      break;
  }

  *err = Err(op_node->op(), "Incompatible types to add.",
      std::string("I see a ") + Value::DescribeType(left->type()) + " and a " +
      Value::DescribeType(right.type()) + ".");
}

Value ExecutePlusEquals(Scope* scope,
                        const BinaryOpNode* op_node,
                        const Token& left,
                        const Value& right,
                        Err* err) {
  // We modify in-place rather than doing read-modify-write to avoid
  // copying large lists.
  Value* left_value =
      scope->GetValueForcedToCurrentScope(left.value(), op_node);
  if (!left_value) {
    *err = Err(left, "Undefined variable for +=.",
        "I don't have something with this name in scope now.");
    return Value();
  }
  ValuePlusEquals(scope, op_node, left, left_value, right, false, err);
  left_value->set_origin(op_node);
  scope->MarkUnused(left.value());
  return Value();
}

void ValueMinusEquals(const BinaryOpNode* op_node,
                      Value* left,
                      const Value& right,
                      bool allow_type_conversion,
                      Err* err) {
  switch (left->type()) {
    // Left-hand-side int.
    case Value::INTEGER:
      switch (right.type()) {
        case Value::INTEGER:  // int - int -> subtraction.
          left->int_value() -= right.int_value();
          return;

        default:
          break;
      }
      break;

    // Left-hand-side string.
    case Value::STRING:
      break;  // All are errors.

    // Left-hand-side list.
    case Value::LIST:
      RemoveMatchesFromList(op_node, left, right, err);
      return;

    default:
      break;
  }

  *err = Err(op_node->op(), "Incompatible types to add.",
      std::string("I see a ") + Value::DescribeType(left->type()) + " and a " +
      Value::DescribeType(right.type()) + ".");
}

Value ExecuteMinusEquals(Scope* scope,
                         const BinaryOpNode* op_node,
                         const Token& left,
                         const Value& right,
                         Err* err) {
  Value* left_value =
      scope->GetValueForcedToCurrentScope(left.value(), op_node);
  if (!left_value) {
    *err = Err(left, "Undefined variable for -=.",
        "I don't have something with this name in scope now.");
    return Value();
  }
  ValueMinusEquals(op_node, left_value, right, false, err);
  left_value->set_origin(op_node);
  scope->MarkUnused(left.value());
  return Value();
}

// Plus/Minus -----------------------------------------------------------------

Value ExecutePlus(Scope* scope,
                  const BinaryOpNode* op_node,
                  const Value& left,
                  const Value& right,
                  Err* err) {
  Value ret = left;
  ValuePlusEquals(scope, op_node, Token(), &ret, right, true, err);
  ret.set_origin(op_node);
  return ret;
}

Value ExecuteMinus(Scope* scope,
                   const BinaryOpNode* op_node,
                   const Value& left,
                   const Value& right,
                   Err* err) {
  Value ret = left;
  ValueMinusEquals(op_node, &ret, right, true, err);
  ret.set_origin(op_node);
  return ret;
}

// Comparison -----------------------------------------------------------------

Value ExecuteEqualsEquals(Scope* scope,
                          const BinaryOpNode* op_node,
                          const Value& left,
                          const Value& right,
                          Err* err) {
  if (left == right)
    return Value(op_node, true);
  return Value(op_node, false);
}

Value ExecuteNotEquals(Scope* scope,
                       const BinaryOpNode* op_node,
                       const Value& left,
                       const Value& right,
                       Err* err) {
  // Evaluate in terms of ==.
  Value result = ExecuteEqualsEquals(scope, op_node, left, right, err);
  result.int_value() = static_cast<int64>(!result.int_value());
  return result;
}

Value FillNeedsTwoIntegersError(const BinaryOpNode* op_node,
                                const Value& left,
                                const Value& right,
                                Err* err) {
  *err = Err(op_node, "Comparison requires two integers.",
             "This operator can only compare two integers.");
  err->AppendRange(left.origin()->GetRange());
  err->AppendRange(right.origin()->GetRange());
  return Value();
}

Value ExecuteLessEquals(Scope* scope,
                        const BinaryOpNode* op_node,
                        const Value& left,
                        const Value& right,
                        Err* err) {
  if (left.type() != Value::INTEGER || right.type() != Value::INTEGER)
    return FillNeedsTwoIntegersError(op_node, left, right, err);
  return Value(op_node, left.int_value() <= right.int_value());
}

Value ExecuteGreaterEquals(Scope* scope,
                           const BinaryOpNode* op_node,
                           const Value& left,
                           const Value& right,
                           Err* err) {
  if (left.type() != Value::INTEGER || right.type() != Value::INTEGER)
    return FillNeedsTwoIntegersError(op_node, left, right, err);
  return Value(op_node, left.int_value() >= right.int_value());
}

Value ExecuteGreater(Scope* scope,
                     const BinaryOpNode* op_node,
                     const Value& left,
                     const Value& right,
                     Err* err) {
  if (left.type() != Value::INTEGER || right.type() != Value::INTEGER)
    return FillNeedsTwoIntegersError(op_node, left, right, err);
  return Value(op_node, left.int_value() > right.int_value());
}

Value ExecuteLess(Scope* scope,
                  const BinaryOpNode* op_node,
                  const Value& left,
                  const Value& right,
                  Err* err) {
  if (left.type() != Value::INTEGER || right.type() != Value::INTEGER)
    return FillNeedsTwoIntegersError(op_node, left, right, err);
  return Value(op_node, left.int_value() < right.int_value());
}

// Binary ----------------------------------------------------------------------

Value ExecuteOr(Scope* scope,
                const BinaryOpNode* op_node,
                const Value& left,
                const Value& right,
                Err* err) {
  if (left.type() != Value::BOOLEAN) {
    *err = Err(left, "Left side of || operator is not a boolean.");
    err->AppendRange(op_node->GetRange());
  } else if (right.type() != Value::BOOLEAN) {
    *err = Err(right, "Right side of || operator is not a boolean.");
    err->AppendRange(op_node->GetRange());
  }
  return Value(op_node, left.boolean_value() || right.boolean_value());
}

Value ExecuteAnd(Scope* scope,
                 const BinaryOpNode* op_node,
                 const Value& left,
                 const Value& right,
                 Err* err) {
  if (left.type() != Value::BOOLEAN) {
    *err = Err(left, "Left side of && operator is not a boolean.");
    err->AppendRange(op_node->GetRange());
  } else if (right.type() != Value::BOOLEAN) {
    *err = Err(right, "Right side of && operator is not a boolean.");
    err->AppendRange(op_node->GetRange());
  }
  return Value(op_node, left.boolean_value() && right.boolean_value());
}

}  // namespace

// ----------------------------------------------------------------------------

bool IsUnaryOperator(const Token& token) {
  return token.type() == Token::BANG;
}

bool IsBinaryOperator(const Token& token) {
  return token.type() == Token::EQUAL ||
         token.type() == Token::PLUS ||
         token.type() == Token::MINUS ||
         token.type() == Token::PLUS_EQUALS ||
         token.type() == Token::MINUS_EQUALS ||
         token.type() == Token::EQUAL_EQUAL ||
         token.type() == Token::NOT_EQUAL ||
         token.type() == Token::LESS_EQUAL ||
         token.type() == Token::GREATER_EQUAL ||
         token.type() == Token::LESS_THAN ||
         token.type() == Token::GREATER_THAN ||
         token.type() == Token::BOOLEAN_AND ||
         token.type() == Token::BOOLEAN_OR;
}

bool IsFunctionCallArgBeginScoper(const Token& token) {
  return token.type() == Token::LEFT_PAREN;
}

bool IsFunctionCallArgEndScoper(const Token& token) {
  return token.type() == Token::RIGHT_PAREN;
}

bool IsScopeBeginScoper(const Token& token) {
  return token.type() == Token::LEFT_BRACE;
}

bool IsScopeEndScoper(const Token& token) {
  return token.type() == Token::RIGHT_BRACE;
}

Value ExecuteUnaryOperator(Scope* scope,
                           const UnaryOpNode* op_node,
                           const Value& expr,
                           Err* err) {
  DCHECK(op_node->op().type() == Token::BANG);

  if (expr.type() != Value::BOOLEAN) {
    *err = Err(expr, "Operand of ! operator is not a boolean.");
    err->AppendRange(op_node->GetRange());
    return Value();
  }
  // TODO(scottmg): Why no unary minus?
  return Value(op_node, !expr.boolean_value());
}

Value ExecuteBinaryOperator(Scope* scope,
                            const BinaryOpNode* op_node,
                            const ParseNode* left,
                            const ParseNode* right,
                            Err* err) {
  const Token& op = op_node->op();

  // First handle the ones that take an lvalue.
  if (op.type() == Token::EQUAL ||
      op.type() == Token::PLUS_EQUALS ||
      op.type() == Token::MINUS_EQUALS) {
    const IdentifierNode* left_id = left->AsIdentifier();
    if (!left_id) {
      *err = Err(op, "Operator requires an lvalue.",
                 "This thing on the left is not an idenfitier.");
      err->AppendRange(left->GetRange());
      return Value();
    }
    const Token& dest = left_id->value();

    Value right_value = right->Execute(scope, err);
    if (err->has_error())
      return Value();
    if (right_value.type() == Value::NONE) {
      *err = Err(op, "Operator requires an rvalue.",
                 "This thing on the right does not evaluate to a value.");
      err->AppendRange(right->GetRange());
      return Value();
    }

    if (op.type() == Token::EQUAL)
      return ExecuteEquals(scope, op_node, dest, right_value, err);
    if (op.type() == Token::PLUS_EQUALS)
      return ExecutePlusEquals(scope, op_node, dest, right_value, err);
    if (op.type() == Token::MINUS_EQUALS)
      return ExecuteMinusEquals(scope, op_node, dest, right_value, err);
    NOTREACHED();
    return Value();
  }

  // Left value.
  Value left_value = left->Execute(scope, err);
  if (err->has_error())
    return Value();
  if (left_value.type() == Value::NONE) {
    *err = Err(op, "Operator requires an value.",
               "This thing on the left does not evaluate to a value.");
    err->AppendRange(left->GetRange());
    return Value();
  }

  // Right value. Note: don't move this above to share code with the lvalue
  // version since in this case we want to execute the left side first.
  Value right_value = right->Execute(scope, err);
  if (err->has_error())
    return Value();
  if (right_value.type() == Value::NONE) {
    *err = Err(op, "Operator requires an value.",
               "This thing on the right does not evaluate to a value.");
    err->AppendRange(right->GetRange());
    return Value();
  }

  // +, -.
  if (op.type() == Token::MINUS)
    return ExecuteMinus(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::PLUS)
    return ExecutePlus(scope, op_node, left_value, right_value, err);

  // Comparisons.
  if (op.type() == Token::EQUAL_EQUAL)
    return ExecuteEqualsEquals(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::NOT_EQUAL)
    return ExecuteNotEquals(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::GREATER_EQUAL)
    return ExecuteGreaterEquals(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::LESS_EQUAL)
    return ExecuteLessEquals(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::GREATER_THAN)
    return ExecuteGreater(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::LESS_THAN)
    return ExecuteLess(scope, op_node, left_value, right_value, err);

  // ||, &&.
  if (op.type() == Token::BOOLEAN_OR)
    return ExecuteOr(scope, op_node, left_value, right_value, err);
  if (op.type() == Token::BOOLEAN_AND)
    return ExecuteAnd(scope, op_node, left_value, right_value, err);

  return Value();
}
