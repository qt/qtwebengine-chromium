/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @extends {WebInspector.View}
 * @param {!WebInspector.CPUProfileHeader} profileHeader
 */
WebInspector.CPUProfileView = function(profileHeader)
{
    WebInspector.View.call(this);

    this.element.classList.add("profile-view");
    
    this.showSelfTimeAsPercent = WebInspector.settings.createSetting("cpuProfilerShowSelfTimeAsPercent", true);
    this.showTotalTimeAsPercent = WebInspector.settings.createSetting("cpuProfilerShowTotalTimeAsPercent", true);
    this.showAverageTimeAsPercent = WebInspector.settings.createSetting("cpuProfilerShowAverageTimeAsPercent", true);
    this._viewType = WebInspector.settings.createSetting("cpuProfilerView", WebInspector.CPUProfileView._TypeHeavy);

    var columns = [];
    columns.push({id: "self", title: WebInspector.UIString("Self"), width: "72px", sort: WebInspector.DataGrid.Order.Descending, sortable: true});
    columns.push({id: "total", title: WebInspector.UIString("Total"), width: "72px", sortable: true});
    columns.push({id: "function", title: WebInspector.UIString("Function"), disclosure: true, sortable: true});

    this.dataGrid = new WebInspector.DataGrid(columns);
    this.dataGrid.addEventListener(WebInspector.DataGrid.Events.SortingChanged, this._sortProfile, this);
    this.dataGrid.element.addEventListener("mousedown", this._mouseDownInDataGrid.bind(this), true);
    this.dataGrid.show(this.element);

    this.viewSelectComboBox = new WebInspector.StatusBarComboBox(this._changeView.bind(this));

    var options = {};
    options[WebInspector.CPUProfileView._TypeFlame] = this.viewSelectComboBox.createOption(WebInspector.UIString("Flame Chart"), "", WebInspector.CPUProfileView._TypeFlame);
    options[WebInspector.CPUProfileView._TypeHeavy] = this.viewSelectComboBox.createOption(WebInspector.UIString("Heavy (Bottom Up)"), "", WebInspector.CPUProfileView._TypeHeavy);
    options[WebInspector.CPUProfileView._TypeTree] = this.viewSelectComboBox.createOption(WebInspector.UIString("Tree (Top Down)"), "", WebInspector.CPUProfileView._TypeTree);

    var optionName = this._viewType.get() || WebInspector.CPUProfileView._TypeFlame;
    var option = options[optionName] || options[WebInspector.CPUProfileView._TypeFlame];
    this.viewSelectComboBox.select(option);

    this._statusBarButtonsElement = document.createElement("span");

    this.percentButton = new WebInspector.StatusBarButton("", "percent-time-status-bar-item");
    this.percentButton.addEventListener("click", this._percentClicked, this);
    this._statusBarButtonsElement.appendChild(this.percentButton.element);

    this.focusButton = new WebInspector.StatusBarButton(WebInspector.UIString("Focus selected function."), "focus-profile-node-status-bar-item");
    this.focusButton.setEnabled(false);
    this.focusButton.addEventListener("click", this._focusClicked, this);
    this._statusBarButtonsElement.appendChild(this.focusButton.element);

    this.excludeButton = new WebInspector.StatusBarButton(WebInspector.UIString("Exclude selected function."), "exclude-profile-node-status-bar-item");
    this.excludeButton.setEnabled(false);
    this.excludeButton.addEventListener("click", this._excludeClicked, this);
    this._statusBarButtonsElement.appendChild(this.excludeButton.element);

    this.resetButton = new WebInspector.StatusBarButton(WebInspector.UIString("Restore all functions."), "reset-profile-status-bar-item");
    this.resetButton.visible = false;
    this.resetButton.addEventListener("click", this._resetClicked, this);
    this._statusBarButtonsElement.appendChild(this.resetButton.element);

    this.profileHead = /** @type {?ProfilerAgent.CPUProfileNode} */ (null);
    this.profile = profileHeader;

    this._linkifier = new WebInspector.Linkifier(new WebInspector.Linkifier.DefaultFormatter(30));

    if (this.profile._profile) // If the profile has been loaded from file then use it.
        this._processProfileData(this.profile._profile);
    else
        this._processProfileData(this.profile.protocolProfile());
}

WebInspector.CPUProfileView._TypeFlame = "Flame";
WebInspector.CPUProfileView._TypeTree = "Tree";
WebInspector.CPUProfileView._TypeHeavy = "Heavy";

WebInspector.CPUProfileView.prototype = {
    /**
     * @param {!number} timeLeft
     * @param {!number} timeRight
     */
    selectRange: function(timeLeft, timeRight)
    {
        if (!this._flameChart)
            return;
        this._flameChart.selectRange(timeLeft, timeRight);
    },

    _revealProfilerNode: function(event)
    {
        var current = this.profileDataGridTree.children[0];

        while (current && current.profileNode !== event.data)
            current = current.traverseNextNode(false, null, false);

        if (current)
            current.revealAndSelect();
    },

    /**
     * @param {?ProfilerAgent.CPUProfile} profile
     */
    _processProfileData: function(profile)
    {
        this.profileHead = profile.head;
        this.samples = profile.samples;

        this._calculateTimes(profile);

        this._assignParentsInProfile();
        if (this.samples)
            this._buildIdToNodeMap();
        this._changeView();
        this._updatePercentButton();
        if (this._flameChart)
            this._flameChart.update();
    },

    get statusBarItems()
    {
        return [this.viewSelectComboBox.element, this._statusBarButtonsElement];
    },

    /**
     * @return {!WebInspector.ProfileDataGridTree}
     */
    _getBottomUpProfileDataGridTree: function()
    {
        if (!this._bottomUpProfileDataGridTree)
            this._bottomUpProfileDataGridTree = new WebInspector.BottomUpProfileDataGridTree(this, /** @type {!ProfilerAgent.CPUProfileNode} */ (this.profileHead));
        return this._bottomUpProfileDataGridTree;
    },

    /**
     * @return {!WebInspector.ProfileDataGridTree}
     */
    _getTopDownProfileDataGridTree: function()
    {
        if (!this._topDownProfileDataGridTree)
            this._topDownProfileDataGridTree = new WebInspector.TopDownProfileDataGridTree(this, /** @type {!ProfilerAgent.CPUProfileNode} */ (this.profileHead));
        return this._topDownProfileDataGridTree;
    },

    willHide: function()
    {
        this._currentSearchResultIndex = -1;
    },

    refresh: function()
    {
        var selectedProfileNode = this.dataGrid.selectedNode ? this.dataGrid.selectedNode.profileNode : null;

        this.dataGrid.rootNode().removeChildren();

        var children = this.profileDataGridTree.children;
        var count = children.length;

        for (var index = 0; index < count; ++index)
            this.dataGrid.rootNode().appendChild(children[index]);

        if (selectedProfileNode)
            selectedProfileNode.selected = true;
    },

    refreshVisibleData: function()
    {
        var child = this.dataGrid.rootNode().children[0];
        while (child) {
            child.refresh();
            child = child.traverseNextNode(false, null, true);
        }
    },

    refreshShowAsPercents: function()
    {
        this._updatePercentButton();
        this.refreshVisibleData();
    },

    searchCanceled: function()
    {
        if (this._searchResults) {
            for (var i = 0; i < this._searchResults.length; ++i) {
                var profileNode = this._searchResults[i].profileNode;

                delete profileNode._searchMatchedSelfColumn;
                delete profileNode._searchMatchedTotalColumn;
                delete profileNode._searchMatchedFunctionColumn;

                profileNode.refresh();
            }
        }

        delete this._searchFinishedCallback;
        this._currentSearchResultIndex = -1;
        this._searchResults = [];
    },

    performSearch: function(query, finishedCallback)
    {
        // Call searchCanceled since it will reset everything we need before doing a new search.
        this.searchCanceled();

        query = query.trim();

        if (!query.length)
            return;

        this._searchFinishedCallback = finishedCallback;

        var greaterThan = (query.startsWith(">"));
        var lessThan = (query.startsWith("<"));
        var equalTo = (query.startsWith("=") || ((greaterThan || lessThan) && query.indexOf("=") === 1));
        var percentUnits = (query.lastIndexOf("%") === (query.length - 1));
        var millisecondsUnits = (query.length > 2 && query.lastIndexOf("ms") === (query.length - 2));
        var secondsUnits = (!millisecondsUnits && query.lastIndexOf("s") === (query.length - 1));

        var queryNumber = parseFloat(query);
        if (greaterThan || lessThan || equalTo) {
            if (equalTo && (greaterThan || lessThan))
                queryNumber = parseFloat(query.substring(2));
            else
                queryNumber = parseFloat(query.substring(1));
        }

        var queryNumberMilliseconds = (secondsUnits ? (queryNumber * 1000) : queryNumber);

        // Make equalTo implicitly true if it wasn't specified there is no other operator.
        if (!isNaN(queryNumber) && !(greaterThan || lessThan))
            equalTo = true;

        var matcher = createPlainTextSearchRegex(query, "i");

        function matchesQuery(/*ProfileDataGridNode*/ profileDataGridNode)
        {
            delete profileDataGridNode._searchMatchedSelfColumn;
            delete profileDataGridNode._searchMatchedTotalColumn;
            delete profileDataGridNode._searchMatchedFunctionColumn;

            if (percentUnits) {
                if (lessThan) {
                    if (profileDataGridNode.selfPercent < queryNumber)
                        profileDataGridNode._searchMatchedSelfColumn = true;
                    if (profileDataGridNode.totalPercent < queryNumber)
                        profileDataGridNode._searchMatchedTotalColumn = true;
                } else if (greaterThan) {
                    if (profileDataGridNode.selfPercent > queryNumber)
                        profileDataGridNode._searchMatchedSelfColumn = true;
                    if (profileDataGridNode.totalPercent > queryNumber)
                        profileDataGridNode._searchMatchedTotalColumn = true;
                }

                if (equalTo) {
                    if (profileDataGridNode.selfPercent == queryNumber)
                        profileDataGridNode._searchMatchedSelfColumn = true;
                    if (profileDataGridNode.totalPercent == queryNumber)
                        profileDataGridNode._searchMatchedTotalColumn = true;
                }
            } else if (millisecondsUnits || secondsUnits) {
                if (lessThan) {
                    if (profileDataGridNode.selfTime < queryNumberMilliseconds)
                        profileDataGridNode._searchMatchedSelfColumn = true;
                    if (profileDataGridNode.totalTime < queryNumberMilliseconds)
                        profileDataGridNode._searchMatchedTotalColumn = true;
                } else if (greaterThan) {
                    if (profileDataGridNode.selfTime > queryNumberMilliseconds)
                        profileDataGridNode._searchMatchedSelfColumn = true;
                    if (profileDataGridNode.totalTime > queryNumberMilliseconds)
                        profileDataGridNode._searchMatchedTotalColumn = true;
                }

                if (equalTo) {
                    if (profileDataGridNode.selfTime == queryNumberMilliseconds)
                        profileDataGridNode._searchMatchedSelfColumn = true;
                    if (profileDataGridNode.totalTime == queryNumberMilliseconds)
                        profileDataGridNode._searchMatchedTotalColumn = true;
                }
            }

            if (profileDataGridNode.functionName.match(matcher) || (profileDataGridNode.url && profileDataGridNode.url.match(matcher)))
                profileDataGridNode._searchMatchedFunctionColumn = true;

            if (profileDataGridNode._searchMatchedSelfColumn ||
                profileDataGridNode._searchMatchedTotalColumn ||
                profileDataGridNode._searchMatchedFunctionColumn)
            {
                profileDataGridNode.refresh();
                return true;
            }

            return false;
        }

        var current = this.profileDataGridTree.children[0];

        while (current) {
            if (matchesQuery(current)) {
                this._searchResults.push({ profileNode: current });
            }

            current = current.traverseNextNode(false, null, false);
        }

        finishedCallback(this, this._searchResults.length);
    },

    jumpToFirstSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        this._currentSearchResultIndex = 0;
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    jumpToLastSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        this._currentSearchResultIndex = (this._searchResults.length - 1);
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    jumpToNextSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        if (++this._currentSearchResultIndex >= this._searchResults.length)
            this._currentSearchResultIndex = 0;
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    jumpToPreviousSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        if (--this._currentSearchResultIndex < 0)
            this._currentSearchResultIndex = (this._searchResults.length - 1);
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    showingFirstSearchResult: function()
    {
        return (this._currentSearchResultIndex === 0);
    },

    showingLastSearchResult: function()
    {
        return (this._searchResults && this._currentSearchResultIndex === (this._searchResults.length - 1));
    },

    currentSearchResultIndex: function() {
        return this._currentSearchResultIndex;
    },

    _jumpToSearchResult: function(index)
    {
        var searchResult = this._searchResults[index];
        if (!searchResult)
            return;

        var profileNode = searchResult.profileNode;
        profileNode.revealAndSelect();
    },

    _ensureFlameChartCreated: function()
    {
        if (this._flameChart)
            return;
        var dataProvider = new WebInspector.CPUFlameChartDataProvider(this);
        this._flameChart = new WebInspector.FlameChart(dataProvider);
        this._flameChart.addEventListener(WebInspector.FlameChart.Events.EntrySelected, this._onEntrySelected.bind(this));
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onEntrySelected: function(event)
    {
        var node = event.data;
        if (!node || !node.scriptId)
            return;
        var script = WebInspector.debuggerModel.scriptForId(node.scriptId)
        if (!script)
            return;
        var uiLocation = script.rawLocationToUILocation(node.lineNumber);
        if (!uiLocation)
            return;
        WebInspector.panel("sources").showUILocation(uiLocation);
    },

    _changeView: function()
    {
        if (!this.profile)
            return;

        switch (this.viewSelectComboBox.selectedOption().value) {
        case WebInspector.CPUProfileView._TypeFlame:
            this._ensureFlameChartCreated();
            this.dataGrid.detach();
            this._flameChart.show(this.element);
            this._viewType.set(WebInspector.CPUProfileView._TypeFlame);
            this._statusBarButtonsElement.enableStyleClass("hidden", true);
            return;
        case WebInspector.CPUProfileView._TypeTree:
            this.profileDataGridTree = this._getTopDownProfileDataGridTree();
            this._sortProfile();
            this._viewType.set(WebInspector.CPUProfileView._TypeTree);
            break;
        case WebInspector.CPUProfileView._TypeHeavy:
            this.profileDataGridTree = this._getBottomUpProfileDataGridTree();
            this._sortProfile();
            this._viewType.set(WebInspector.CPUProfileView._TypeHeavy);
            break;
        }

        this._statusBarButtonsElement.enableStyleClass("hidden", false);

        if (this._flameChart)
            this._flameChart.detach();
        this.dataGrid.show(this.element);

        if (!this.currentQuery || !this._searchFinishedCallback || !this._searchResults)
            return;

        // The current search needs to be performed again. First negate out previous match
        // count by calling the search finished callback with a negative number of matches.
        // Then perform the search again the with same query and callback.
        this._searchFinishedCallback(this, -this._searchResults.length);
        this.performSearch(this.currentQuery, this._searchFinishedCallback);
    },

    _percentClicked: function(event)
    {
        var currentState = this.showSelfTimeAsPercent.get() && this.showTotalTimeAsPercent.get() && this.showAverageTimeAsPercent.get();
        this.showSelfTimeAsPercent.set(!currentState);
        this.showTotalTimeAsPercent.set(!currentState);
        this.showAverageTimeAsPercent.set(!currentState);
        this.refreshShowAsPercents();
    },

    _updatePercentButton: function()
    {
        if (this.showSelfTimeAsPercent.get() && this.showTotalTimeAsPercent.get() && this.showAverageTimeAsPercent.get()) {
            this.percentButton.title = WebInspector.UIString("Show absolute total and self times.");
            this.percentButton.toggled = true;
        } else {
            this.percentButton.title = WebInspector.UIString("Show total and self times as percentages.");
            this.percentButton.toggled = false;
        }
    },

    _focusClicked: function(event)
    {
        if (!this.dataGrid.selectedNode)
            return;

        this.resetButton.visible = true;
        this.profileDataGridTree.focus(this.dataGrid.selectedNode);
        this.refresh();
        this.refreshVisibleData();
    },

    _excludeClicked: function(event)
    {
        var selectedNode = this.dataGrid.selectedNode

        if (!selectedNode)
            return;

        selectedNode.deselect();

        this.resetButton.visible = true;
        this.profileDataGridTree.exclude(selectedNode);
        this.refresh();
        this.refreshVisibleData();
    },

    _resetClicked: function(event)
    {
        this.resetButton.visible = false;
        this.profileDataGridTree.restore();
        this._linkifier.reset();
        this.refresh();
        this.refreshVisibleData();
    },

    _dataGridNodeSelected: function(node)
    {
        this.focusButton.setEnabled(true);
        this.excludeButton.setEnabled(true);
    },

    _dataGridNodeDeselected: function(node)
    {
        this.focusButton.setEnabled(false);
        this.excludeButton.setEnabled(false);
    },

    _sortProfile: function()
    {
        var sortAscending = this.dataGrid.isSortOrderAscending();
        var sortColumnIdentifier = this.dataGrid.sortColumnIdentifier();
        var sortProperty = {
                "self": "selfTime",
                "total": "totalTime",
                "function": "functionName"
            }[sortColumnIdentifier];

        this.profileDataGridTree.sort(WebInspector.ProfileDataGridTree.propertyComparator(sortProperty, sortAscending));

        this.refresh();
    },

    _mouseDownInDataGrid: function(event)
    {
        if (event.detail < 2)
            return;

        var cell = event.target.enclosingNodeOrSelfWithNodeName("td");
        if (!cell || (!cell.classList.contains("total-column") && !cell.classList.contains("self-column") && !cell.classList.contains("average-column")))
            return;

        if (cell.classList.contains("total-column"))
            this.showTotalTimeAsPercent.set(!this.showTotalTimeAsPercent.get());
        else if (cell.classList.contains("self-column"))
            this.showSelfTimeAsPercent.set(!this.showSelfTimeAsPercent.get());
        else if (cell.classList.contains("average-column"))
            this.showAverageTimeAsPercent.set(!this.showAverageTimeAsPercent.get());

        this.refreshShowAsPercents();

        event.consume(true);
    },

    _calculateTimes: function(profile)
    {
        function totalHitCount(node) {
            var result = node.hitCount;
            for (var i = 0; i < node.children.length; i++)
                result += totalHitCount(node.children[i]);
            return result;
        }
        profile.totalHitCount = totalHitCount(profile.head);

        var durationMs = 1000 * (profile.endTime - profile.startTime);
        var samplingInterval = durationMs / profile.totalHitCount;
        this.samplingIntervalMs = samplingInterval;

        function calculateTimesForNode(node) {
            node.selfTime = node.hitCount * samplingInterval;
            var totalHitCount = node.hitCount;
            for (var i = 0; i < node.children.length; i++)
                totalHitCount += calculateTimesForNode(node.children[i]);
            node.totalTime = totalHitCount * samplingInterval;
            return totalHitCount;
        }
        calculateTimesForNode(profile.head);
    },

    _assignParentsInProfile: function()
    {
        var head = this.profileHead;
        head.parent = null;
        head.head = null;
        var nodesToTraverse = [ { parent: head, children: head.children } ];
        while (nodesToTraverse.length > 0) {
            var pair = nodesToTraverse.pop();
            var parent = pair.parent;
            var children = pair.children;
            var length = children.length;
            for (var i = 0; i < length; ++i) {
                children[i].head = head;
                children[i].parent = parent;
                if (children[i].children.length > 0)
                    nodesToTraverse.push({ parent: children[i], children: children[i].children });
            }
        }
    },

    _buildIdToNodeMap: function()
    {
        var idToNode = this._idToNode = {};
        var stack = [this.profileHead];
        while (stack.length) {
            var node = stack.pop();
            idToNode[node.id] = node;
            for (var i = 0; i < node.children.length; i++)
                stack.push(node.children[i]);
        }

        var topLevelNodes = this.profileHead.children;
        for (var i = 0; i < topLevelNodes.length; i++) {
            var node = topLevelNodes[i];
            if (node.functionName == "(garbage collector)") {
                this._gcNode = node;
                break;
            }
        }
    },

    __proto__: WebInspector.View.prototype
}

/**
 * @constructor
 * @extends {WebInspector.ProfileType}
 * @implements {WebInspector.CPUProfilerModelDelegate}
 */
WebInspector.CPUProfileType = function()
{
    WebInspector.ProfileType.call(this, WebInspector.CPUProfileType.TypeId, WebInspector.UIString("Collect JavaScript CPU Profile"));
    this._recording = false;
    this._nextProfileId = 1;

    this._nextAnonymousConsoleProfileNumber = 1;
    this._anonymousConsoleProfileIdToTitle = {};

    WebInspector.CPUProfileType.instance = this;
    WebInspector.cpuProfilerModel.setDelegate(this);
}

WebInspector.CPUProfileType.TypeId = "CPU";

WebInspector.CPUProfileType.prototype = {
    /**
     * @override
     * @return {string}
     */
    fileExtension: function()
    {
        return ".cpuprofile";
    },

    get buttonTooltip()
    {
        return this._recording ? WebInspector.UIString("Stop CPU profiling.") : WebInspector.UIString("Start CPU profiling.");
    },

    /**
     * @override
     * @return {boolean}
     */
    buttonClicked: function()
    {
        if (this._recording) {
            this.stopRecordingProfile();
            return false;
        } else {
            this.startRecordingProfile();
            return true;
        }
    },

    get treeItemTitle()
    {
        return WebInspector.UIString("CPU PROFILES");
    },

    get description()
    {
        return WebInspector.UIString("CPU profiles show where the execution time is spent in your page's JavaScript functions.");
    },

    /**
     * @param {string} id
     * @param {!DebuggerAgent.Location} scriptLocation
     * @param {string=} title
     */
    consoleProfile: function(id, scriptLocation, title)
    {
        var resolvedTitle = title;
        if (!resolvedTitle) {
            resolvedTitle = WebInspector.UIString("Profile %s", this._nextAnonymousConsoleProfileNumber++);
            this._anonymousConsoleProfileIdToTitle[id] = resolvedTitle;
        }
        this._addMessageToConsole(WebInspector.ConsoleMessage.MessageType.Profile, scriptLocation, resolvedTitle);
    },

    /**
     * @param {string} protocolId
     * @param {!DebuggerAgent.Location} scriptLocation
     * @param {!ProfilerAgent.CPUProfile} cpuProfile
     * @param {string=} title
     */
    consoleProfileEnd: function(protocolId, scriptLocation, cpuProfile, title)
    {
        // Make sure ProfilesPanel is initialized and CPUProfileType is created.
        var resolvedTitle = title;
        if (typeof title === "undefined") {
            resolvedTitle = this._anonymousConsoleProfileIdToTitle[protocolId];
            delete this._anonymousConsoleProfileIdToTitle[protocolId];
        }

        var id = this._nextProfileId++;
        var profile = new WebInspector.CPUProfileHeader(this, resolvedTitle, id);
        profile.setProtocolProfile(cpuProfile);
        this.addProfile(profile);

        resolvedTitle += "#" + id;
        this._addMessageToConsole(WebInspector.ConsoleMessage.MessageType.ProfileEnd, scriptLocation, resolvedTitle);
    },

    /**
     * @param {string} type
     * @param {!DebuggerAgent.Location} scriptLocation
     * @param {string} title
     */
    _addMessageToConsole: function(type, scriptLocation, title)
    {
        var rawLocation = new WebInspector.DebuggerModel.Location(scriptLocation.scriptId, scriptLocation.lineNumber, scriptLocation.columnNumber || 0);
        var uiLocation = WebInspector.debuggerModel.rawLocationToUILocation(rawLocation);
        var url;
        if (uiLocation)
            url = uiLocation.url();
        var message = WebInspector.ConsoleMessage.create(
            WebInspector.ConsoleMessage.MessageSource.ConsoleAPI,
            WebInspector.ConsoleMessage.MessageLevel.Debug,
            title,
            type,
            url || undefined,
            scriptLocation.lineNumber,
            scriptLocation.columnNumber);
        WebInspector.console.addMessage(message);
    },

    /**
     * @param {!ProfilerAgent.CPUProfile} cpuProfile
     * @param {string} title
     */
    _addProfileHeader: function(cpuProfile, title)
    {
        var id = this._nextProfileId++;
        var profile = new WebInspector.CPUProfileHeader(this, title, id);
        profile.setProtocolProfile(cpuProfile);
        this.addProfile(profile);
    },

    isRecordingProfile: function()
    {
        return this._recording;
    },

    startRecordingProfile: function()
    {
        if (this._profileBeingRecorded)
            return;
        var id = this._nextProfileId++;
        this._profileBeingRecorded = new WebInspector.CPUProfileHeader(this, WebInspector.UIString("Recording\u2026"), id);
        this.addProfile(this._profileBeingRecorded);

        this._recording = true;
        WebInspector.cpuProfilerModel.setRecording(true);
        WebInspector.userMetrics.ProfilesCPUProfileTaken.record();
        ProfilerAgent.start();
    },

    stopRecordingProfile: function()
    {
        this._recording = false;
        WebInspector.cpuProfilerModel.setRecording(false);

        /**
         * @param {?string} error
         * @param {?ProfilerAgent.CPUProfile} profile
         * @this {WebInspector.CPUProfileType}
         */
        function didStopProfiling(error, profile)
        {
            if (!this._profileBeingRecorded)
                return;
            this._profileBeingRecorded.setProtocolProfile(profile);

            var title = WebInspector.UIString("Profile %d", this._profileBeingRecorded.uid);
            this._profileBeingRecorded.title = title;
            this._profileBeingRecorded.sidebarElement.mainTitle = title;
            var recordedProfile = this._profileBeingRecorded;
            this._profileBeingRecorded = null;
            WebInspector.panels.profiles._showProfile(recordedProfile);
        }
        ProfilerAgent.stop(didStopProfiling.bind(this));
    },

    /**
     * @override
     * @param {string} title
     * @return {!WebInspector.ProfileHeader}
     */
    createProfileLoadedFromFile: function(title)
    {
        return new WebInspector.CPUProfileHeader(this, title);
    },

    /**
     * @override
     */
    removeProfile: function(profile)
    {
        if (this._profileBeingRecorded === profile) {
            this.stopRecordingProfile();
            this._profileBeingRecorded = null;
        }
        WebInspector.ProfileType.prototype.removeProfile.call(this, profile);
    },

    /**
     * @override
     */
    resetProfiles: function()
    {
        this._reset();
    },

    __proto__: WebInspector.ProfileType.prototype
}

/**
 * @constructor
 * @extends {WebInspector.ProfileHeader}
 * @implements {WebInspector.OutputStream}
 * @implements {WebInspector.OutputStreamDelegate}
 * @param {!WebInspector.CPUProfileType} type
 * @param {string} title
 * @param {number=} uid
 */
WebInspector.CPUProfileHeader = function(type, title, uid)
{
    WebInspector.ProfileHeader.call(this, type, title, uid);
    this._tempFile = null;
}

WebInspector.CPUProfileHeader.prototype = {
    onTransferStarted: function()
    {
        this._jsonifiedProfile = "";
        this.sidebarElement.subtitle = WebInspector.UIString("Loading\u2026 %s", Number.bytesToString(this._jsonifiedProfile.length));
    },

    /**
     * @param {!WebInspector.ChunkedReader} reader
     */
    onChunkTransferred: function(reader)
    {
        this.sidebarElement.subtitle = WebInspector.UIString("Loading\u2026 %d\%", Number.bytesToString(this._jsonifiedProfile.length));
    },

    onTransferFinished: function()
    {
        this.sidebarElement.subtitle = WebInspector.UIString("Parsing\u2026");
        this._profile = JSON.parse(this._jsonifiedProfile);
        this._jsonifiedProfile = null;
        this.sidebarElement.subtitle = WebInspector.UIString("Loaded");

        if (this._profileType._profileBeingRecorded === this)
            this._profileType._profileBeingRecorded = null;
    },

    /**
     * @param {!WebInspector.ChunkedReader} reader
     */
    onError: function(reader, e)
    {
        switch(e.target.error.code) {
        case e.target.error.NOT_FOUND_ERR:
            this.sidebarElement.subtitle = WebInspector.UIString("'%s' not found.", reader.fileName());
        break;
        case e.target.error.NOT_READABLE_ERR:
            this.sidebarElement.subtitle = WebInspector.UIString("'%s' is not readable", reader.fileName());
        break;
        case e.target.error.ABORT_ERR:
            break;
        default:
            this.sidebarElement.subtitle = WebInspector.UIString("'%s' error %d", reader.fileName(), e.target.error.code);
        }
    },

    /**
     * @param {string} text
     */
    write: function(text)
    {
        this._jsonifiedProfile += text;
    },

    close: function() { },

    /**
     * @override
     */
    createSidebarTreeElement: function()
    {
        return new WebInspector.ProfileSidebarTreeElement(this, "profile-sidebar-tree-item");
    },

    /**
     * @override
     * @param {!WebInspector.ProfilesPanel} profilesPanel
     */
    createView: function(profilesPanel)
    {
        return new WebInspector.CPUProfileView(this);
    },

    /**
     * @override
     * @return {boolean}
     */
    canSaveToFile: function()
    {
        return !!this._tempFile;
    },

    saveToFile: function()
    {
        var fileOutputStream = new WebInspector.FileOutputStream();

        /**
         * @param {boolean} accepted
         * @this {WebInspector.CPUProfileHeader}
         */
        function onOpenForSave(accepted)
        {
            if (!accepted)
                return;
            function didRead(data)
            {
                if (data)
                    fileOutputStream.write(data, fileOutputStream.close.bind(fileOutputStream));
                else
                    fileOutputStream.close();
            }
            this._tempFile.read(didRead.bind(this));
        }
        this._fileName = this._fileName || "CPU-" + new Date().toISO8601Compact() + this._profileType.fileExtension();
        fileOutputStream.open(this._fileName, onOpenForSave.bind(this));
    },

    /**
     * @param {!File} file
     */
    loadFromFile: function(file)
    {
        this.sidebarElement.subtitle = WebInspector.UIString("Loading\u2026");
        this.sidebarElement.wait = true;

        var fileReader = new WebInspector.ChunkedFileReader(file, 10000000, this);
        fileReader.start(this);
    },


    /**
     * @return {?ProfilerAgent.CPUProfile}
     */
    protocolProfile: function()
    {
        return this._protocolProfile;
    },

    /**
     * @param {!ProfilerAgent.CPUProfile} cpuProfile
     */
    setProtocolProfile: function(cpuProfile)
    {
        this._protocolProfile = cpuProfile;
        this._saveProfileDataToTempFile(cpuProfile);
    },

    /**
     * @param {!ProfilerAgent.CPUProfile} data
     */
    _saveProfileDataToTempFile: function(data)
    {
        var serializedData = JSON.stringify(data);

        /**
         * @this {WebInspector.CPUProfileHeader}
         */
        function didCreateTempFile(tempFile)
        {
            this._writeToTempFile(tempFile, serializedData);
        }
        new WebInspector.TempFile("cpu-profiler", this.uid,  didCreateTempFile.bind(this));
    },

    /**
     * @param {?WebInspector.TempFile} tempFile
     * @param {string} serializedData
     */
    _writeToTempFile: function(tempFile, serializedData)
    {
        this._tempFile = tempFile;
        if (tempFile)
            tempFile.write(serializedData);
    },

    __proto__: WebInspector.ProfileHeader.prototype
}

/**
 * @constructor
 * @implements {WebInspector.FlameChartDataProvider}
 */
WebInspector.CPUFlameChartDataProvider = function(cpuProfileView)
{
    WebInspector.FlameChartDataProvider.call(this);
    this._cpuProfileView = cpuProfileView;
}

WebInspector.CPUFlameChartDataProvider.prototype = {
    /**
     * @param {!WebInspector.FlameChart.ColorGenerator} colorGenerator
     * @return {!Object}
     */
    timelineData: function(colorGenerator)
    {
        return this._timelineData || this._calculateTimelineData(colorGenerator);
    },

    /**
     * @param {!WebInspector.FlameChart.ColorGenerator} colorGenerator
     * @return {?Object}
     */
    _calculateTimelineData: function(colorGenerator)
    {
        if (!this._cpuProfileView.profileHead)
            return null;

        var samples = this._cpuProfileView.samples;
        var idToNode = this._cpuProfileView._idToNode;
        var gcNode = this._cpuProfileView._gcNode;
        var samplesCount = samples.length;
        var samplingInterval = this._cpuProfileView.samplingIntervalMs;

        var index = 0;

        var openIntervals = [];
        var stackTrace = [];
        var colorEntryIndexes = [];
        var maxDepth = 5; // minimum stack depth for the case when we see no activity.
        var depth = 0;

        /**
         * @constructor
         * @param {!Object} colorPair
         * @param {!number} depth
         * @param {!number} duration
         * @param {!number} startTime
         * @param {!Object} node
         */
        function ChartEntry(colorPair, depth, duration, startTime, node)
        {
            this.colorPair = colorPair;
            this.depth = depth;
            this.duration = duration;
            this.startTime = startTime;
            this.node = node;
            this.selfTime = 0;
        }
        var entries = /** @type {!Array.<!ChartEntry>} */ ([]);

        for (var sampleIndex = 0; sampleIndex < samplesCount; sampleIndex++) {
            var node = idToNode[samples[sampleIndex]];
            stackTrace.length = 0;
            while (node) {
                stackTrace.push(node);
                node = node.parent;
            }
            stackTrace.pop(); // Remove (root) node

            maxDepth = Math.max(maxDepth, depth);
            depth = 0;
            node = stackTrace.pop();
            var intervalIndex;

            // GC samples have no stack, so we just put GC node on top of the last recoreded sample.
            if (node === gcNode) {
                while (depth < openIntervals.length) {
                    intervalIndex = openIntervals[depth].index;
                    entries[intervalIndex].duration += samplingInterval;
                    ++depth;
                }
                // If previous stack is also GC then just continue.
                if (openIntervals.length > 0 && openIntervals.peekLast().node === node) {
                    entries[intervalIndex].selfTime += samplingInterval;
                    continue;
                }
            }

            while (node && depth < openIntervals.length && node === openIntervals[depth].node) {
                intervalIndex = openIntervals[depth].index;
                entries[intervalIndex].duration += samplingInterval;
                node = stackTrace.pop();
                ++depth;
            }
            if (depth < openIntervals.length)
                openIntervals.length = depth;
            if (!node) {
                entries[intervalIndex].selfTime += samplingInterval;
                continue;
            }

            while (node) {
                var colorPair = colorGenerator._colorPairForID(node.functionName + ":" + node.url + ":" + node.lineNumber);
                var indexesForColor = colorEntryIndexes[colorPair.index];
                if (!indexesForColor)
                    indexesForColor = colorEntryIndexes[colorPair.index] = [];

                var entry = new ChartEntry(colorPair, depth, samplingInterval, sampleIndex * samplingInterval, node);
                indexesForColor.push(entries.length);
                entries.push(entry);
                openIntervals.push({node: node, index: index});
                ++index;

                node = stackTrace.pop();
                ++depth;
            }
            entries[entries.length - 1].selfTime += samplingInterval;
        }

        var entryNodes = new Array(entries.length);
        var entryColorIndexes = new Uint16Array(entries.length);
        var entryLevels = new Uint8Array(entries.length);
        var entryTotalTimes = new Float32Array(entries.length);
        var entrySelfTimes = new Float32Array(entries.length);
        var entryOffsets = new Float32Array(entries.length);
        var entryTitles = new Array(entries.length);
        var entryDeoptFlags = new Uint8Array(entries.length);

        for (var i = 0; i < entries.length; ++i) {
            var entry = entries[i];
            entryNodes[i] = entry.node;
            entryColorIndexes[i] = colorPair.index;
            entryLevels[i] = entry.depth;
            entryTotalTimes[i] = entry.duration;
            entrySelfTimes[i] = entry.selfTime;
            entryOffsets[i] = entry.startTime;
            entryTitles[i] = entry.node.functionName;
            var reason = entry.node.deoptReason;
            entryDeoptFlags[i] = (reason && reason !== "no reason");
        }

        this._timelineData = {
            maxStackDepth: Math.max(maxDepth, depth),
            totalTime: this._cpuProfileView.profileHead.totalTime,
            entryNodes: entryNodes,
            entryColorIndexes: entryColorIndexes,
            entryLevels: entryLevels,
            entryTotalTimes: entryTotalTimes,
            entrySelfTimes: entrySelfTimes,
            entryOffsets: entryOffsets,
            colorEntryIndexes: colorEntryIndexes,
            entryTitles: entryTitles,
            entryDeoptFlags: entryDeoptFlags
        };

        return this._timelineData;
    },

    /**
     * @param {number} ms
     */
    _millisecondsToString: function(ms)
    {
        if (ms === 0)
            return "0";
        if (ms < 1000)
            return WebInspector.UIString("%.1f\u2009ms", ms);
        return Number.secondsToString(ms / 1000, true);
    },

    /**
     * @param {number} entryIndex
     */
    prepareHighlightedEntryInfo: function(entryIndex)
    {
        var timelineData = this._timelineData;
        var node = timelineData.entryNodes[entryIndex];
        if (!node)
            return null;

        var entryInfo = [];
        function pushEntryInfoRow(title, text)
        {
            var row = {};
            row.title = title;
            row.text = text;
            entryInfo.push(row);
        }

        pushEntryInfoRow(WebInspector.UIString("Name"), timelineData.entryTitles[entryIndex]);
        var selfTime = this._millisecondsToString(timelineData.entrySelfTimes[entryIndex]);
        var totalTime = this._millisecondsToString(timelineData.entryTotalTimes[entryIndex]);
        pushEntryInfoRow(WebInspector.UIString("Self time"), selfTime);
        pushEntryInfoRow(WebInspector.UIString("Total time"), totalTime);
        if (node.url)
            pushEntryInfoRow(WebInspector.UIString("URL"), node.url + ":" + node.lineNumber);
        pushEntryInfoRow(WebInspector.UIString("Aggregated self time"), Number.secondsToString(node.selfTime / 1000, true));
        pushEntryInfoRow(WebInspector.UIString("Aggregated total time"), Number.secondsToString(node.totalTime / 1000, true));
        if (node.deoptReason && node.deoptReason !== "no reason")
            pushEntryInfoRow(WebInspector.UIString("Not optimized"), node.deoptReason);

        return entryInfo;
    },

    /**
     * @param {number} entryIndex
     * @return {boolean}
     */
    canJumpToEntry: function(entryIndex)
    {
        return this._timelineData.entryNodes[entryIndex].scriptId !== "0";
    },

    /**
     * @param {number} entryIndex
     * @return {!Object}
     */
    entryData: function(entryIndex)
    {
        return this._timelineData.entryNodes[entryIndex];
    },

    __proto__: WebInspector.FlameChartDataProvider
}

