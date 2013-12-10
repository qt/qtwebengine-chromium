// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/command_line.h"
#include "tools/gn/commands.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file.h"
#include "tools/gn/item.h"
#include "tools/gn/item_node.h"
#include "tools/gn/pattern.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/target.h"

namespace commands {

namespace {

// Returns the file path generating this item node.
base::FilePath FilePathForItemNode(const ItemNode& node) {
  const InputFile* file = node.generated_from_here().begin().file();
  if (!file)
    return base::FilePath(FILE_PATH_LITERAL("=UNRESOLVED DEPENDENCY="));
  return file->physical_name();
}

}  // namespace

const char kRefs[] = "refs";
const char kRefs_HelpShort[] =
    "refs: Find stuff referencing a target, directory, or config.";
const char kRefs_Help[] =
    "gn refs <label_pattern> [--files]\n"
    "  Finds code referencing a given label. The label can be a\n"
    "  target or config name. Unlike most other commands, unresolved\n"
    "  dependencies will be tolerated. This allows you to use this command\n"
    "  to find references to targets you're in the process of moving.\n"
    "\n"
    "  By default, the mapping from source item to dest item (where the\n"
    "  pattern matches the dest item). See \"gn help pattern\" for\n"
    "  information on pattern-matching rules.\n"
    "\n"
    "Option:\n"
    "  --files\n"
    "      Output unique filenames referencing a matched target or config.\n"
    "\n"
    "Examples:\n"
    "  gn refs \"//tools/gn/*\"\n"
    "      Find all targets depending on any target or config in the\n"
    "      \"tools/gn\" directory.\n"
    "\n"
    "  gn refs //tools/gn:gn\n"
    "      Find all targets depending on the given exact target name.\n"
    "\n"
    "  gn refs \"*gtk*\" --files\n"
    "      Find all unique buildfiles with a dependency on a target that has\n"
    "      the substring \"gtk\" in the name.\n";

int RunRefs(const std::vector<std::string>& args) {
  if (args.size() != 1 && args.size() != 2) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn refs <label_pattern>\"").PrintToStdout();
    return 1;
  }

  // Check for common errors on input.
  if (args[0].find('*') == std::string::npos) {
    // We need to begin with a "//" and have a colon if there's no "*" or it
    // will be impossible to match anything.
    if (args[0].size() < 2 ||
        (args[0][0] != '/' && args[0][1] != '/') ||
        args[0].find(':') == std::string::npos) {
      Err(Location(), "Patterns match the entire label. Since your pattern "
          "has no wildcard, it\nshould start with a \"//\" and have a colon "
          "or it can never match anything.\nTo match a substring, use "
          "\"*foo*\".").PrintToStdout();
      return 1;
    }
  }

  Pattern pattern(args[0]);

  Setup* setup = new Setup;
  setup->set_check_for_bad_items(false);
  if (!setup->DoSetup() || !setup->Run())
    return 1;

  const ItemTree& item_tree = setup->build_settings().item_tree();
  base::AutoLock lock(item_tree.lock());

  std::vector<const ItemNode*> nodes;
  item_tree.GetAllItemNodesLocked(&nodes);

  const CommandLine* cmdline = CommandLine::ForCurrentProcess();

  bool file_output = cmdline->HasSwitch("files");
  std::set<std::string> unique_output;

  for (size_t node_index = 0; node_index < nodes.size(); node_index++) {
    const ItemNode& node = *nodes[node_index];
    const ItemNode::ItemNodeMap& deps = node.direct_dependencies();
    for (ItemNode::ItemNodeMap::const_iterator d = deps.begin();
         d != deps.end(); ++d) {
      std::string label = d->first->item()->label().GetUserVisibleName(false);
      if (pattern.MatchesString(label)) {
        // Got a match.
        if (file_output) {
          unique_output.insert(FilePathToUTF8(FilePathForItemNode(node)));
          break;  // Found a match for this target's file, don't need more.
        } else {
          // We can get dupes when there are differnet toolchains involved,
          // so we want to send all output through the de-duper.
          unique_output.insert(
              node.item()->label().GetUserVisibleName(false) + " -> " + label);
        }
      }
    }
  }

  for (std::set<std::string>::iterator i = unique_output.begin();
       i != unique_output.end(); ++i)
    OutputString(*i + "\n");

  return 0;
}

}  // namespace commands
