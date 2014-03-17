/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @param {InjectedScriptHost} InjectedScriptHost
 * @param {Window} inspectedWindow
 * @param {number} injectedScriptId
 */
(function (InjectedScriptHost, inspectedWindow, injectedScriptId) {

/**
 * Protect against Object overwritten by the user code.
 * @suppress {duplicate}
 */
var Object = /** @type {function(new:Object, *=)} */ ({}.constructor);

/**
 * @param {Arguments} array
 * @param {number=} index
 * @return {Array.<*>}
 */
function slice(array, index)
{
    var result = [];
    for (var i = index || 0; i < array.length; ++i)
        result.push(array[i]);
    return result;
}

/**
 * @param {*} obj
 * @return {string}
 */
function toString(obj)
{
    // We don't use String(obj) because it could be overriden.
    return "" + obj;
}

/**
 * @param {*} obj
 * @return {string}
 */
function toStringDescription(obj)
{
    if (typeof obj === "number" && obj === 0 && 1 / obj < 0)
        return "-0"; // Negative zero.
    return "" + obj;
}

/**
 * Please use this bind, not the one from Function.prototype
 * @param {function(...)} func
 * @param {Object} thisObject
 * @param {...number} var_args
 */
function bind(func, thisObject, var_args)
{
    var args = slice(arguments, 2);

    /**
     * @param {...number} var_args
     */
    function bound(var_args)
    {
        return func.apply(thisObject, args.concat(slice(arguments)));
    }
    bound.toString = function() {
        return "bound: " + func;
    };
    return bound;
}

/**
 * @constructor
 */
var InjectedScript = function()
{
    /** @type {number} */
    this._lastBoundObjectId = 1;
    /** @type {!Object.<number, Object>} */
    this._idToWrappedObject = {};
    /** @type {!Object.<number, string>} */
    this._idToObjectGroupName = {};
    /** @type {!Object.<string, Array.<number>>} */
    this._objectGroups = {};
    /** @type {!Object.<string, Object>} */
    this._modules = {};
}

/**
 * @type {Object.<string, boolean>}
 * @const
 */
InjectedScript.primitiveTypes = {
    undefined: true,
    boolean: true,
    number: true,
    string: true
}

InjectedScript.prototype = {
    /**
     * @param {*} object
     * @return {boolean}
     */
    isPrimitiveValue: function(object)
    {
        // FIXME(33716): typeof document.all is always 'undefined'.
        return InjectedScript.primitiveTypes[typeof object] && !this._isHTMLAllCollection(object);
    },

    /**
     * @param {*} object
     * @param {string} groupName
     * @param {boolean} canAccessInspectedWindow
     * @param {boolean} generatePreview
     * @return {!RuntimeAgent.RemoteObject}
     */
    wrapObject: function(object, groupName, canAccessInspectedWindow, generatePreview)
    {
        if (canAccessInspectedWindow)
            return this._wrapObject(object, groupName, false, generatePreview);
        return this._fallbackWrapper(object);
    },

    /**
     * @param {*} object
     * @return {!RuntimeAgent.RemoteObject}
     */
    _fallbackWrapper: function(object)
    {
        var result = {};
        result.type = typeof object;
        if (this.isPrimitiveValue(object))
            result.value = object;
        else
            result.description = toString(object);
        return /** @type {!RuntimeAgent.RemoteObject} */ (result);
    },

    /**
     * @param {boolean} canAccessInspectedWindow
     * @param {Object} table
     * @param {Array.<string>|string|boolean} columns
     * @return {!RuntimeAgent.RemoteObject}
     */
    wrapTable: function(canAccessInspectedWindow, table, columns)
    {
        if (!canAccessInspectedWindow)
            return this._fallbackWrapper(table);
        var columnNames = null;
        if (typeof columns === "string")
            columns = [columns];
        if (InjectedScriptHost.type(columns) == "array") {
            columnNames = [];
            for (var i = 0; i < columns.length; ++i)
                columnNames.push(toString(columns[i]));
        }
        return this._wrapObject(table, "console", false, true, columnNames, true);
    },

    /**
     * @param {*} object
     */
    inspectNode: function(object)
    {
        this._inspect(object);
    },

    /**
     * @param {*} object
     * @return {*}
     */
    _inspect: function(object)
    {
        if (arguments.length === 0)
            return;

        var objectId = this._wrapObject(object, "");
        var hints = {};

        switch (injectedScript._describe(object)) {
            case "Database":
                var databaseId = InjectedScriptHost.databaseId(object)
                if (databaseId)
                    hints.databaseId = databaseId;
                break;
            case "Storage":
                var storageId = InjectedScriptHost.storageId(object)
                if (storageId)
                    hints.domStorageId = InjectedScriptHost.evaluate("(" + storageId + ")");
                break;
        }
        InjectedScriptHost.inspect(objectId, hints);
        return object;
    },

    /**
     * This method cannot throw.
     * @param {*} object
     * @param {string=} objectGroupName
     * @param {boolean=} forceValueType
     * @param {boolean=} generatePreview
     * @param {?Array.<string>=} columnNames
     * @param {boolean=} isTable
     * @return {!RuntimeAgent.RemoteObject}
     * @suppress {checkTypes}
     */
    _wrapObject: function(object, objectGroupName, forceValueType, generatePreview, columnNames, isTable)
    {
        try {
            return new InjectedScript.RemoteObject(object, objectGroupName, forceValueType, generatePreview, columnNames, isTable);
        } catch (e) {
            try {
                var description = injectedScript._describe(e);
            } catch (ex) {
                var description = "<failed to convert exception to string>";
            }
            return new InjectedScript.RemoteObject(description);
        }
    },

    /**
     * @param {Object} object
     * @param {string=} objectGroupName
     * @return {string}
     */
    _bind: function(object, objectGroupName)
    {
        var id = this._lastBoundObjectId++;
        this._idToWrappedObject[id] = object;
        var objectId = "{\"injectedScriptId\":" + injectedScriptId + ",\"id\":" + id + "}";
        if (objectGroupName) {
            var group = this._objectGroups[objectGroupName];
            if (!group) {
                group = [];
                this._objectGroups[objectGroupName] = group;
            }
            group.push(id);
            this._idToObjectGroupName[id] = objectGroupName;
        }
        return objectId;
    },

    /**
     * @param {string} objectId
     * @return {Object}
     */
    _parseObjectId: function(objectId)
    {
        return InjectedScriptHost.evaluate("(" + objectId + ")");
    },

    /**
     * @param {string} objectGroupName
     */
    releaseObjectGroup: function(objectGroupName)
    {
        var group = this._objectGroups[objectGroupName];
        if (!group)
            return;
        for (var i = 0; i < group.length; i++)
            this._releaseObject(group[i]);
        delete this._objectGroups[objectGroupName];
    },

    /**
     * @param {string} methodName
     * @param {string} args
     * @return {*}
     */
    dispatch: function(methodName, args)
    {
        var argsArray = InjectedScriptHost.evaluate("(" + args + ")");
        var result = this[methodName].apply(this, argsArray);
        if (typeof result === "undefined") {
            inspectedWindow.console.error("Web Inspector error: InjectedScript.%s returns undefined", methodName);
            result = null;
        }
        return result;
    },

    /**
     * @param {string} objectId
     * @param {boolean} ownProperties
     * @param {boolean} accessorPropertiesOnly
     * @return {Array.<RuntimeAgent.PropertyDescriptor>|boolean}
     */
    getProperties: function(objectId, ownProperties, accessorPropertiesOnly)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        var objectGroupName = this._idToObjectGroupName[parsedObjectId.id];

        if (!this._isDefined(object))
            return false;
        var descriptors = this._propertyDescriptors(object, ownProperties, accessorPropertiesOnly);

        // Go over properties, wrap object values.
        for (var i = 0; i < descriptors.length; ++i) {
            var descriptor = descriptors[i];
            if ("get" in descriptor)
                descriptor.get = this._wrapObject(descriptor.get, objectGroupName);
            if ("set" in descriptor)
                descriptor.set = this._wrapObject(descriptor.set, objectGroupName);
            if ("value" in descriptor)
                descriptor.value = this._wrapObject(descriptor.value, objectGroupName);
            if (!("configurable" in descriptor))
                descriptor.configurable = false;
            if (!("enumerable" in descriptor))
                descriptor.enumerable = false;
        }
        return descriptors;
    },

    /**
     * @param {string} objectId
     * @return {Array.<Object>|boolean}
     */
    getInternalProperties: function(objectId, ownProperties)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        var objectGroupName = this._idToObjectGroupName[parsedObjectId.id];
        if (!this._isDefined(object))
            return false;
        var descriptors = [];
        var internalProperties = InjectedScriptHost.getInternalProperties(object);
        if (internalProperties) {
            for (var i = 0; i < internalProperties.length; i++) {
                var property = internalProperties[i];
                var descriptor = {
                    name: property.name,
                    value: this._wrapObject(property.value, objectGroupName)
                };
                descriptors.push(descriptor);
            }
        }
        return descriptors;
    },

    /**
     * @param {string} functionId
     * @return {!DebuggerAgent.FunctionDetails|string}
     */
    getFunctionDetails: function(functionId)
    {
        var parsedFunctionId = this._parseObjectId(functionId);
        var func = this._objectForId(parsedFunctionId);
        if (typeof func !== "function")
            return "Cannot resolve function by id.";
        var details = InjectedScriptHost.functionDetails(func);
        if ("rawScopes" in details) {
            var objectGroupName = this._idToObjectGroupName[parsedFunctionId.id];
            var rawScopes = details.rawScopes;
            var scopes = [];
            delete details.rawScopes;
            for (var i = 0; i < rawScopes.length; i++)
                scopes.push(InjectedScript.CallFrameProxy._createScopeJson(rawScopes[i].type, rawScopes[i].object, objectGroupName));
            details.scopeChain = scopes;
        }
        return details;
    },

    /**
     * @param {string} objectId
     */
    releaseObject: function(objectId)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        this._releaseObject(parsedObjectId.id);
    },

    /**
     * @param {number} id
     */
    _releaseObject: function(id)
    {
        delete this._idToWrappedObject[id];
        delete this._idToObjectGroupName[id];
    },

    /**
     * @param {Object} object
     * @param {boolean=} ownProperties
     * @param {boolean=} accessorPropertiesOnly
     * @return {Array.<Object>}
     */
    _propertyDescriptors: function(object, ownProperties, accessorPropertiesOnly)
    {
        var descriptors = [];
        var nameProcessed = { __proto__: null };

        /**
         * @param {Object} o
         * @param {Array.<string>} names
         */
        function process(o, names)
        {
            for (var i = 0; i < names.length; ++i) {
                var name = names[i];
                if (nameProcessed[name])
                    continue;

                try {
                    nameProcessed[name] = true;
                    var descriptor = Object.getOwnPropertyDescriptor(/** @type {!Object} */ (o), name);
                    if (descriptor) {
                        if (accessorPropertiesOnly && !("get" in descriptor || "set" in descriptor))
                            continue;
                    } else {
                        // Not all bindings provide proper descriptors. Fall back to the writable, configurable property.
                        if (accessorPropertiesOnly)
                            continue;
                        try {
                            descriptor = { name: name, value: o[name], writable: false, configurable: false, enumerable: false};
                            if (o === object)
                                descriptor.isOwn = true;
                            descriptors.push(descriptor);
                        } catch (e) {
                            // Silent catch.
                        }
                        continue;
                    }
                } catch (e) {
                    if (accessorPropertiesOnly)
                        continue;
                    var descriptor = {};
                    descriptor.value = e;
                    descriptor.wasThrown = true;
                }

                descriptor.name = name;
                if (o === object)
                    descriptor.isOwn = true;
                descriptors.push(descriptor);
            }
        }

        for (var o = object; this._isDefined(o); o = o.__proto__) {
            // First call Object.keys() to enforce ordering of the property descriptors.
            process(o, Object.keys(/** @type {!Object} */ (o)));
            process(o, Object.getOwnPropertyNames(/** @type {!Object} */ (o)));

            if (ownProperties) {
                if (object.__proto__ && !accessorPropertiesOnly)
                    descriptors.push({ name: "__proto__", value: object.__proto__, writable: true, configurable: true, enumerable: false, isOwn: true});
                break;
            }
        }

        return descriptors;
    },

    /**
     * @param {string} expression
     * @param {string} objectGroup
     * @param {boolean} injectCommandLineAPI
     * @param {boolean} returnByValue
     * @param {boolean} generatePreview
     * @return {*}
     */
    evaluate: function(expression, objectGroup, injectCommandLineAPI, returnByValue, generatePreview)
    {
        return this._evaluateAndWrap(InjectedScriptHost.evaluate, InjectedScriptHost, expression, objectGroup, false, injectCommandLineAPI, returnByValue, generatePreview);
    },

    /**
     * @param {string} objectId
     * @param {string} expression
     * @param {boolean} returnByValue
     * @return {Object|string}
     */
    callFunctionOn: function(objectId, expression, args, returnByValue)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        if (!this._isDefined(object))
            return "Could not find object with given id";

        if (args) {
            var resolvedArgs = [];
            args = InjectedScriptHost.evaluate(args);
            for (var i = 0; i < args.length; ++i) {
                var resolvedCallArgument;
                try {
                    resolvedCallArgument = this._resolveCallArgument(args[i]);
                } catch (e) {
                    return toString(e);
                }
                resolvedArgs.push(resolvedCallArgument)
            }
        }

        try {
            var objectGroup = this._idToObjectGroupName[parsedObjectId.id];
            var func = InjectedScriptHost.evaluate("(" + expression + ")");
            if (typeof func !== "function")
                return "Given expression does not evaluate to a function";

            return { wasThrown: false,
                     result: this._wrapObject(func.apply(object, resolvedArgs), objectGroup, returnByValue) };
        } catch (e) {
            return this._createThrownValue(e, objectGroup);
        }
    },

    /**
     * Resolves a value from CallArgument description.
     * @param {RuntimeAgent.CallArgument} callArgumentJson
     * @return {*} resolved value
     * @throws {string} error message
     */
    _resolveCallArgument: function(callArgumentJson) {
        var objectId = callArgumentJson.objectId;
        if (objectId) {
            var parsedArgId = this._parseObjectId(objectId);
            if (!parsedArgId || parsedArgId["injectedScriptId"] !== injectedScriptId)
                throw "Arguments should belong to the same JavaScript world as the target object.";

            var resolvedArg = this._objectForId(parsedArgId);
            if (!this._isDefined(resolvedArg))
                throw "Could not find object with given id";

            return resolvedArg;
        } else if ("value" in callArgumentJson) {
            return callArgumentJson.value;
        }
        return undefined;
    },

    /**
     * @param {Function} evalFunction
     * @param {Object} object
     * @param {string} objectGroup
     * @param {boolean} isEvalOnCallFrame
     * @param {boolean} injectCommandLineAPI
     * @param {boolean} returnByValue
     * @param {boolean} generatePreview
     * @return {*}
     */
    _evaluateAndWrap: function(evalFunction, object, expression, objectGroup, isEvalOnCallFrame, injectCommandLineAPI, returnByValue, generatePreview)
    {
        try {
            return { wasThrown: false,
                     result: this._wrapObject(this._evaluateOn(evalFunction, object, objectGroup, expression, isEvalOnCallFrame, injectCommandLineAPI), objectGroup, returnByValue, generatePreview) };
        } catch (e) {
            return this._createThrownValue(e, objectGroup);
        }
    },

    /**
     * @param {*} value
     * @param {string} objectGroup
     * @return {Object}
     */
    _createThrownValue: function(value, objectGroup)
    {
        var remoteObject = this._wrapObject(value, objectGroup);
        try {
            remoteObject.description = toStringDescription(value);
        } catch (e) {}
        return { wasThrown: true, result: remoteObject };
    },

    /**
     * @param {Function} evalFunction
     * @param {Object} object
     * @param {string} objectGroup
     * @param {string} expression
     * @param {boolean} isEvalOnCallFrame
     * @param {boolean} injectCommandLineAPI
     * @return {*}
     */
    _evaluateOn: function(evalFunction, object, objectGroup, expression, isEvalOnCallFrame, injectCommandLineAPI)
    {
        // Only install command line api object for the time of evaluation.
        // Surround the expression in with statements to inject our command line API so that
        // the window object properties still take more precedent than our API functions.

        try {
            if (injectCommandLineAPI && inspectedWindow.console) {
                inspectedWindow.console._commandLineAPI = new CommandLineAPI(this._commandLineAPIImpl, isEvalOnCallFrame ? object : null);
                expression = "with ((console && console._commandLineAPI) || {}) {\n" + expression + "\n}";
            }
            var result = evalFunction.call(object, expression);
            if (objectGroup === "console")
                this._lastResult = result;
            return result;
        } finally {
            if (injectCommandLineAPI && inspectedWindow.console)
                delete inspectedWindow.console._commandLineAPI;
        }
    },

    /**
     * @param {Object} callFrame
     * @return {!Array.<InjectedScript.CallFrameProxy>|boolean}
     */
    wrapCallFrames: function(callFrame)
    {
        if (!callFrame)
            return false;

        var result = [];
        var depth = 0;
        do {
            result.push(new InjectedScript.CallFrameProxy(depth++, callFrame));
            callFrame = callFrame.caller;
        } while (callFrame);
        return result;
    },

    /**
     * @param {Object} topCallFrame
     * @param {string} callFrameId
     * @param {string} expression
     * @param {string} objectGroup
     * @param {boolean} injectCommandLineAPI
     * @param {boolean} returnByValue
     * @param {boolean} generatePreview
     * @return {*}
     */
    evaluateOnCallFrame: function(topCallFrame, callFrameId, expression, objectGroup, injectCommandLineAPI, returnByValue, generatePreview)
    {
        var callFrame = this.callFrameForId(topCallFrame, callFrameId);
        if (!callFrame)
            return "Could not find call frame with given id";
        return this._evaluateAndWrap(callFrame.evaluate, callFrame, expression, objectGroup, true, injectCommandLineAPI, returnByValue, generatePreview);
    },

    /**
     * @param {Object} topCallFrame
     * @param {string} callFrameId
     * @return {*}
     */
    restartFrame: function(topCallFrame, callFrameId)
    {
        var callFrame = this.callFrameForId(topCallFrame, callFrameId);
        if (!callFrame)
            return "Could not find call frame with given id";
        var result = callFrame.restart();
        if (result === false)
            result = "Restart frame is not supported";
        return result;
    },

    /**
     * @param {Object} topCallFrame
     * @param {string} callFrameId
     * @return {*} a stepIn position array ready for protocol JSON or a string error
     */
    getStepInPositions: function(topCallFrame, callFrameId)
    {
        var callFrame = this.callFrameForId(topCallFrame, callFrameId);
        if (!callFrame)
            return "Could not find call frame with given id";
        var stepInPositionsUnpacked = JSON.parse(callFrame.stepInPositions);
        if (typeof stepInPositionsUnpacked !== "object")
            return "Step in positions not available";
        return stepInPositionsUnpacked;
    },

    /**
     * Either callFrameId or functionObjectId must be specified.
     * @param {Object} topCallFrame
     * @param {string|boolean} callFrameId or false
     * @param {string|boolean} functionObjectId or false
     * @param {number} scopeNumber
     * @param {string} variableName
     * @param {string} newValueJsonString RuntimeAgent.CallArgument structure serialized as string
     * @return {string|undefined} undefined if success or an error message
     */
    setVariableValue: function(topCallFrame, callFrameId, functionObjectId, scopeNumber, variableName, newValueJsonString)
    {
        var setter;
        if (typeof callFrameId === "string") {
            var callFrame = this.callFrameForId(topCallFrame, callFrameId);
            if (!callFrame)
                return "Could not find call frame with given id";
            setter = callFrame.setVariableValue.bind(callFrame);
        } else {
            var parsedFunctionId = this._parseObjectId(/** @type {string} */ (functionObjectId));
            var func = this._objectForId(parsedFunctionId);
            if (typeof func !== "function")
                return "Cannot resolve function by id.";
            setter = InjectedScriptHost.setFunctionVariableValue.bind(InjectedScriptHost, func);
        }
        var newValueJson;
        try {
            newValueJson = InjectedScriptHost.evaluate("(" + newValueJsonString + ")");
        } catch (e) {
            return "Failed to parse new value JSON " + newValueJsonString + " : " + e;
        }
        var resolvedValue;
        try {
            resolvedValue = this._resolveCallArgument(newValueJson);
        } catch (e) {
            return toString(e);
        }
        try {
            setter(scopeNumber, variableName, resolvedValue);
        } catch (e) {
            return "Failed to change variable value: " + e;
        }
        return undefined;
    },

    /**
     * @param {Object} topCallFrame
     * @param {string} callFrameId
     * @return {Object}
     */
    callFrameForId: function(topCallFrame, callFrameId)
    {
        var parsedCallFrameId = InjectedScriptHost.evaluate("(" + callFrameId + ")");
        var ordinal = parsedCallFrameId["ordinal"];
        var callFrame = topCallFrame;
        while (--ordinal >= 0 && callFrame)
            callFrame = callFrame.caller;
        return callFrame;
    },

    /**
     * @param {Object} objectId
     * @return {Object}
     */
    _objectForId: function(objectId)
    {
        return this._idToWrappedObject[objectId.id];
    },

    /**
     * @param {string} objectId
     * @return {Object}
     */
    findObjectById: function(objectId)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        return this._objectForId(parsedObjectId);
    },

    /**
     * @param {string} objectId
     * @return {Node}
     */
    nodeForObjectId: function(objectId)
    {
        var object = this.findObjectById(objectId);
        if (!object || this._subtype(object) !== "node")
            return null;
        return /** @type {Node} */ (object);
    },

    /**
     * @param {string} name
     * @return {Object}
     */
    module: function(name)
    {
        return this._modules[name];
    },

    /**
     * @param {string} name
     * @param {string} source
     * @return {Object}
     */
    injectModule: function(name, source)
    {
        delete this._modules[name];
        var moduleFunction = InjectedScriptHost.evaluate("(" + source + ")");
        if (typeof moduleFunction !== "function") {
            inspectedWindow.console.error("Web Inspector error: A function was expected for module %s evaluation", name);
            return null;
        }
        var module = moduleFunction.call(inspectedWindow, InjectedScriptHost, inspectedWindow, injectedScriptId, this);
        this._modules[name] = module;
        return module;
    },

    /**
     * @param {*} object
     * @return {boolean}
     */
    _isDefined: function(object)
    {
        return !!object || this._isHTMLAllCollection(object);
    },

    /**
     * @param {*} object
     * @return {boolean}
     */
    _isHTMLAllCollection: function(object)
    {
        // document.all is reported as undefined, but we still want to process it.
        return (typeof object === "undefined") && InjectedScriptHost.isHTMLAllCollection(object);
    },

    /**
     * @param {*} obj
     * @return {string?}
     */
    _subtype: function(obj)
    {
        if (obj === null)
            return "null";

        if (this.isPrimitiveValue(obj))
            return null;

        if (this._isHTMLAllCollection(obj))
            return "array";

        var preciseType = InjectedScriptHost.type(obj);
        if (preciseType)
            return preciseType;

        // FireBug's array detection.
        try {
            if (typeof obj.splice === "function" && isFinite(obj.length))
                return "array";
            if (Object.prototype.toString.call(obj) === "[object Arguments]" && isFinite(obj.length)) // arguments.
                return "array";
        } catch (e) {
        }

        // If owning frame has navigated to somewhere else window properties will be undefined.
        return null;
    },

    /**
     * @param {*} obj
     * @return {string?}
     */
    _describe: function(obj)
    {
        if (this.isPrimitiveValue(obj))
            return null;

        // Type is object, get subtype.
        var subtype = this._subtype(obj);

        if (subtype === "regexp")
            return toString(obj);

        if (subtype === "date")
            return toString(obj);

        if (subtype === "node") {
            var description = obj.nodeName.toLowerCase();
            switch (obj.nodeType) {
            case 1 /* Node.ELEMENT_NODE */:
                description += obj.id ? "#" + obj.id : "";
                var className = obj.className;
                description += (className && typeof className === "string") ? "." + className.trim().replace(/\s+/g, ".") : "";
                break;
            case 10 /*Node.DOCUMENT_TYPE_NODE */:
                description = "<!DOCTYPE " + description + ">";
                break;
            }
            return description;
        }

        var className = InjectedScriptHost.internalConstructorName(obj);
        if (subtype === "array") {
            if (typeof obj.length === "number")
                className += "[" + obj.length + "]";
            return className;
        }

        // NodeList in JSC is a function, check for array prior to this.
        if (typeof obj === "function")
            return toString(obj);

        if (className === "Object") {
            // In Chromium DOM wrapper prototypes will have Object as their constructor name,
            // get the real DOM wrapper name from the constructor property.
            var constructorName = obj.constructor && obj.constructor.name;
            if (constructorName)
                return constructorName;
        }
        return className;
    }
}

/**
 * @type {!InjectedScript}
 * @const
 */
var injectedScript = new InjectedScript();

/**
 * @constructor
 * @param {*} object
 * @param {string=} objectGroupName
 * @param {boolean=} forceValueType
 * @param {boolean=} generatePreview
 * @param {?Array.<string>=} columnNames
 * @param {boolean=} isTable
 */
InjectedScript.RemoteObject = function(object, objectGroupName, forceValueType, generatePreview, columnNames, isTable)
{
    this.type = typeof object;
    if (injectedScript.isPrimitiveValue(object) || object === null || forceValueType) {
        // We don't send undefined values over JSON.
        if (this.type !== "undefined")
            this.value = object;

        // Null object is object with 'null' subtype.
        if (object === null)
            this.subtype = "null";

        // Provide user-friendly number values.
        if (this.type === "number")
            this.description = toStringDescription(object);
        return;
    }

    object = /** @type {Object} */ (object);

    this.objectId = injectedScript._bind(object, objectGroupName);
    var subtype = injectedScript._subtype(object);
    if (subtype)
        this.subtype = subtype;
    this.className = InjectedScriptHost.internalConstructorName(object);
    this.description = injectedScript._describe(object);

    if (generatePreview && (this.type === "object" || injectedScript._isHTMLAllCollection(object)))
        this.preview = this._generatePreview(object, undefined, columnNames, isTable, false);
}

InjectedScript.RemoteObject.prototype = {
    /**
     * @param {Object} object
     * @param {Array.<string>=} firstLevelKeys
     * @param {?Array.<string>=} secondLevelKeys
     * @param {boolean=} isTable
     * @param {boolean=} isTableRow
     * @return {!RuntimeAgent.ObjectPreview} preview
     */
    _generatePreview: function(object, firstLevelKeys, secondLevelKeys, isTable, isTableRow)
    {
        var preview = {};
        preview.lossless = true;
        preview.overflow = false;
        preview.properties = [];

        var firstLevelKeysCount = firstLevelKeys ? firstLevelKeys.length : 0;

        var propertiesThreshold = {
            properties: (isTable || isTableRow) ? 1000 : Math.max(5, firstLevelKeysCount),
            indexes: (isTable || isTableRow) ? 1000 : Math.max(100, firstLevelKeysCount)
        };

        try {
            var descriptors = injectedScript._propertyDescriptors(object);

            if (firstLevelKeys) {
                var nameToDescriptors = {};
                for (var i = 0; i < descriptors.length; ++i) {
                    var descriptor = descriptors[i];
                    nameToDescriptors["#" + descriptor.name] = descriptor;
                }
                descriptors = [];
                for (var i = 0; i < firstLevelKeys.length; ++i)
                    descriptors.push(nameToDescriptors["#" + firstLevelKeys[i]]);
            }

            for (var i = 0; i < descriptors.length; ++i) {
                if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0)
                    break;

                var descriptor = descriptors[i];
                if (!descriptor)
                    continue;
                if (descriptor.wasThrown) {
                    preview.lossless = false;
                    continue;
                }
                if (!descriptor.enumerable && !descriptor.isOwn)
                    continue;

                var name = descriptor.name;
                if (name === "__proto__")
                    continue;
                if (this.subtype === "array" && name === "length")
                    continue;

                if (!("value" in descriptor)) {
                    this._appendPropertyPreview(preview, { name: name, type: "accessor" }, propertiesThreshold);
                    continue;
                }

                var value = descriptor.value;
                if (value === null) {
                    this._appendPropertyPreview(preview, { name: name, type: "object", value: "null" }, propertiesThreshold);
                    continue;
                }

                const maxLength = 100;
                var type = typeof value;
                if (!descriptor.enumerable && type === "function")
                    continue;
                if (type === "undefined" && injectedScript._isHTMLAllCollection(value))
                    type = "object";

                if (InjectedScript.primitiveTypes[type]) {
                    if (type === "string" && value.length > maxLength) {
                        value = this._abbreviateString(value, maxLength, true);
                        preview.lossless = false;
                    }
                    this._appendPropertyPreview(preview, { name: name, type: type, value: toStringDescription(value) }, propertiesThreshold);
                    continue;
                }

                if (secondLevelKeys === null || secondLevelKeys) {
                    var subPreview = this._generatePreview(value, secondLevelKeys || undefined, undefined, false, isTable);
                    var property = { name: name, type: type, valuePreview: subPreview };
                    this._appendPropertyPreview(preview, property, propertiesThreshold);
                    if (!subPreview.lossless)
                        preview.lossless = false;
                    if (subPreview.overflow)
                        preview.overflow = true;
                    continue;
                }

                preview.lossless = false;

                var subtype = injectedScript._subtype(value);
                var description = "";
                if (type !== "function")
                    description = this._abbreviateString(/** @type {string} */ (injectedScript._describe(value)), maxLength, subtype === "regexp");

                var property = { name: name, type: type, value: description };
                if (subtype)
                    property.subtype = subtype;
                this._appendPropertyPreview(preview, property, propertiesThreshold);
            }
        } catch (e) {
            preview.lossless = false;
        }

        return preview;
    },

    /**
     * @param {!RuntimeAgent.ObjectPreview} preview
     * @param {!Object} property
     * @param {!Object} propertiesThreshold
     */
    _appendPropertyPreview: function(preview, property, propertiesThreshold)
    {
        if (toString(property.name >>> 0) === property.name)
            propertiesThreshold.indexes--;
        else
            propertiesThreshold.properties--;
        if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0) {
            preview.overflow = true;
            preview.lossless = false;
        } else {
            preview.properties.push(property);
        }
    },

    /**
     * @param {string} string
     * @param {number} maxLength
     * @param {boolean=} middle
     * @returns
     */
    _abbreviateString: function(string, maxLength, middle)
    {
        if (string.length <= maxLength)
            return string;
        if (middle) {
            var leftHalf = maxLength >> 1;
            var rightHalf = maxLength - leftHalf - 1;
            return string.substr(0, leftHalf) + "\u2026" + string.substr(string.length - rightHalf, rightHalf);
        }
        return string.substr(0, maxLength) + "\u2026";
    }
}
/**
 * @constructor
 * @param {number} ordinal
 * @param {!Object} callFrame
 */
InjectedScript.CallFrameProxy = function(ordinal, callFrame)
{
    this.callFrameId = "{\"ordinal\":" + ordinal + ",\"injectedScriptId\":" + injectedScriptId + "}";
    this.functionName = (callFrame.type === "function" ? callFrame.functionName : "");
    this.location = { scriptId: toString(callFrame.sourceID), lineNumber: callFrame.line, columnNumber: callFrame.column };
    this.scopeChain = this._wrapScopeChain(callFrame);
    this.this = injectedScript._wrapObject(callFrame.thisObject, "backtrace");
    if (callFrame.isAtReturn)
        this.returnValue = injectedScript._wrapObject(callFrame.returnValue, "backtrace");
}

InjectedScript.CallFrameProxy.prototype = {
    /**
     * @param {Object} callFrame
     * @return {!Array.<DebuggerAgent.Scope>}
     */
    _wrapScopeChain: function(callFrame)
    {
        var scopeChain = callFrame.scopeChain;
        var scopeChainProxy = [];
        for (var i = 0; i < scopeChain.length; i++) {
            var scope = InjectedScript.CallFrameProxy._createScopeJson(callFrame.scopeType(i), scopeChain[i], "backtrace");
            scopeChainProxy.push(scope);
        }
        return scopeChainProxy;
    }
}

/**
 * @param {number} scopeTypeCode
 * @param {*} scopeObject
 * @param {string} groupId
 * @return {!DebuggerAgent.Scope}
 */
InjectedScript.CallFrameProxy._createScopeJson = function(scopeTypeCode, scopeObject, groupId) {
    const GLOBAL_SCOPE = 0;
    const LOCAL_SCOPE = 1;
    const WITH_SCOPE = 2;
    const CLOSURE_SCOPE = 3;
    const CATCH_SCOPE = 4;

    /** @type {!Object.<number, string>} */
    var scopeTypeNames = {};
    scopeTypeNames[GLOBAL_SCOPE] = "global";
    scopeTypeNames[LOCAL_SCOPE] = "local";
    scopeTypeNames[WITH_SCOPE] = "with";
    scopeTypeNames[CLOSURE_SCOPE] = "closure";
    scopeTypeNames[CATCH_SCOPE] = "catch";

    return {
        object: injectedScript._wrapObject(scopeObject, groupId),
        type: /** @type {DebuggerAgent.ScopeType} */ (scopeTypeNames[scopeTypeCode])
    };
}

/**
 * @constructor
 * @param {CommandLineAPIImpl} commandLineAPIImpl
 * @param {Object} callFrame
 */
function CommandLineAPI(commandLineAPIImpl, callFrame)
{
    /**
     * @param {string} member
     * @return {boolean}
     */
    function inScopeVariables(member)
    {
        if (!callFrame)
            return false;

        var scopeChain = callFrame.scopeChain;
        for (var i = 0; i < scopeChain.length; ++i) {
            if (member in scopeChain[i])
                return true;
        }
        return false;
    }

    /**
     * @param {string} name The name of the method for which a toString method should be generated.
     * @return {function():string}
     */
    function customToStringMethod(name)
    {
        return function()
        {
            var funcArgsSyntax = "";
            try {
                var funcSyntax = "" + commandLineAPIImpl[name];
                funcSyntax = funcSyntax.replace(/\n/g, " ");
                funcSyntax = funcSyntax.replace(/^function[^\(]*\(([^\)]*)\).*$/, "$1");
                funcSyntax = funcSyntax.replace(/\s*,\s*/g, ", ");
                funcSyntax = funcSyntax.replace(/\bopt_(\w+)\b/g, "[$1]");
                funcArgsSyntax = funcSyntax.trim();
            } catch (e) {
            }
            return "function " + name + "(" + funcArgsSyntax + ") { [Command Line API] }";
        };
    }

    for (var i = 0; i < CommandLineAPI.members_.length; ++i) {
        var member = CommandLineAPI.members_[i];
        if (member in inspectedWindow || inScopeVariables(member))
            continue;

        this[member] = bind(commandLineAPIImpl[member], commandLineAPIImpl);
        this[member].toString = customToStringMethod(member);
    }

    for (var i = 0; i < 5; ++i) {
        var member = "$" + i;
        if (member in inspectedWindow || inScopeVariables(member))
            continue;

        this.__defineGetter__("$" + i, bind(commandLineAPIImpl._inspectedObject, commandLineAPIImpl, i));
    }

    this.$_ = injectedScript._lastResult;
}

// NOTE: Please keep the list of API methods below snchronized to that in WebInspector.RuntimeModel!
// NOTE: Argument names of these methods will be printed in the console, so use pretty names!
/**
 * @type {Array.<string>}
 * @const
 */
CommandLineAPI.members_ = [
    "$", "$$", "$x", "dir", "dirxml", "keys", "values", "profile", "profileEnd",
    "monitorEvents", "unmonitorEvents", "inspect", "copy", "clear", "getEventListeners",
    "debug", "undebug", "monitor", "unmonitor", "table"
];

/**
 * @constructor
 */
function CommandLineAPIImpl()
{
}

CommandLineAPIImpl.prototype = {
    /**
     * @param {string} selector
     * @param {Node=} opt_startNode
     */
    $: function (selector, opt_startNode)
    {
        if (this._canQuerySelectorOnNode(opt_startNode))
            return opt_startNode.querySelector(selector);

        return inspectedWindow.document.querySelector(selector);
    },

    /**
     * @param {string} selector
     * @param {Node=} opt_startNode
     */
    $$: function (selector, opt_startNode)
    {
        if (this._canQuerySelectorOnNode(opt_startNode))
            return opt_startNode.querySelectorAll(selector);
        return inspectedWindow.document.querySelectorAll(selector);
    },

    /**
     * @param {Node=} node
     * @return {boolean}
     */
    _canQuerySelectorOnNode: function(node)
    {
        return !!node && InjectedScriptHost.type(node) === "node" && (node.nodeType === Node.ELEMENT_NODE || node.nodeType === Node.DOCUMENT_NODE || node.nodeType === Node.DOCUMENT_FRAGMENT_NODE);
    },

    /**
     * @param {string} xpath
     * @param {Node=} opt_startNode
     */
    $x: function(xpath, opt_startNode)
    {
        var doc = (opt_startNode && opt_startNode.ownerDocument) || inspectedWindow.document;
        var result = doc.evaluate(xpath, opt_startNode || doc, null, XPathResult.ANY_TYPE, null);
        switch (result.resultType) {
        case XPathResult.NUMBER_TYPE:
            return result.numberValue;
        case XPathResult.STRING_TYPE:
            return result.stringValue;
        case XPathResult.BOOLEAN_TYPE:
            return result.booleanValue;
        default:
            var nodes = [];
            var node;
            while (node = result.iterateNext())
                nodes.push(node);
            return nodes;
        }
    },

    dir: function(var_args)
    {
        return inspectedWindow.console.dir.apply(inspectedWindow.console, arguments)
    },

    dirxml: function(var_args)
    {
        return inspectedWindow.console.dirxml.apply(inspectedWindow.console, arguments)
    },

    keys: function(object)
    {
        return Object.keys(object);
    },

    values: function(object)
    {
        var result = [];
        for (var key in object)
            result.push(object[key]);
        return result;
    },

    profile: function(opt_title)
    {
        return inspectedWindow.console.profile.apply(inspectedWindow.console, arguments)
    },

    profileEnd: function(opt_title)
    {
        return inspectedWindow.console.profileEnd.apply(inspectedWindow.console, arguments)
    },

    /**
     * @param {Object} object
     * @param {Array.<string>|string=} opt_types
     */
    monitorEvents: function(object, opt_types)
    {
        if (!object || !object.addEventListener || !object.removeEventListener)
            return;
        var types = this._normalizeEventTypes(opt_types);
        for (var i = 0; i < types.length; ++i) {
            object.removeEventListener(types[i], this._logEvent, false);
            object.addEventListener(types[i], this._logEvent, false);
        }
    },

    /**
     * @param {Object} object
     * @param {Array.<string>|string=} opt_types
     */
    unmonitorEvents: function(object, opt_types)
    {
        if (!object || !object.addEventListener || !object.removeEventListener)
            return;
        var types = this._normalizeEventTypes(opt_types);
        for (var i = 0; i < types.length; ++i)
            object.removeEventListener(types[i], this._logEvent, false);
    },

    /**
     * @param {*} object
     * @return {*}
     */
    inspect: function(object)
    {
        return injectedScript._inspect(object);
    },

    copy: function(object)
    {
        var string;
        if (injectedScript._subtype(object) === "node") {
            string = object.outerHTML;
        } else if (injectedScript.isPrimitiveValue(object)) {
            string = toString(object);
        } else {
            try {
                string = JSON.stringify(object, null, "  ");
            } catch (e) {
                string = toString(object);
            }
        }

        var hints = { copyToClipboard: true };
        var remoteObject = injectedScript._wrapObject(string, "")
        InjectedScriptHost.inspect(remoteObject, hints);
    },

    clear: function()
    {
        InjectedScriptHost.clearConsoleMessages();
    },

    /**
     * @param {Node} node
     * @return {{type: string, listener: function(), useCapture: boolean, remove: function()}|undefined}
     */
    getEventListeners: function(node)
    {
        var result = InjectedScriptHost.getEventListeners(node);
        if (!result)
            return result;
        /** @this {{type: string, listener: function(), useCapture: boolean}} */
        var removeFunc = function()
        {
            node.removeEventListener(this.type, this.listener, this.useCapture);
        }
        for (var type in result) {
            var listeners = result[type];
            for (var i = 0, listener; listener = listeners[i]; ++i) {
                listener["type"] = type;
                listener["remove"] = removeFunc;
            }
        }
        return result;
    },

    debug: function(fn)
    {
        InjectedScriptHost.debugFunction(fn);
    },

    undebug: function(fn)
    {
        InjectedScriptHost.undebugFunction(fn);
    },

    monitor: function(fn)
    {
        InjectedScriptHost.monitorFunction(fn);
    },

    unmonitor: function(fn) {
        InjectedScriptHost.unmonitorFunction(fn);
    },

    table: function(data, opt_columns)
    {
        inspectedWindow.console.table.apply(inspectedWindow.console, arguments);
    },

    /**
     * @param {number} num
     */
    _inspectedObject: function(num)
    {
        return InjectedScriptHost.inspectedObject(num);
    },

    /**
     * @param {Array.<string>|string=} types
     * @return {Array.<string>}
     */
    _normalizeEventTypes: function(types)
    {
        if (typeof types === "undefined")
            types = [ "mouse", "key", "touch", "control", "load", "unload", "abort", "error", "select", "change", "submit", "reset", "focus", "blur", "resize", "scroll", "search", "devicemotion", "deviceorientation" ];
        else if (typeof types === "string")
            types = [ types ];

        var result = [];
        for (var i = 0; i < types.length; i++) {
            if (types[i] === "mouse")
                result.splice(0, 0, "mousedown", "mouseup", "click", "dblclick", "mousemove", "mouseover", "mouseout", "mousewheel");
            else if (types[i] === "key")
                result.splice(0, 0, "keydown", "keyup", "keypress", "textInput");
            else if (types[i] === "touch")
                result.splice(0, 0, "touchstart", "touchmove", "touchend", "touchcancel");
            else if (types[i] === "control")
                result.splice(0, 0, "resize", "scroll", "zoom", "focus", "blur", "select", "change", "submit", "reset");
            else
                result.push(types[i]);
        }
        return result;
    },

    /**
     * @param {Event} event
     */
    _logEvent: function(event)
    {
        inspectedWindow.console.log(event.type, event);
    }
}

injectedScript._commandLineAPIImpl = new CommandLineAPIImpl();
return injectedScript;
})
