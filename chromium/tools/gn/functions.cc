// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/functions.h"

#include <iostream>

#include "base/strings/string_util.h"
#include "tools/gn/config.h"
#include "tools/gn/config_values_generator.h"
#include "tools/gn/err.h"
#include "tools/gn/input_file.h"
#include "tools/gn/item_tree.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/target_manager.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"

namespace {

// This is called when a template is invoked. When we see a template
// declaration, that funciton is RunTemplate.
Value RunTemplateInvocation(Scope* scope,
                            const FunctionCallNode* invocation,
                            const std::vector<Value>& args,
                            BlockNode* block,
                            const FunctionCallNode* rule,
                            Err* err) {
  if (!EnsureNotProcessingImport(invocation, scope, err))
    return Value();

  Scope block_scope(scope);
  if (!FillTargetBlockScope(scope, invocation,
                            invocation->function().value().as_string(),
                            block, args, &block_scope, err))
    return Value();

  // Run the block for the rule invocation.
  block->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  // Now run the rule itself with that block as the current scope.
  rule->block()->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  block_scope.CheckForUnusedVars(err);
  return Value();
}

}  // namespace

// ----------------------------------------------------------------------------

bool EnsureNotProcessingImport(const ParseNode* node,
                               const Scope* scope,
                               Err* err) {
  if (scope->IsProcessingImport()) {
    *err = Err(node, "Not valid from an import.",
        "Imports are for defining defaults, variables, and rules. The\n"
        "appropriate place for this kind of thing is really in a normal\n"
        "BUILD file.");
    return false;
  }
  return true;
}

bool EnsureNotProcessingBuildConfig(const ParseNode* node,
                                    const Scope* scope,
                                    Err* err) {
  if (scope->IsProcessingBuildConfig()) {
    *err = Err(node, "Not valid from the build config.",
        "You can't do this kind of thing from the build config script, "
        "silly!\nPut it in a regular BUILD file.");
    return false;
  }
  return true;
}

bool FillTargetBlockScope(const Scope* scope,
                          const FunctionCallNode* function,
                          const std::string& target_type,
                          const BlockNode* block,
                          const std::vector<Value>& args,
                          Scope* block_scope,
                          Err* err) {
  if (!block) {
    FillNeedsBlockError(function, err);
    return false;
  }

  // Copy the target defaults, if any, into the scope we're going to execute
  // the block in.
  const Scope* default_scope = scope->GetTargetDefaults(target_type);
  if (default_scope) {
    if (!default_scope->NonRecursiveMergeTo(block_scope, function,
                                            "target defaults", err))
      return false;
  }

  // The name is the single argument to the target function.
  if (!EnsureSingleStringArg(function, args, err))
    return false;

  // Set the target name variable to the current target, and mark it used
  // because we don't want to issue an error if the script ignores it.
  const base::StringPiece target_name("target_name");
  block_scope->SetValue(target_name, Value(function, args[0].string_value()),
                        function);
  block_scope->MarkUsed(target_name);
  return true;
}

void FillNeedsBlockError(const FunctionCallNode* function, Err* err) {
  *err = Err(function->function(), "This function call requires a block.",
      "The block's \"{\" must be on the same line as the function "
      "call's \")\".");
}

bool EnsureSingleStringArg(const FunctionCallNode* function,
                           const std::vector<Value>& args,
                           Err* err) {
  if (args.size() != 1) {
    *err = Err(function->function(), "Incorrect arguments.",
               "This function requires a single string argument.");
    return false;
  }
  return args[0].VerifyTypeIs(Value::STRING, err);
}

const Label& ToolchainLabelForScope(const Scope* scope) {
  return scope->settings()->toolchain()->label();
}

Label MakeLabelForScope(const Scope* scope,
                        const FunctionCallNode* function,
                        const std::string& name) {
  const Label& toolchain_label = ToolchainLabelForScope(scope);
  return Label(scope->GetSourceDir(), name, toolchain_label.dir(),
               toolchain_label.name());
}

namespace functions {

// assert ----------------------------------------------------------------------

const char kAssert[] = "assert";
const char kAssert_Help[] =
    "assert: Assert an expression is true at generation time.\n"
    "\n"
    "  assert(<condition> [, <error string>])\n"
    "\n"
    "  If the condition is false, the build will fail with an error. If the\n"
    "  optional second argument is provided, that string will be printed\n"
    "  with the error message.\n"
    "\n"
    "Examples:\n"
    "  assert(is_win)\n"
    "  assert(defined(sources), \"Sources must be defined\")\n";

Value RunAssert(Scope* scope,
                const FunctionCallNode* function,
                const std::vector<Value>& args,
                Err* err) {
  if (args.size() != 1 && args.size() != 2) {
    *err = Err(function->function(), "Wrong number of arguments.",
               "assert() takes one or two argument, "
               "were you expecting somethig else?");
  } else if (args[0].type() != Value::BOOLEAN) {
    *err = Err(function->function(), "Assertion value not a bool.");
  } else if (!args[0].boolean_value()) {
    if (args.size() == 2) {
      // Optional string message.
      if (args[1].type() != Value::STRING) {
        *err = Err(function->function(), "Assertion failed.",
            "<<<ERROR MESSAGE IS NOT A STRING>>>");
      } else {
        *err = Err(function->function(), "Assertion failed.",
            args[1].string_value());
      }
    } else {
      *err = Err(function->function(), "Assertion failed.");
    }

    if (args[0].origin()) {
      // If you do "assert(foo)" we'd ideally like to show you where foo was
      // set, and in this case the origin of the args will tell us that.
      // However, if you do "assert(foo && bar)" the source of the value will
      // be the assert like, which isn't so helpful.
      //
      // So we try to see if the args are from the same line or not. This will
      // break if you do "assert(\nfoo && bar)" and we may show the second line
      // as the source, oh well. The way around this is to check to see if the
      // origin node is inside our function call block.
      Location origin_location = args[0].origin()->GetRange().begin();
      if (origin_location.file() != function->function().location().file() ||
          origin_location.line_number() !=
              function->function().location().line_number()) {
        err->AppendSubErr(Err(args[0].origin()->GetRange(), "",
                              "This is where it was set."));
      }
    }
  }
  return Value();
}

// config ----------------------------------------------------------------------

const char kConfig[] = "config";
const char kConfig_Help[] =
    "config: Defines a configuration object.\n"
    "\n"
    "  Configuration objects can be applied to targets and specify sets of\n"
    "  compiler flags, includes, defines, etc. They provide a way to\n"
    "  conveniently group sets of this configuration information.\n"
    "\n"
    "  A config is referenced by its label just like a target.\n"
    "\n"
    "  The values in a config are additive only. If you want to remove a flag\n"
    "  you need to remove the corresponding config that sets it. The final\n"
    "  set of flags, defines, etc. for a target is generated in this order:\n"
    "\n"
    "   1. The values specified directly on the target (rather than using a\n"
    "      config.\n"
    "   2. The configs specified in the target's \"configs\" list, in order.\n"
    "   3. Direct dependent configs from a breadth-first traversal of the\n"
    "      dependency tree in the order that the targets appear in \"deps\".\n"
    "   4. All dependent configs from a breadth-first traversal of the\n"
    "      dependency tree in the order that the targets appear in \"deps\".\n"
    "\n"
    "Variables valid in a config definition:\n"
    CONFIG_VALUES_VARS_HELP
    "\n"
    "Variables on a target used to apply configs:\n"
    "  all_dependent_configs, configs, direct_dependent_configs,\n"
    "  forward_dependent_configs_from\n"
    "\n"
    "Example:\n"
    "  config(\"myconfig\") {\n"
    "    includes = [ \"include/common\" ]\n"
    "    defines = [ \"ENABLE_DOOM_MELON\" ]\n"
    "  }\n"
    "\n"
    "  executable(\"mything\") {\n"
    "    configs = [ \":myconfig\" ]\n"
    "  }\n";

Value RunConfig(const FunctionCallNode* function,
                const std::vector<Value>& args,
                Scope* scope,
                Err* err) {
  if (!EnsureSingleStringArg(function, args, err) ||
      !EnsureNotProcessingImport(function, scope, err))
    return Value();

  Label label(MakeLabelForScope(scope, function, args[0].string_value()));

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Generating config", label.GetUserVisibleName(true));

  // Create the empty config object.
  ItemTree* tree = &scope->settings()->build_settings()->item_tree();
  Config* config = Config::GetConfig(scope->settings(), function->GetRange(),
                                     label, NULL, err);
  if (err->has_error())
    return Value();

  // Fill it.
  const SourceDir& input_dir = scope->GetSourceDir();
  ConfigValuesGenerator gen(&config->config_values(), scope,
                            function->function(), input_dir, err);
  gen.Run();
  if (err->has_error())
    return Value();

  // Mark as complete.
  {
    base::AutoLock lock(tree->lock());
    tree->MarkItemDefinedLocked(scope->settings()->build_settings(), label,
                                err);
  }
  return Value();
}

// declare_args ----------------------------------------------------------------

const char kDeclareArgs[] = "declare_args";
const char kDeclareArgs_Help[] =
    "declare_args: Declare build arguments used by this file.\n"
    "\n"
    "  Introduces the given arguments into the current scope. If they are\n"
    "  not specified on the command line or in a toolchain's arguments,\n"
    "  the default values given in the declare_args block will be used.\n"
    "  However, these defaults will not override command-line values.\n"
    "\n"
    "  See also \"gn help buildargs\" for an overview.\n"
    "\n"
    "Example:\n"
    "  declare_args() {\n"
    "    enable_teleporter = true\n"
    "    enable_doom_melon = false\n"
    "  }\n"
    "\n"
    "  If you want to override the (default disabled) Doom Melon:\n"
    "    gn --args=\"enable_doom_melon=true enable_teleporter=false\"\n"
    "  This also sets the teleporter, but it's already defaulted to on so\n"
    "  it will have no effect.\n";

Value RunDeclareArgs(Scope* scope,
                     const FunctionCallNode* function,
                     const std::vector<Value>& args,
                     BlockNode* block,
                     Err* err) {
  Scope block_scope(scope);
  block->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  // Pass the values from our scope into the Args object for adding to the
  // scope with the proper values (taking into account the defaults given in
  // the block_scope, and arguments passed into the build).
  Scope::KeyValueMap values;
  block_scope.GetCurrentScopeValues(&values);
  scope->settings()->build_settings()->build_args().DeclareArgs(
      values, scope, err);
  return Value();
}

// defined ---------------------------------------------------------------------

const char kDefined[] = "defined";
const char kDefined_Help[] =
    "defined: Returns whether an identifier is defined.\n"
    "\n"
    "  Returns true if the given argument is defined. This is most useful in\n"
    "  templates to assert that the caller set things up properly.\n"
    "\n"
    "Example:\n"
    "\n"
    "  template(\"mytemplate\") {\n"
    "    # To help users call this template properly...\n"
    "    assert(defined(sources), \"Sources must be defined\")\n"
    "\n"
    "    # If we want to accept an optional \"values\" argument, we don't\n"
    "    # want to dereference something that may not be defined.\n"
    "    if (!defined(outputs)) {\n"
    "      outputs = []\n"
    "    }\n"
    "  }\n";

Value RunDefined(Scope* scope,
                 const FunctionCallNode* function,
                 const ListNode* args_list,
                 Err* err) {
  const std::vector<const ParseNode*>& args_vector = args_list->contents();
  const IdentifierNode* identifier = NULL;
  if (args_vector.size() != 1 ||
      !(identifier = args_vector[0]->AsIdentifier())) {
    *err = Err(function, "Bad argument to defined().",
        "defined() takes one argument which should be an identifier.");
    return Value();
  }

  if (scope->GetValue(identifier->value().value()))
    return Value(function, true);
  return Value(function, false);
}

// import ----------------------------------------------------------------------

const char kImport[] = "import";
const char kImport_Help[] =
    "import: Import a file into the current scope.\n"
    "\n"
    "  The import command loads the rules and variables resulting from\n"
    "  executing the given file into the current scope.\n"
    "\n"
    "  By convention, imported files are named with a .gni extension.\n"
    "\n"
    "  It does not do an \"include\". The imported file is executed in a\n"
    "  standalone environment from the caller of the import command. The\n"
    "  results of this execution are cached for other files that import the\n"
    "  same .gni file.\n"
    "\n"
    "  Note that you can not import a BUILD.gn file that's otherwise used\n"
    "  in the build. Files must either be imported or implicitly loaded as\n"
    "  a result of deps rules, but not both.\n"
    "\n"
    "  The imported file's scope will be merged with the scope at the point\n"
    "  import was called. If there is a conflict (both the current scope and\n"
    "  the imported file define some variable or rule with the same name)\n"
    "  a runtime error will be thrown. Therefore, it's good practice to\n"
    "  minimize the stuff that an imported file defines.\n"
    "\n"
    "Examples:\n"
    "\n"
    "  import(\"//build/rules/idl_compilation_rule.gni\")\n"
    "\n"
    "  # Looks in the current directory.\n"
    "  import(\"my_vars.gni\")\n";

Value RunImport(Scope* scope,
                const FunctionCallNode* function,
                const std::vector<Value>& args,
                Err* err) {
  if (!EnsureSingleStringArg(function, args, err) ||
      !EnsureNotProcessingImport(function, scope, err))
    return Value();

  const SourceDir& input_dir = scope->GetSourceDir();
  SourceFile import_file =
      input_dir.ResolveRelativeFile(args[0].string_value());
  scope->settings()->import_manager().DoImport(import_file, function,
                                               scope, err);
  return Value();
}

// set_sources_assignment_filter -----------------------------------------------

const char kSetSourcesAssignmentFilter[] = "set_sources_assignment_filter";
const char kSetSourcesAssignmentFilter_Help[] =
    "set_sources_assignment_filter: Set a pattern to filter source files.\n"
    "\n"
    "  The sources assignment filter is a list of patterns that remove files\n"
    "  from the list implicitly whenever the \"sources\" variable is\n"
    "  assigned to. This is intended to be used to globally filter out files\n"
    "  with platform-specific naming schemes when they don't apply, for\n"
    "  example, you may want to filter out all \"*_win.cc\" files on non-\n"
    "  Windows platforms.\n"
    "\n"
    "  See \"gn help patterns\" for specifics on patterns.\n"
    "\n"
    "  Typically this will be called once in the master build config script\n"
    "  to set up the filter for the current platform. Subsequent calls will\n"
    "  overwrite the previous values.\n"
    "\n"
    "  If you want to bypass the filter and add a file even if it might\n"
    "  be filtered out, call set_sources_assignment_filter([]) to clear the\n"
    "  list of filters. This will apply until the current scope exits\n"
    "\n"
    "Example:\n"
    "  # Filter out all _win files.\n"
    "  set_sources_assignment_filter([ \"*_win.cc\", \"*_win.h\" ])\n";

Value RunSetSourcesAssignmentFilter(Scope* scope,
                                    const FunctionCallNode* function,
                                    const std::vector<Value>& args,
                                    Err* err) {
  if (args.size() != 1) {
    *err = Err(function, "set_sources_assignment_filter takes one argument.");
  } else {
    scoped_ptr<PatternList> f(new PatternList);
    f->SetFromValue(args[0], err);
    if (!err->has_error())
      scope->set_sources_assignment_filter(f.Pass());
  }
  return Value();
}

// print -----------------------------------------------------------------------

const char kPrint[] = "print";
const char kPrint_Help[] =
    "print(...)\n"
    "  Prints all arguments to the console separated by spaces. A newline is\n"
    "  automatically appended to the end.\n"
    "\n"
    "  This function is intended for debugging. Note that build files are run\n"
    "  in parallel so you may get interleaved prints. A buildfile may also\n"
    "  be executed more than once in parallel in the context of different\n"
    "  toolchains so the prints from one file may be duplicated or\n"
    "  interleaved with itself.\n"
    "\n"
    "Examples:\n"
    "  print(\"Hello world\")\n"
    "\n"
    "  print(sources, deps)\n";

Value RunPrint(Scope* scope,
               const FunctionCallNode* function,
               const std::vector<Value>& args,
               Err* err) {
  for (size_t i = 0; i < args.size(); i++) {
    if (i != 0)
      std::cout << " ";
    std::cout << args[i].ToString(false);
  }
  std::cout << std::endl;
  return Value();
}

// -----------------------------------------------------------------------------

FunctionInfo::FunctionInfo()
    : self_evaluating_args_runner(NULL),
      generic_block_runner(NULL),
      executed_block_runner(NULL),
      no_block_runner(NULL),
      help(NULL) {
}

FunctionInfo::FunctionInfo(SelfEvaluatingArgsFunction seaf, const char* in_help)
    : self_evaluating_args_runner(seaf),
      generic_block_runner(NULL),
      executed_block_runner(NULL),
      no_block_runner(NULL),
      help(in_help) {
}

FunctionInfo::FunctionInfo(GenericBlockFunction gbf, const char* in_help)
    : self_evaluating_args_runner(NULL),
      generic_block_runner(gbf),
      executed_block_runner(NULL),
      no_block_runner(NULL),
      help(in_help) {
}

FunctionInfo::FunctionInfo(ExecutedBlockFunction ebf, const char* in_help)
    : self_evaluating_args_runner(NULL),
      generic_block_runner(NULL),
      executed_block_runner(ebf),
      no_block_runner(NULL),
      help(in_help) {
}

FunctionInfo::FunctionInfo(NoBlockFunction nbf, const char* in_help)
    : self_evaluating_args_runner(NULL),
      generic_block_runner(NULL),
      executed_block_runner(NULL),
      no_block_runner(nbf),
      help(in_help) {
}

// Setup the function map via a static initializer. We use this because it
// avoids race conditions without having to do some global setup function or
// locking-heavy singleton checks at runtime. In practice, we always need this
// before we can do anything interesting, so it's OK to wait for the
// initializer.
struct FunctionInfoInitializer {
  FunctionInfoMap map;

  FunctionInfoInitializer() {
    #define INSERT_FUNCTION(command) \
        map[k##command] = FunctionInfo(&Run##command, k##command##_Help);

    INSERT_FUNCTION(Assert)
    INSERT_FUNCTION(Component)
    INSERT_FUNCTION(Config)
    INSERT_FUNCTION(Copy)
    INSERT_FUNCTION(Custom)
    INSERT_FUNCTION(DeclareArgs)
    INSERT_FUNCTION(Defined)
    INSERT_FUNCTION(ExecScript)
    INSERT_FUNCTION(Executable)
    INSERT_FUNCTION(Group)
    INSERT_FUNCTION(Import)
    INSERT_FUNCTION(Print)
    INSERT_FUNCTION(ProcessFileTemplate)
    INSERT_FUNCTION(ReadFile)
    INSERT_FUNCTION(RebasePath)
    INSERT_FUNCTION(SetDefaults)
    INSERT_FUNCTION(SetDefaultToolchain)
    INSERT_FUNCTION(SetSourcesAssignmentFilter)
    INSERT_FUNCTION(SharedLibrary)
    INSERT_FUNCTION(StaticLibrary)
    INSERT_FUNCTION(Template)
    INSERT_FUNCTION(Test)
    INSERT_FUNCTION(Tool)
    INSERT_FUNCTION(Toolchain)
    INSERT_FUNCTION(ToolchainArgs)
    INSERT_FUNCTION(WriteFile)

    #undef INSERT_FUNCTION
  }
};
const FunctionInfoInitializer function_info;

const FunctionInfoMap& GetFunctions() {
  return function_info.map;
}

Value RunFunction(Scope* scope,
                  const FunctionCallNode* function,
                  const ListNode* args_list,
                  BlockNode* block,
                  Err* err) {
  const Token& name = function->function();

  const FunctionInfoMap& function_map = GetFunctions();
  FunctionInfoMap::const_iterator found_function =
      function_map.find(name.value());
  if (found_function == function_map.end()) {
    // No build-in function matching this, check for a template.
    const FunctionCallNode* rule =
        scope->GetTemplate(function->function().value().as_string());
    if (rule) {
      Value args = args_list->Execute(scope, err);
      if (err->has_error())
        return Value();
      return RunTemplateInvocation(scope, function, args.list_value(), block,
                                   rule, err);
    }

    *err = Err(name, "Unknown function.");
    return Value();
  }

  if (found_function->second.self_evaluating_args_runner) {
    return found_function->second.self_evaluating_args_runner(
        scope, function, args_list, err);
  }

  // All other function types take a pre-executed set of args.
  Value args = args_list->Execute(scope, err);
  if (err->has_error())
    return Value();

  if (found_function->second.generic_block_runner) {
    if (!block) {
      FillNeedsBlockError(function, err);
      return Value();
    }
    return found_function->second.generic_block_runner(
        scope, function, args.list_value(), block, err);
  }

  if (found_function->second.executed_block_runner) {
    if (!block) {
      FillNeedsBlockError(function, err);
      return Value();
    }

    Scope block_scope(scope);
    block->ExecuteBlockInScope(&block_scope, err);
    if (err->has_error())
      return Value();
    return found_function->second.executed_block_runner(
        function, args.list_value(), &block_scope, err);
  }

  // Otherwise it's a no-block function.
  return found_function->second.no_block_runner(scope, function,
                                                args.list_value(), err);
}

}  // namespace functions
