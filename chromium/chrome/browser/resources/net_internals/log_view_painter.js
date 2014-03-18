// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(eroman): put these methods into a namespace.

var printLogEntriesAsText;
var createLogEntryTablePrinter;
var proxySettingsToString;
var stripCookiesAndLoginInfo;

// Start of anonymous namespace.
(function() {
'use strict';

function canCollapseBeginWithEnd(beginEntry) {
  return beginEntry &&
         beginEntry.isBegin() &&
         beginEntry.end &&
         beginEntry.end.index == beginEntry.index + 1 &&
         (!beginEntry.orig.params || !beginEntry.end.orig.params);
}

/**
 * Adds a child pre element to the end of |parent|, and writes the
 * formatted contents of |logEntries| to it.
 */
printLogEntriesAsText = function(logEntries, parent, privacyStripping,
                                 logCreationTime) {
  var tablePrinter = createLogEntryTablePrinter(logEntries, privacyStripping,
                                                logCreationTime);

  // Format the table for fixed-width text.
  tablePrinter.toText(0, parent);
}
/**
 * Creates a TablePrinter for use by the above two functions.
 */
createLogEntryTablePrinter = function(logEntries, privacyStripping,
                                      logCreationTime) {
  var entries = LogGroupEntry.createArrayFrom(logEntries);
  var tablePrinter = new TablePrinter();
  var parameterOutputter = new ParameterOutputter(tablePrinter);

  if (entries.length == 0)
    return tablePrinter;

  var startTime = timeutil.convertTimeTicksToTime(entries[0].orig.time);

  for (var i = 0; i < entries.length; ++i) {
    var entry = entries[i];

    // Avoid printing the END for a BEGIN that was immediately before, unless
    // both have extra parameters.
    if (!entry.isEnd() || !canCollapseBeginWithEnd(entry.begin)) {
      var entryTime = timeutil.convertTimeTicksToTime(entry.orig.time);
      addRowWithTime(tablePrinter, entryTime, startTime);

      for (var j = entry.getDepth(); j > 0; --j)
        tablePrinter.addCell('  ');

      var eventText = getTextForEvent(entry);
      // Get the elapsed time, and append it to the event text.
      if (entry.isBegin()) {
        var dt = '?';
        // Definite time.
        if (entry.end) {
          dt = entry.end.orig.time - entry.orig.time;
        } else if (logCreationTime != undefined) {
          dt = (logCreationTime - entryTime) + '+';
        }
        eventText += '  [dt=' + dt + ']';
      }

      var mainCell = tablePrinter.addCell(eventText);
      mainCell.allowOverflow = true;
    }

    // Output the extra parameters.
    if (typeof entry.orig.params == 'object') {
      // Those 5 skipped cells are: two for "t=", and three for "st=".
      tablePrinter.setNewRowCellIndent(5 + entry.getDepth());
      writeParameters(entry.orig, privacyStripping, parameterOutputter);

      tablePrinter.setNewRowCellIndent(0);
    }
  }

  // If viewing a saved log file, add row with just the time the log was
  // created, if the event never completed.
  var lastEntry = entries[entries.length - 1];
  // If the last entry has a non-zero depth or is a begin event, the source is
  // still active.
  var isSourceActive = lastEntry.getDepth() != 0 || lastEntry.isBegin();
  if (logCreationTime != undefined && isSourceActive)
    addRowWithTime(tablePrinter, logCreationTime, startTime);

  return tablePrinter;
}

/**
 * Adds a new row to the given TablePrinter, and adds five cells containing
 * information about the time an event occured.
 * Format is '[t=<UTC time in ms>] [st=<ms since the source started>]'.
 * @param {TablePrinter} tablePrinter The table printer to add the cells to.
 * @param {number} eventTime The time the event occured, as a UTC time in
 *     milliseconds.
 * @param {number} startTime The time the first event for the source occured,
 *     as a UTC time in milliseconds.
 */
function addRowWithTime(tablePrinter, eventTime, startTime) {
  tablePrinter.addRow();
  tablePrinter.addCell('t=');
  var tCell = tablePrinter.addCell(eventTime);
  tCell.alignRight = true;
  tablePrinter.addCell(' [st=');
  var stCell = tablePrinter.addCell(eventTime - startTime);
  stCell.alignRight = true;
  tablePrinter.addCell('] ');
}

/**
 * |hexString| must be a string of hexadecimal characters with no whitespace,
 * whose length is a multiple of two.  Writes multiple lines to |out| with
 * the hexadecimal characters from |hexString| on the left, in groups of
 * two, and their corresponding ASCII characters on the right.
 *
 * |asciiCharsPerLine| specifies how many ASCII characters will be put on each
 * line of the output string.
 */
function writeHexString(hexString, asciiCharsPerLine, out) {
  // Number of transferred bytes in a line of output.  Length of a
  // line is roughly 4 times larger.
  var hexCharsPerLine = 2 * asciiCharsPerLine;
  for (var i = 0; i < hexString.length; i += hexCharsPerLine) {
    var hexLine = '';
    var asciiLine = '';
    for (var j = i; j < i + hexCharsPerLine && j < hexString.length; j += 2) {
      var hex = hexString.substr(j, 2);
      hexLine += hex + ' ';
      var charCode = parseInt(hex, 16);
      // For ASCII codes 32 though 126, display the corresponding
      // characters.  Use a space for nulls, and a period for
      // everything else.
      if (charCode >= 0x20 && charCode <= 0x7E) {
        asciiLine += String.fromCharCode(charCode);
      } else if (charCode == 0x00) {
        asciiLine += ' ';
      } else {
        asciiLine += '.';
      }
    }

    // Make the ASCII text for the last line of output align with the previous
    // lines.
    hexLine += makeRepeatedString(' ', 3 * asciiCharsPerLine - hexLine.length);
    out.writeLine('   ' + hexLine + '  ' + asciiLine);
  }
}

/**
 * Wrapper around a TablePrinter to simplify outputting lines of text for event
 * parameters.
 */
var ParameterOutputter = (function() {
  /**
   * @constructor
   */
  function ParameterOutputter(tablePrinter) {
    this.tablePrinter_ = tablePrinter;
  }

  ParameterOutputter.prototype = {
    /**
     * Outputs a single line.
     */
    writeLine: function(line) {
      this.tablePrinter_.addRow();
      var cell = this.tablePrinter_.addCell(line);
      cell.allowOverflow = true;
      return cell;
    },

    /**
     * Outputs a key=value line which looks like:
     *
     *   --> key = value
     */
    writeArrowKeyValue: function(key, value, link) {
      var cell = this.writeLine(kArrow + key + ' = ' + value);
      cell.link = link;
    },

    /**
     * Outputs a key= line which looks like:
     *
     *   --> key =
     */
    writeArrowKey: function(key) {
      this.writeLine(kArrow + key + ' =');
    },

    /**
     * Outputs multiple lines, each indented by numSpaces.
     * For instance if numSpaces=8 it might look like this:
     *
     *         line 1
     *         line 2
     *         line 3
     */
    writeSpaceIndentedLines: function(numSpaces, lines) {
      var prefix = makeRepeatedString(' ', numSpaces);
      for (var i = 0; i < lines.length; ++i)
        this.writeLine(prefix + lines[i]);
    },

    /**
     * Outputs multiple lines such that the first line has
     * an arrow pointing at it, and subsequent lines
     * align with the first one. For example:
     *
     *   --> line 1
     *       line 2
     *       line 3
     */
    writeArrowIndentedLines: function(lines) {
      if (lines.length == 0)
        return;

      this.writeLine(kArrow + lines[0]);

      for (var i = 1; i < lines.length; ++i)
        this.writeLine(kArrowIndentation + lines[i]);
    }
  };

  var kArrow = ' --> ';
  var kArrowIndentation = '     ';

  return ParameterOutputter;
})();  // end of ParameterOutputter

/**
 * Formats the parameters for |entry| and writes them to |out|.
 * Certain event types have custom pretty printers. Everything else will
 * default to a JSON-like format.
 */
function writeParameters(entry, privacyStripping, out) {
  if (privacyStripping) {
    // If privacy stripping is enabled, remove data as needed.
    entry = stripCookiesAndLoginInfo(entry);
  } else {
    // If headers are in an object, convert them to an array for better display.
    entry = reformatHeaders(entry);
  }

  // Use any parameter writer available for this event type.
  var paramsWriter = getParamaterWriterForEventType(entry.type);
  var consumedParams = {};
  if (paramsWriter)
    paramsWriter(entry, out, consumedParams);

  // Write any un-consumed parameters.
  for (var k in entry.params) {
    if (consumedParams[k])
      continue;
    defaultWriteParameter(k, entry.params[k], out);
  }
}

/**
 * Finds a writer to format the parameters for events of type |eventType|.
 *
 * @return {function} The returned function "writer" can be invoked
 *                    as |writer(entry, writer, consumedParams)|. It will
 *                    output the parameters of |entry| to |out|, and fill
 *                    |consumedParams| with the keys of the parameters
 *                    consumed. If no writer is available for |eventType| then
 *                    returns null.
 */
function getParamaterWriterForEventType(eventType) {
  switch (eventType) {
    case EventType.HTTP_TRANSACTION_SEND_REQUEST_HEADERS:
    case EventType.HTTP_TRANSACTION_SEND_TUNNEL_HEADERS:
      return writeParamsForRequestHeaders;

    case EventType.PROXY_CONFIG_CHANGED:
      return writeParamsForProxyConfigChanged;

    case EventType.CERT_VERIFIER_JOB:
    case EventType.SSL_CERTIFICATES_RECEIVED:
      return writeParamsForCertificates;

    case EventType.SSL_VERSION_FALLBACK:
      return writeParamsForSSLVersionFallback;
  }
  return null;
}

/**
 * Default parameter writer that outputs a visualization of field named |key|
 * with value |value| to |out|.
 */
function defaultWriteParameter(key, value, out) {
  if (key == 'headers' && value instanceof Array) {
    out.writeArrowIndentedLines(value);
    return;
  }

  // For transferred bytes, display the bytes in hex and ASCII.
  if (key == 'hex_encoded_bytes' && typeof value == 'string') {
    out.writeArrowKey(key);
    writeHexString(value, 20, out);
    return;
  }

  // Handle source_dependency entries - add link and map source type to
  // string.
  if (key == 'source_dependency' && typeof value == 'object') {
    var link = '#events&s=' + value.id;
    var valueStr = value.id + ' (' + EventSourceTypeNames[value.type] + ')';
    out.writeArrowKeyValue(key, valueStr, link);
    return;
  }

  if (key == 'net_error' && typeof value == 'number') {
    var valueStr = value + ' (' + netErrorToString(value) + ')';
    out.writeArrowKeyValue(key, valueStr);
    return;
  }

  if (key == 'quic_error' && typeof value == 'number') {
    var valueStr = value + ' (' + quicErrorToString(value) + ')';
    out.writeArrowKeyValue(key, valueStr);
    return;
  }

  if (key == 'quic_crypto_handshake_message' && typeof value == 'string') {
    var lines = value.split('\n');
    out.writeArrowIndentedLines(lines);
    return;
  }

  if (key == 'quic_rst_stream_error' && typeof value == 'number') {
    var valueStr = value + ' (' + quicRstStreamErrorToString(value) + ')';
    out.writeArrowKeyValue(key, valueStr);
    return;
  }

  if (key == 'load_flags' && typeof value == 'number') {
    var valueStr = value + ' (' + getLoadFlagSymbolicString(value) + ')';
    out.writeArrowKeyValue(key, valueStr);
    return;
  }

  if (key == 'load_state' && typeof value == 'number') {
    var valueStr = value + ' (' + getKeyWithValue(LoadState, value) + ')';
    out.writeArrowKeyValue(key, valueStr);
    return;
  }

  // Otherwise just default to JSON formatting of the value.
  out.writeArrowKeyValue(key, JSON.stringify(value));
}

/**
 * Returns the set of LoadFlags that make up the integer |loadFlag|.
 * For example: getLoadFlagSymbolicString(
 */
function getLoadFlagSymbolicString(loadFlag) {
  // Load flag of 0 means "NORMAL". Special case this, since and-ing with
  // 0 is always going to be false.
  if (loadFlag == 0)
    return getKeyWithValue(LoadFlag, loadFlag);

  var matchingLoadFlagNames = [];

  for (var k in LoadFlag) {
    if (loadFlag & LoadFlag[k])
      matchingLoadFlagNames.push(k);
  }

  return matchingLoadFlagNames.join(' | ');
}

/**
 * Converts an SSL version number to a textual representation.
 * For instance, SSLVersionNumberToName(0x0301) returns 'TLS 1.0'.
 */
function SSLVersionNumberToName(version) {
  if ((version & 0xFFFF) != version) {
    // If the version number is more than 2 bytes long something is wrong.
    // Print it as hex.
    return 'SSL 0x' + version.toString(16);
  }

  // See if it is a known TLS name.
  var kTLSNames = {
    0x0301: 'TLS 1.0',
    0x0302: 'TLS 1.1',
    0x0303: 'TLS 1.2'
  };
  var name = kTLSNames[version];
  if (name)
    return name;

  // Otherwise label it as an SSL version.
  var major = (version & 0xFF00) >> 8;
  var minor = version & 0x00FF;

  return 'SSL ' + major + '.' + minor;
}

/**
 * TODO(eroman): get rid of this, as it is only used by 1 callsite.
 *
 * Indent |lines| by |start|.
 *
 * For example, if |start| = ' -> ' and |lines| = ['line1', 'line2', 'line3']
 * the output will be:
 *
 *   " -> line1\n" +
 *   "    line2\n" +
 *   "    line3"
 */
function indentLines(start, lines) {
  return start + lines.join('\n' + makeRepeatedString(' ', start.length));
}

/**
 * If entry.param.headers exists and is an object other than an array, converts
 * it into an array and returns a new entry.  Otherwise, just returns the
 * original entry.
 */
function reformatHeaders(entry) {
  // If there are no headers, or it is not an object other than an array,
  // return |entry| without modification.
  if (!entry.params || entry.params.headers === undefined ||
      typeof entry.params.headers != 'object' ||
      entry.params.headers instanceof Array) {
    return entry;
  }

  // Duplicate the top level object, and |entry.params|, so the original object
  // will not be modified.
  entry = shallowCloneObject(entry);
  entry.params = shallowCloneObject(entry.params);

  // Convert headers to an array.
  var headers = [];
  for (var key in entry.params.headers)
    headers.push(key + ': ' + entry.params.headers[key]);
  entry.params.headers = headers;

  return entry;
}

/**
 * Removes a cookie or unencrypted login information from a single HTTP header
 * line, if present, and returns the modified line.  Otherwise, just returns
 * the original line.
 */
function stripCookieOrLoginInfo(line) {
  var patterns = [
      // Cookie patterns
      /^set-cookie: /i,
      /^set-cookie2: /i,
      /^cookie: /i,

      // Unencrypted authentication patterns
      /^authorization: \S*\s*/i,
      /^proxy-authorization: \S*\s*/i];

  // Prefix will hold the first part of the string that contains no private
  // information.  If null, no part of the string contains private information.
  var prefix = null;
  for (var i = 0; i < patterns.length; i++) {
    var match = patterns[i].exec(line);
    if (match != null) {
      prefix = match[0];
      break;
    }
  }

  // Look for authentication information from data received from the server in
  // multi-round Negotiate authentication.
  if (prefix === null) {
    var challengePatterns = [
        /^www-authenticate: (\S*)\s*/i,
        /^proxy-authenticate: (\S*)\s*/i];
    for (var i = 0; i < challengePatterns.length; i++) {
      var match = challengePatterns[i].exec(line);
      if (!match)
        continue;

      // If there's no data after the scheme name, do nothing.
      if (match[0].length == line.length)
        break;

      // Ignore lines with commas, as they may contain lists of schemes, and
      // the information we want to hide is Base64 encoded, so has no commas.
      if (line.indexOf(',') >= 0)
        break;

      // Ignore Basic and Digest authentication challenges, as they contain
      // public information.
      if (/^basic$/i.test(match[1]) || /^digest$/i.test(match[1]))
        break;

      prefix = match[0];
      break;
    }
  }

  if (prefix) {
    var suffix = line.slice(prefix.length);
    // If private information has already been removed, keep the line as-is.
    // This is often the case when viewing a loaded log.
    // TODO(mmenke):  Remove '[value was stripped]' check once M24 hits stable.
    if (suffix.search(/^\[[0-9]+ bytes were stripped\]$/) == -1 &&
        suffix != '[value was stripped]') {
      return prefix + '[' + suffix.length + ' bytes were stripped]';
    }
  }

  return line;
}

/**
 * If |entry| has headers, returns a copy of |entry| with all cookie and
 * unencrypted login text removed.  Otherwise, returns original |entry| object.
 * This is needed so that JSON log dumps can be made without affecting the
 * source data.  Converts headers stored in objects to arrays.
 */
stripCookiesAndLoginInfo = function(entry) {
  if (!entry.params || entry.params.headers === undefined ||
      !(entry.params.headers instanceof Object)) {
    return entry;
  }

  // Make sure entry's headers are in an array.
  entry = reformatHeaders(entry);

  // Duplicate the top level object, and |entry.params|.  All other fields are
  // just pointers to the original values, as they won't be modified, other than
  // |entry.params.headers|.
  entry = shallowCloneObject(entry);
  entry.params = shallowCloneObject(entry.params);

  entry.params.headers = entry.params.headers.map(stripCookieOrLoginInfo);
  return entry;
}

/**
 * Outputs the request header parameters of |entry| to |out|.
 */
function writeParamsForRequestHeaders(entry, out, consumedParams) {
  var params = entry.params;

  if (!(typeof params.line == 'string') || !(params.headers instanceof Array)) {
    // Unrecognized params.
    return;
  }

  // Strip the trailing CRLF that params.line contains.
  var lineWithoutCRLF = params.line.replace(/\r\n$/g, '');
  out.writeArrowIndentedLines([lineWithoutCRLF].concat(params.headers));

  consumedParams.line = true;
  consumedParams.headers = true;
}

/**
 * Outputs the certificate parameters of |entry| to |out|.
 */
function writeParamsForCertificates(entry, out, consumedParams) {
  if (!(entry.params.certificates instanceof Array)) {
    // Unrecognized params.
    return;
  }

  var certs = entry.params.certificates.reduce(function(previous, current) {
    return previous.concat(current.split('\n'));
  }, new Array());

  out.writeArrowKey('certificates');
  out.writeSpaceIndentedLines(8, certs);

  consumedParams.certificates = true;
}

/**
 * Outputs the SSL version fallback parameters of |entry| to |out|.
 */
function writeParamsForSSLVersionFallback(entry, out, consumedParams) {
  var params = entry.params;

  if (typeof params.version_before != 'number' ||
      typeof params.version_after != 'number') {
    // Unrecognized params.
    return;
  }

  var line = SSLVersionNumberToName(params.version_before) +
             ' ==> ' +
             SSLVersionNumberToName(params.version_after);
  out.writeArrowIndentedLines([line]);

  consumedParams.version_before = true;
  consumedParams.version_after = true;
}

function writeParamsForProxyConfigChanged(entry, out, consumedParams) {
  var params = entry.params;

  if (typeof params.new_config != 'object') {
    // Unrecognized params.
    return;
  }

  if (typeof params.old_config == 'object') {
    var oldConfigString = proxySettingsToString(params.old_config);
    // The previous configuration may not be present in the case of
    // the initial proxy settings fetch.
    out.writeArrowKey('old_config');

    out.writeSpaceIndentedLines(8, oldConfigString.split('\n'));

    consumedParams.old_config = true;
  }

  var newConfigString = proxySettingsToString(params.new_config);
  out.writeArrowKey('new_config');
  out.writeSpaceIndentedLines(8, newConfigString.split('\n'));

  consumedParams.new_config = true;
}

function getTextForEvent(entry) {
  var text = '';

  if (entry.isBegin() && canCollapseBeginWithEnd(entry)) {
    // Don't prefix with '+' if we are going to collapse the END event.
    text = ' ';
  } else if (entry.isBegin()) {
    text = '+' + text;
  } else if (entry.isEnd()) {
    text = '-' + text;
  } else {
    text = ' ';
  }

  text += EventTypeNames[entry.orig.type];
  return text;
}

proxySettingsToString = function(config) {
  if (!config)
    return '';

  // TODO(eroman): if |config| has unexpected properties, print it as JSON
  //               rather than hide them.

  function getProxyListString(proxies) {
    // Older versions of Chrome would set these values as strings, whereas newer
    // logs use arrays.
    // TODO(eroman): This behavior changed in M27. Support for older logs can
    //               safely be removed circa M29.
    if (Array.isArray(proxies)) {
      var listString = proxies.join(', ');
      if (proxies.length > 1)
        return '[' + listString + ']';
      return listString;
    }
    return proxies;
  }

  // The proxy settings specify up to three major fallback choices
  // (auto-detect, custom pac url, or manual settings).
  // We enumerate these to a list so we can later number them.
  var modes = [];

  // Output any automatic settings.
  if (config.auto_detect)
    modes.push(['Auto-detect']);
  if (config.pac_url)
    modes.push(['PAC script: ' + config.pac_url]);

  // Output any manual settings.
  if (config.single_proxy || config.proxy_per_scheme) {
    var lines = [];

    if (config.single_proxy) {
      lines.push('Proxy server: ' + getProxyListString(config.single_proxy));
    } else if (config.proxy_per_scheme) {
      for (var urlScheme in config.proxy_per_scheme) {
        if (urlScheme != 'fallback') {
          lines.push('Proxy server for ' + urlScheme.toUpperCase() + ': ' +
                     getProxyListString(config.proxy_per_scheme[urlScheme]));
        }
      }
      if (config.proxy_per_scheme.fallback) {
        lines.push('Proxy server for everything else: ' +
                   getProxyListString(config.proxy_per_scheme.fallback));
      }
    }

    // Output any proxy bypass rules.
    if (config.bypass_list) {
      if (config.reverse_bypass) {
        lines.push('Reversed bypass list: ');
      } else {
        lines.push('Bypass list: ');
      }

      for (var i = 0; i < config.bypass_list.length; ++i)
        lines.push('  ' + config.bypass_list[i]);
    }

    modes.push(lines);
  }

  var result = [];
  if (modes.length < 1) {
    // If we didn't find any proxy settings modes, we are using DIRECT.
    result.push('Use DIRECT connections.');
  } else if (modes.length == 1) {
    // If there was just one mode, don't bother numbering it.
    result.push(modes[0].join('\n'));
  } else {
    // Otherwise concatenate all of the modes into a numbered list
    // (which correspond with the fallback order).
    for (var i = 0; i < modes.length; ++i)
      result.push(indentLines('(' + (i + 1) + ') ', modes[i]));
  }

  if (config.source != undefined && config.source != 'UNKNOWN')
    result.push('Source: ' + config.source);

  return result.join('\n');
};

// End of anonymous namespace.
})();
