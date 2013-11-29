#!/usr/bin/env python
# Copyright (c) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import os.path
import generate_protocol_externs
import re
import shutil
import sys
import tempfile

scripts_path = os.path.dirname(os.path.abspath(__file__))
devtools_path = os.path.dirname(scripts_path)
inspector_path = os.path.dirname(devtools_path) + "/core/inspector"
devtools_frontend_path = devtools_path + "/front_end"
protocol_externs_path = devtools_frontend_path + "/protocol_externs.js"
webgl_rendering_context_idl_path = os.path.dirname(devtools_path) + "/core/html/canvas/WebGLRenderingContext.idl"

generate_protocol_externs.generate_protocol_externs(protocol_externs_path, devtools_path + "/protocol.json")

jsmodule_name_prefix = "jsmodule_"
modules = [
    {
        "name": "common",
        "dependencies": [],
        "sources": [
            "Color.js",
            "DOMExtension.js",
            "Object.js",
            "ParsedURL.js",
            "Progress.js",
            "Settings.js",
            "UIString.js",
            "UserMetrics.js",
            "utilities.js",
        ]
    },
    {
        "name": "sdk",
        "dependencies": ["common"],
        "sources": [
            "ApplicationCacheModel.js",
            "CompilerScriptMapping.js",
            "ConsoleModel.js",
            "ContentProvider.js",
            "ContentProviderBasedProjectDelegate.js",
            "ContentProviders.js",
            "CookieParser.js",
            "CSSMetadata.js",
            "CSSStyleModel.js",
            "CSSStyleSheetMapping.js",
            "BreakpointManager.js",
            "Database.js",
            "DOMAgent.js",
            "DOMStorage.js",
            "DebuggerModel.js",
            "DebuggerScriptMapping.js",
            "FileManager.js",
            "FileSystemMapping.js",
            "FileSystemModel.js",
            "FileSystemProjectDelegate.js",
            "FileUtils.js",
            "HAREntry.js",
            "IndexedDBModel.js",
            "InspectorBackend.js",
            "IsolatedFileSystemManager.js",
            "IsolatedFileSystem.js",
            "Linkifier.js",
            "NetworkLog.js",
            "NetworkUISourceCodeProvider.js",
            "OverridesSupport.js",
            "PresentationConsoleMessageHelper.js",
            "RuntimeModel.js",
            "SASSSourceMapping.js",
            "Script.js",
            "ScriptFormatter.js",
            "ScriptSnippetModel.js",
            "SimpleWorkspaceProvider.js",
            "SnippetStorage.js",
            "SourceMapping.js",
            "StylesSourceMapping.js",
            "TimelineManager.js",
            "RemoteObject.js",
            "Resource.js",
            "DefaultScriptMapping.js",
            "ResourceScriptMapping.js",
            "LiveEditSupport.js",
            "ResourceTreeModel.js",
            "ResourceType.js",
            "ResourceUtils.js",
            "SourceMap.js",
            "TracingAgent.js",
            "NetworkManager.js",
            "NetworkRequest.js",
            "UISourceCode.js",
            "Workspace.js",
            "protocol_externs.js",
        ]
    },
    {
        "name": "ui",
        "dependencies": ["common"],
        "sources": [
            "Checkbox.js",
            "CodeMirrorTextEditor.js",
            "ContextMenu.js",
            "CompletionDictionary.js",
            "DOMSyntaxHighlighter.js",
            "DataGrid.js",
            "Dialog.js",
            "DockController.js",
            "Drawer.js",
            "EmptyView.js",
            "GoToLineDialog.js",
            "HelpScreen.js",
            "InspectorView.js",
            "KeyboardShortcut.js",
            "OverviewGrid.js",
            "Panel.js",
            "Placard.js",
            "Popover.js",
            "ProgressIndicator.js",
            "PropertiesSection.js",
            "SearchController.js",
            "Section.js",
            "SidebarPane.js",
            "SidebarTreeElement.js",
            "ShortcutsScreen.js",
            "ShowMoreDataGridNode.js",
            "SidebarOverlay.js",
            "SoftContextMenu.js",
            "SourceTokenizer.js",
            "Spectrum.js",
            "SplitView.js",
            "SidebarView.js",
            "StatusBarButton.js",
            "SuggestBox.js",
            "TabbedPane.js",
            "TextEditor.js",
            "TextEditorHighlighter.js",
            "TextRange.js",
            "TextEditorModel.js",
            "TextPrompt.js",
            "TextUtils.js",
            "TimelineGrid.js",
            "Toolbar.js",
            "UIUtils.js",
            "View.js",
            "ViewportControl.js",
            "treeoutline.js",
        ]
    },
    {
        "name": "components",
        "dependencies": ["sdk", "ui"],
        "sources": [
            "AdvancedSearchController.js",
            "HandlerRegistry.js",
            "ConsoleMessage.js",
            "CookiesTable.js",
            "DOMBreakpointsSidebarPane.js",
            "DOMPresentationUtils.js",
            "ElementsTreeOutline.js",
            "FontView.js",
            "ImageView.js",
            "NativeBreakpointsSidebarPane.js",
            "InspectElementModeController.js",
            "ObjectPopoverHelper.js",
            "ObjectPropertiesSection.js",
            "SourceFrame.js",
            "ResourceView.js",
        ]
    },
    {
        "name": "elements",
        "dependencies": ["components"],
        "sources": [
            "CSSNamedFlowCollectionsView.js",
            "CSSNamedFlowView.js",
            "ElementsPanel.js",
            "ElementsPanelDescriptor.js",
            "EventListenersSidebarPane.js",
            "MetricsSidebarPane.js",
            "PropertiesSidebarPane.js",
            "StylesSidebarPane.js",
        ]
    },
    {
        "name": "network",
        "dependencies": ["components"],
        "sources": [
            "NetworkItemView.js",
            "RequestCookiesView.js",
            "RequestHeadersView.js",
            "RequestHTMLView.js",
            "RequestJSONView.js",
            "RequestPreviewView.js",
            "RequestResponseView.js",
            "RequestTimingView.js",
            "RequestView.js",
            "ResourceWebSocketFrameView.js",
            "NetworkPanel.js",
            "NetworkPanelDescriptor.js",
        ]
    },
    {
        "name": "resources",
        "dependencies": ["components"],
        "sources": [
            "ApplicationCacheItemsView.js",
            "CookieItemsView.js",
            "DatabaseQueryView.js",
            "DatabaseTableView.js",
            "DirectoryContentView.js",
            "DOMStorageItemsView.js",
            "FileContentView.js",
            "FileSystemView.js",
            "IndexedDBViews.js",
            "ResourcesPanel.js",
        ]
    },
    {
        "name": "workers",
        "dependencies": ["components"],
        "sources": [
            "WorkerManager.js",
        ]
    },
    {
        "name": "scripts",
        "dependencies": ["components", "workers"],
        "sources": [
            "BreakpointsSidebarPane.js",
            "CallStackSidebarPane.js",
            "FilePathScoreFunction.js",
            "FilteredItemSelectionDialog.js",
            "JavaScriptSourceFrame.js",
            "NavigatorOverlayController.js",
            "NavigatorView.js",
            "RevisionHistoryView.js",
            "ScopeChainSidebarPane.js",
            "ScriptsNavigator.js",
            "ScriptsPanel.js",
            "ScriptsPanelDescriptor.js",
            "ScriptsSearchScope.js",
            "StyleSheetOutlineDialog.js",
            "TabbedEditorContainer.js",
            "UISourceCodeFrame.js",
            "WatchExpressionsSidebarPane.js",
            "WorkersSidebarPane.js",
        ]
    },
    {
        "name": "console",
        "dependencies": ["components"],
        "sources": [
            "ConsoleView.js",
            "ConsolePanel.js",
        ]
    },
    {
        "name": "timeline",
        "dependencies": ["components"],
        "sources": [
            "DOMCountersGraph.js",
            "MemoryStatistics.js",
            "TimelineModel.js",
            "TimelineOverviewPane.js",
            "TimelinePanel.js",
            "TimelinePanelDescriptor.js",
            "TimelinePresentationModel.js",
            "TimelineFrameController.js"
        ]
    },
    {
        "name": "audits",
        "dependencies": ["components"],
        "sources": [
            "AuditCategories.js",
            "AuditController.js",
            "AuditFormatters.js",
            "AuditLauncherView.js",
            "AuditResultView.js",
            "AuditRules.js",
            "AuditsPanel.js",
        ]
    },
    {
        "name": "extensions",
        "dependencies": ["components"],
        "sources": [
            "ExtensionAPI.js",
            "ExtensionAuditCategory.js",
            "ExtensionPanel.js",
            "ExtensionRegistryStub.js",
            "ExtensionServer.js",
            "ExtensionView.js",
        ]
    },
    {
        "name": "settings",
        "dependencies": ["components", "extensions"],
        "sources": [
            "SettingsScreen.js",
            "OverridesView.js",
        ]
    },
    {
        "name": "tests",
        "dependencies": ["components"],
        "sources": [
            "TestController.js",
        ]
    },
    {
        "name": "profiler",
        "dependencies": ["components", "workers"],
        "sources": [
            "BottomUpProfileDataGridTree.js",
            "CPUProfileView.js",
            "FlameChart.js",
            "HeapSnapshot.js",
            "HeapSnapshotDataGrids.js",
            "HeapSnapshotGridNodes.js",
            "HeapSnapshotLoader.js",
            "HeapSnapshotProxy.js",
            "HeapSnapshotView.js",
            "HeapSnapshotWorker.js",
            "HeapSnapshotWorkerDispatcher.js",
            "JSHeapSnapshot.js",
            "ProfileDataGridTree.js",
            "ProfilesPanel.js",
            "ProfilesPanelDescriptor.js",
            "ProfileLauncherView.js",
            "TopDownProfileDataGridTree.js",
            "CanvasProfileView.js",
        ]
    },
    {
        "name": "host_stub",
        "dependencies": ["components", "profiler", "timeline"],
        "sources": [
            "InspectorFrontendAPI.js",
            "InspectorFrontendHostStub.js",
        ]
    }
]

if os.system("which java") != 0:
    print "Cannot find java ('which java' returns non-zero error code)"
    sys.exit(1)

modules_by_name = {}
for module in modules:
    modules_by_name[module["name"]] = module


def dump_module(name, recursively, processed_modules):
    if name in processed_modules:
        return ""
    processed_modules[name] = True
    module = modules_by_name[name]
    command = ""
    if recursively:
        for dependency in module["dependencies"]:
            command += dump_module(dependency, recursively, processed_modules)
    command += " \\\n    --module " + jsmodule_name_prefix + module["name"] + ":"
    command += str(len(module["sources"]))
    firstDependency = True
    for dependency in module["dependencies"]:
        if firstDependency:
            command += ":"
        else:
            command += ","
        firstDependency = False
        command += jsmodule_name_prefix + dependency
    for script in module["sources"]:
        command += " \\\n        --js " + devtools_frontend_path + "/" + script
    return command

modules_dir = tempfile.mkdtemp()
compiler_command = "java -jar %s/closure/compiler.jar --summary_detail_level 3 --compilation_level SIMPLE_OPTIMIZATIONS \
    --warning_level VERBOSE --jscomp_off=es5Strict --language_in ECMASCRIPT5 --accept_const_keyword --module_output_path_prefix %s/ \\\n" % (scripts_path, modules_dir)

process_recursively = len(sys.argv) > 1
if process_recursively:
    module_name = sys.argv[1]
    if module_name != "all":
        modules = []
        for i in range(1, len(sys.argv)):
            modules.append(modules_by_name[sys.argv[i]])
    for module in modules:
        command = compiler_command
        command += "    --externs " + devtools_frontend_path + "/externs.js"
        command += dump_module(module["name"], True, {})
        print "Compiling \"" + module["name"] + "\""
        os.system(command)
else:
    command = compiler_command
    command += "    --externs " + devtools_frontend_path + "/externs.js"
    for module in modules:
        command += dump_module(module["name"], False, {})
    print "Compiling front_end..."
    os.system(command)

if not process_recursively:

    def unclosure_injected_script(sourceFileName, outFileName):
        sourceFile = open(sourceFileName, "r")
        source = sourceFile.read()
        sourceFile.close()

        def replace_function(matchobj):
            return re.sub(r"@param", "param", matchobj.group(1) or "") + "\n//" + matchobj.group(2)

        # Comment out the closure function and its jsdocs
        source = re.sub(r"(/\*\*(?:[\s\n]*\*\s*@param[^\n]+\n)+\s*\*/\s*)?\n(\(function)", replace_function, source, count=1)

        # Comment out its return statement
        source = re.sub(r"\n(\s*return\s+[^;]+;\s*\n\}\)\s*)$", "\n/*\\1*/", source)

        outFileName = open(outFileName, "w")
        outFileName.write(source)
        outFileName.close()

    injectedScriptSourceTmpFile = inspector_path + "/" + "InjectedScriptSourceTmp.js"
    injectedScriptCanvasModuleSourceTmpFile = inspector_path + "/" + "InjectedScriptCanvasModuleSourceTmp.js"

    unclosure_injected_script(inspector_path + "/" + "InjectedScriptSource.js", injectedScriptSourceTmpFile)
    unclosure_injected_script(inspector_path + "/" + "InjectedScriptCanvasModuleSource.js", injectedScriptCanvasModuleSourceTmpFile)

    print "Compiling InjectedScriptSource.js and InjectedScriptCanvasModuleSource.js..."
    command = compiler_command
    command += "    --externs " + inspector_path + "/" + "InjectedScriptExterns.js" + " \\\n"
    command += "    --externs " + protocol_externs_path + " \\\n"
    command += "    --module " + jsmodule_name_prefix + "injected_script" + ":1" + " \\\n"
    command += "        --js " + injectedScriptSourceTmpFile + " \\\n"
    command += "    --module " + jsmodule_name_prefix + "injected_canvas_script" + ":1:" + jsmodule_name_prefix + "injected_script" + " \\\n"
    command += "        --js " + injectedScriptCanvasModuleSourceTmpFile + " \\\n"
    command += "\n"
    os.system(command)
    os.system("rm " + injectedScriptSourceTmpFile)
    os.system("rm " + injectedScriptCanvasModuleSourceTmpFile)

    print "Checking generated code in InjectedScriptCanvasModuleSource.js..."
    check_injected_webgl_calls_command = "%s/check_injected_webgl_calls_info.py %s %s/InjectedScriptCanvasModuleSource.js" % (scripts_path, webgl_rendering_context_idl_path, inspector_path)
    os.system(check_injected_webgl_calls_command)

shutil.rmtree(modules_dir)
os.system("rm " + protocol_externs_path)
