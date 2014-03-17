#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''python %prog [options] platform chromium_os_flag template

platform specifies which platform source is being generated for
  and can be one of (win, mac, linux)
chromium_os_flag should be 1 if this is a Chromium OS build
template is the path to a .json policy template file.'''

from __future__ import with_statement
import json
from optparse import OptionParser
import re
import sys
import textwrap


CHROME_POLICY_KEY = 'SOFTWARE\\\\Policies\\\\Google\\\\Chrome'
CHROMIUM_POLICY_KEY = 'SOFTWARE\\\\Policies\\\\Chromium'


class PolicyDetails:
  """Parses a policy template and caches all its details."""

  # Maps policy types to a tuple with 3 other types:
  # - the equivalent base::Value::Type or 'TYPE_EXTERNAL' if the policy
  #   references external data
  # - the equivalent Protobuf field type
  # - the name of one of the protobufs for shared policy types
  # TODO(joaodasilva): refactor the 'dict' type into a more generic 'json' type
  # that can also be used to represent lists of other JSON objects.
  TYPE_MAP = {
    'dict':         ('TYPE_DICTIONARY',   'string',       'String'),
    'external':     ('TYPE_EXTERNAL',     'string',       'String'),
    'int':          ('TYPE_INTEGER',      'int64',        'Integer'),
    'int-enum':     ('TYPE_INTEGER',      'int64',        'Integer'),
    'list':         ('TYPE_LIST',         'StringList',   'StringList'),
    'main':         ('TYPE_BOOLEAN',      'bool',         'Boolean'),
    'string':       ('TYPE_STRING',       'string',       'String'),
    'string-enum':  ('TYPE_STRING',       'string',       'String'),
  }

  class EnumItem:
    def __init__(self, item):
      self.caption = PolicyDetails._RemovePlaceholders(item['caption'])
      self.value = item['value']

  def __init__(self, policy, os, is_chromium_os):
    self.id = policy['id']
    self.name = policy['name']
    self.is_deprecated = policy.get('deprecated', False)
    self.is_device_only = policy.get('device_only', False)
    self.schema = policy.get('schema', {})

    expected_platform = 'chrome_os' if is_chromium_os else os.lower()
    self.platforms = []
    for platform, version in [ p.split(':') for p in policy['supported_on'] ]:
      if not version.endswith('-'):
        continue

      if platform.startswith('chrome.'):
        platform_sub = platform[7:]
        if platform_sub == '*':
          self.platforms.extend(['win', 'mac', 'linux'])
        else:
          self.platforms.append(platform_sub)
      else:
        self.platforms.append(platform)

    self.platforms.sort()
    self.is_supported = expected_platform in self.platforms

    if not PolicyDetails.TYPE_MAP.has_key(policy['type']):
      raise NotImplementedError('Unknown policy type for %s: %s' %
                                (policy['name'], policy['type']))
    self.policy_type, self.protobuf_type, self.policy_protobuf_type = \
        PolicyDetails.TYPE_MAP[policy['type']]
    self.schema = policy['schema']

    self.desc = '\n'.join(
        map(str.strip,
            PolicyDetails._RemovePlaceholders(policy['desc']).splitlines()))
    self.caption = PolicyDetails._RemovePlaceholders(policy['caption'])
    self.max_size = policy.get('max_size', 0)

    items = policy.get('items')
    if items is None:
      self.items = None
    else:
      self.items = [ PolicyDetails.EnumItem(entry) for entry in items ]

  PH_PATTERN = re.compile('<ph[^>]*>([^<]*|[^<]*<ex>([^<]*)</ex>[^<]*)</ph>')

  # Simplistic grit placeholder stripper.
  @staticmethod
  def _RemovePlaceholders(text):
    result = ''
    pos = 0
    for m in PolicyDetails.PH_PATTERN.finditer(text):
      result += text[pos:m.start(0)]
      result += m.group(2) or m.group(1)
      pos = m.end(0)
    result += text[pos:]
    return result


def main():
  parser = OptionParser(usage=__doc__)
  parser.add_option('--pch', '--policy-constants-header', dest='header_path',
                    help='generate header file of policy constants',
                    metavar='FILE')
  parser.add_option('--pcc', '--policy-constants-source', dest='source_path',
                    help='generate source file of policy constants',
                    metavar='FILE')
  parser.add_option('--cpp', '--cloud-policy-protobuf',
                    dest='cloud_policy_proto_path',
                    help='generate cloud policy protobuf file',
                    metavar='FILE')
  parser.add_option('--csp', '--chrome-settings-protobuf',
                    dest='chrome_settings_proto_path',
                    help='generate chrome settings protobuf file',
                    metavar='FILE')
  parser.add_option('--cpd', '--cloud-policy-decoder',
                    dest='cloud_policy_decoder_path',
                    help='generate C++ code decoding the cloud policy protobuf',
                    metavar='FILE')

  (opts, args) = parser.parse_args()

  if len(args) != 3:
    print 'exactly platform, chromium_os flag and input file must be specified.'
    parser.print_help()
    return 2

  os = args[0]
  is_chromium_os = args[1] == '1'
  template_file_name = args[2]

  template_file_contents = _LoadJSONFile(template_file_name)
  policy_details = [ PolicyDetails(policy, os, is_chromium_os)
                     for policy in _Flatten(template_file_contents) ]
  sorted_policy_details = sorted(policy_details, key=lambda policy: policy.name)

  def GenerateFile(path, writer, sorted=False):
    if path:
      with open(path, 'w') as f:
        _OutputGeneratedWarningHeader(f, template_file_name)
        writer(sorted and sorted_policy_details or policy_details, os, f)

  GenerateFile(opts.header_path, _WritePolicyConstantHeader, sorted=True)
  GenerateFile(opts.source_path, _WritePolicyConstantSource, sorted=True)
  GenerateFile(opts.cloud_policy_proto_path, _WriteCloudPolicyProtobuf)
  GenerateFile(opts.chrome_settings_proto_path, _WriteChromeSettingsProtobuf)
  GenerateFile(opts.cloud_policy_decoder_path, _WriteCloudPolicyDecoder)

  return 0


#------------------ shared helpers ---------------------------------#

def _OutputGeneratedWarningHeader(f, template_file_path):
  f.write('//\n'
          '// DO NOT MODIFY THIS FILE DIRECTLY!\n'
          '// IT IS GENERATED BY generate_policy_source.py\n'
          '// FROM ' + template_file_path + '\n'
          '//\n\n')


COMMENT_WRAPPER = textwrap.TextWrapper()
COMMENT_WRAPPER.width = 80
COMMENT_WRAPPER.initial_indent = '// '
COMMENT_WRAPPER.subsequent_indent = '// '
COMMENT_WRAPPER.replace_whitespace = False


# Writes a comment, each line prefixed by // and wrapped to 80 spaces.
def _OutputComment(f, comment):
  for line in comment.splitlines():
    if len(line) == 0:
      f.write('//')
    else:
      f.write(COMMENT_WRAPPER.fill(line))
    f.write('\n')


# Returns an iterator over all the policies in |template_file_contents|.
def _Flatten(template_file_contents):
  for policy in template_file_contents['policy_definitions']:
    if policy['type'] == 'group':
      for sub_policy in policy['policies']:
        yield sub_policy
    else:
      yield policy


def _LoadJSONFile(json_file):
  with open(json_file, 'r') as f:
    text = f.read()
  return eval(text)


#------------------ policy constants header ------------------------#

def _WritePolicyConstantHeader(policies, os, f):
  f.write('#ifndef CHROME_COMMON_POLICY_CONSTANTS_H_\n'
          '#define CHROME_COMMON_POLICY_CONSTANTS_H_\n'
          '\n'
          '#include <string>\n'
          '\n'
          '#include "base/basictypes.h"\n'
          '#include "base/values.h"\n'
          '#include "components/policy/core/common/policy_details.h"\n'
          '\n'
          'namespace policy {\n'
          '\n'
          'namespace internal {\n'
          'struct SchemaData;\n'
          '}\n\n')

  if os == 'win':
    f.write('// The windows registry path where Chrome policy '
            'configuration resides.\n'
            'extern const wchar_t kRegistryChromePolicyKey[];\n')

  f.write('// Returns the PolicyDetails for |policy| if |policy| is a known\n'
          '// Chrome policy, otherwise returns NULL.\n'
          'const PolicyDetails* GetChromePolicyDetails('
              'const std::string& policy);\n'
          '\n'
          '// Returns the schema data of the Chrome policy schema.\n'
          'const internal::SchemaData* GetChromeSchemaData();\n'
          '\n')
  f.write('// Key names for the policy settings.\n'
          'namespace key {\n\n')
  for policy in policies:
    # TODO(joaodasilva): Include only supported policies in
    # configuration_policy_handler.cc and configuration_policy_handler_list.cc
    # so that these names can be conditional on 'policy.is_supported'.
    # http://crbug.com/223616
    f.write('extern const char k' + policy.name + '[];\n')
  f.write('\n}  // namespace key\n\n'
          '}  // namespace policy\n\n'
          '#endif  // CHROME_COMMON_POLICY_CONSTANTS_H_\n')


#------------------ policy constants source ------------------------#

# A mapping of the simple schema types to base::Value::Types.
SIMPLE_SCHEMA_NAME_MAP = {
  'boolean': 'TYPE_BOOLEAN',
  'integer': 'TYPE_INTEGER',
  'null'   : 'TYPE_NULL',
  'number' : 'TYPE_DOUBLE',
  'string' : 'TYPE_STRING',
}


class SchemaNodesGenerator:
  """Builds the internal structs to represent a JSON schema."""

  def __init__(self, shared_strings):
    """Creates a new generator.

    |shared_strings| is a map of strings to a C expression that evaluates to
    that string at runtime. This mapping can be used to reuse existing string
    constants."""
    self.shared_strings = shared_strings
    self.schema_nodes = []
    self.property_nodes = []
    self.properties_nodes = []
    self.simple_types = {
      'boolean': None,
      'integer': None,
      'null': None,
      'number': None,
      'string': None,
    }
    self.stringlist_type = None

  def GetString(self, s):
    return self.shared_strings[s] if s in self.shared_strings else '"%s"' % s

  def AppendSchema(self, type, extra, comment=''):
    index = len(self.schema_nodes)
    self.schema_nodes.append((type, extra, comment))
    return index

  def GetSimpleType(self, name):
    if self.simple_types[name] == None:
      self.simple_types[name] = self.AppendSchema(
          SIMPLE_SCHEMA_NAME_MAP[name],
          -1,
          'simple type: ' + name)
    return self.simple_types[name]

  def GetStringList(self):
    if self.stringlist_type == None:
      self.stringlist_type = self.AppendSchema(
          'TYPE_LIST',
          self.GetSimpleType('string'),
          'simple type: stringlist')
    return self.stringlist_type

  def Generate(self, schema, name):
    """Generates the structs for the given schema.

    |schema|: a valid JSON schema in a dictionary.
    |name|: the name of the current node, for the generated comments."""
    # Simple types use shared nodes.
    if schema['type'] in self.simple_types:
      return self.GetSimpleType(schema['type'])

    if schema['type'] == 'array':
      # Special case for lists of strings, which is a common policy type.
      if schema['items']['type'] == 'string':
        return self.GetStringList()
      return self.AppendSchema(
          'TYPE_LIST',
          self.Generate(schema['items'], 'items of ' + name))
    elif schema['type'] == 'object':
      # Reserve an index first, so that dictionaries come before their
      # properties. This makes sure that the root node is the first in the
      # SchemaNodes array.
      index = self.AppendSchema('TYPE_DICTIONARY', -1)

      if 'additionalProperties' in schema:
        additionalProperties = self.Generate(
            schema['additionalProperties'],
            'additionalProperties of ' + name)
      else:
        additionalProperties = -1

      # Properties must be sorted by name, for the binary search lookup.
      # Note that |properties| must be evaluated immediately, so that all the
      # recursive calls to Generate() append the necessary child nodes; if
      # |properties| were a generator then this wouldn't work.
      sorted_properties = sorted(schema.get('properties', {}).items())
      properties = [ (self.GetString(key), self.Generate(schema, key))
                     for key, schema in sorted_properties ]
      begin = len(self.property_nodes)
      self.property_nodes += properties
      end = len(self.property_nodes)
      if index == 0:
        self.root_properties_begin = begin
        self.root_properties_end = end

      extra = len(self.properties_nodes)
      self.properties_nodes.append((begin, end, additionalProperties, name))

      # Set the right data at |index| now.
      self.schema_nodes[index] = ('TYPE_DICTIONARY', extra, name)
      return index
    else:
      assert False

  def Write(self, f):
    """Writes the generated structs to the given file.

    |f| an open file to write to."""
    f.write('const internal::SchemaNode kSchemas[] = {\n'
            '//  Type                          Extra\n')
    for type, extra, comment in self.schema_nodes:
      type += ','
      f.write('  { base::Value::%-18s %3d },  // %s\n' % (type, extra, comment))
    f.write('};\n\n')

    f.write('const internal::PropertyNode kPropertyNodes[] = {\n'
            '//  Property                                           #Schema\n')
    for key, schema in self.property_nodes:
      key += ','
      f.write('  { %-50s %7d },\n' % (key, schema))
    f.write('};\n\n')

    f.write('const internal::PropertiesNode kProperties[] = {\n'
            '//  Begin    End  Additional Properties\n')
    for node in self.properties_nodes:
      f.write('  { %5d, %5d, %5d },  // %s\n' % node)
    f.write('};\n\n')

    f.write('const internal::SchemaData kChromeSchemaData = {\n'
            '  kSchemas,\n'
            '  kPropertyNodes,\n'
            '  kProperties,\n'
            '};\n\n')


def _WritePolicyConstantSource(policies, os, f):
  f.write('#include "policy/policy_constants.h"\n'
          '\n'
          '#include <algorithm>\n'
          '\n'
          '#include "base/logging.h"\n'
          '#include "components/policy/core/common/schema_internal.h"\n'
          '\n'
          'namespace policy {\n'
          '\n'
          'namespace {\n'
          '\n')

  # Generate the Chrome schema.
  chrome_schema = {
    'type': 'object',
    'properties': {},
  }
  shared_strings = {}
  for policy in policies:
    shared_strings[policy.name] = "key::k%s" % policy.name
    if policy.is_supported:
      chrome_schema['properties'][policy.name] = policy.schema

  # Note: this list must be kept in sync with the known property list of the
  # Chrome schema, so that binary seaching in the PropertyNode array gets the
  # right index on this array as well. See the implementation of
  # GetChromePolicyDetails() below.
  f.write('const PolicyDetails kChromePolicyDetails[] = {\n'
          '//  is_deprecated  is_device_policy  id    max_external_data_size\n')
  for policy in policies:
    if policy.is_supported:
      f.write('  { %-14s %-16s %3s, %24s },\n' % (
                  'true,' if policy.is_deprecated else 'false,',
                  'true,' if policy.is_device_only else 'false,',
                  policy.id,
                  policy.max_size))
  f.write('};\n\n')

  schema_generator = SchemaNodesGenerator(shared_strings)
  schema_generator.Generate(chrome_schema, 'root node')
  schema_generator.Write(f)

  f.write('bool CompareKeys(const internal::PropertyNode& node,\n'
          '                 const std::string& key) {\n'
          '  return node.key < key;\n'
          '}\n\n')

  f.write('}  // namespace\n\n')

  if os == 'win':
    f.write('#if defined(GOOGLE_CHROME_BUILD)\n'
            'const wchar_t kRegistryChromePolicyKey[] = '
            'L"' + CHROME_POLICY_KEY + '";\n'
            '#else\n'
            'const wchar_t kRegistryChromePolicyKey[] = '
            'L"' + CHROMIUM_POLICY_KEY + '";\n'
            '#endif\n\n')

  f.write('const internal::SchemaData* GetChromeSchemaData() {\n'
          '  return &kChromeSchemaData;\n'
          '}\n\n')

  f.write('const PolicyDetails* GetChromePolicyDetails('
              'const std::string& policy) {\n'
          '  // First index in kPropertyNodes of the Chrome policies.\n'
          '  static const int begin_index = %s;\n'
          '  // One-past-the-end of the Chrome policies in kPropertyNodes.\n'
          '  static const int end_index = %s;\n' %
          (schema_generator.root_properties_begin,
           schema_generator.root_properties_end))
  f.write('  const internal::PropertyNode* begin =\n'
          '      kPropertyNodes + begin_index;\n'
          '  const internal::PropertyNode* end = kPropertyNodes + end_index;\n'
          '  const internal::PropertyNode* it =\n'
          '      std::lower_bound(begin, end, policy, CompareKeys);\n'
          '  if (it == end || it->key != policy)\n'
          '    return NULL;\n'
          '  // This relies on kPropertyNodes from begin_index to end_index\n'
          '  // having exactly the same policies (and in the same order) as\n'
          '  // kChromePolicyDetails, so that binary searching on the first\n'
          '  // gets the same results as a binary search on the second would.\n'
          '  // However, kPropertyNodes has the policy names and\n'
          '  // kChromePolicyDetails doesn\'t, so we obtain the index into\n'
          '  // the second array by searching the first to avoid duplicating\n'
          '  // the policy name pointers.\n'
          '  // Offsetting |it| from |begin| here obtains the index we\'re\n'
          '  // looking for.\n'
          '  size_t index = it - begin;\n'
          '  CHECK_LT(index, arraysize(kChromePolicyDetails));\n'
          '  return kChromePolicyDetails + index;\n'
          '}\n\n')

  f.write('namespace key {\n\n')
  for policy in policies:
    # TODO(joaodasilva): Include only supported policies in
    # configuration_policy_handler.cc and configuration_policy_handler_list.cc
    # so that these names can be conditional on 'policy.is_supported'.
    # http://crbug.com/223616
    f.write('const char k{name}[] = "{name}";\n'.format(name=policy.name))
  f.write('\n}  // namespace key\n\n'
          '}  // namespace policy\n')


#------------------ policy protobufs --------------------------------#

CHROME_SETTINGS_PROTO_HEAD = '''
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package enterprise_management;

// For StringList and PolicyOptions.
import "cloud_policy.proto";

'''


CLOUD_POLICY_PROTO_HEAD = '''
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package enterprise_management;

message StringList {
  repeated string entries = 1;
}

message PolicyOptions {
  enum PolicyMode {
    // The given settings are applied regardless of user choice.
    MANDATORY = 0;
    // The user may choose to override the given settings.
    RECOMMENDED = 1;
    // No policy value is present and the policy should be ignored.
    UNSET = 2;
  }
  optional PolicyMode mode = 1 [default = MANDATORY];
}

message BooleanPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool value = 2;
}

message IntegerPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional int64 value = 2;
}

message StringPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string value = 2;
}

message StringListPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional StringList value = 2;
}

'''


# Field IDs [1..RESERVED_IDS] will not be used in the wrapping protobuf.
RESERVED_IDS = 2


def _WritePolicyProto(f, policy, fields):
  _OutputComment(f, policy.caption + '\n\n' + policy.desc)
  if policy.items is not None:
    _OutputComment(f, '\nValid values:')
    for item in policy.items:
      _OutputComment(f, '  %s: %s' % (str(item.value), item.caption))
  if policy.policy_type == 'TYPE_DICTIONARY':
    _OutputComment(f, '\nValue schema:\n%s' %
                   json.dumps(policy.schema, sort_keys=True, indent=4,
                              separators=(',', ': ')))
  _OutputComment(f, '\nSupported on: %s' % ', '.join(policy.platforms))
  f.write('message %sProto {\n' % policy.name)
  f.write('  optional PolicyOptions policy_options = 1;\n')
  f.write('  optional %s %s = 2;\n' % (policy.protobuf_type, policy.name))
  f.write('}\n\n')
  fields += [ '  optional %sProto %s = %s;\n' %
              (policy.name, policy.name, policy.id + RESERVED_IDS) ]


def _WriteChromeSettingsProtobuf(policies, os, f):
  f.write(CHROME_SETTINGS_PROTO_HEAD)

  fields = []
  f.write('// PBs for individual settings.\n\n')
  for policy in policies:
    # Note: this protobuf also gets the unsupported policies, since it's an
    # exaustive list of all the supported user policies on any platform.
    if not policy.is_device_only:
      _WritePolicyProto(f, policy, fields)

  f.write('// --------------------------------------------------\n'
          '// Big wrapper PB containing the above groups.\n\n'
          'message ChromeSettingsProto {\n')
  f.write(''.join(fields))
  f.write('}\n\n')


def _WriteCloudPolicyProtobuf(policies, os, f):
  f.write(CLOUD_POLICY_PROTO_HEAD)
  f.write('message CloudPolicySettings {\n')
  for policy in policies:
    if policy.is_supported and not policy.is_device_only:
      f.write('  optional %sPolicyProto %s = %s;\n' %
              (policy.policy_protobuf_type, policy.name,
               policy.id + RESERVED_IDS))
  f.write('}\n\n')


#------------------ protobuf decoder -------------------------------#

CPP_HEAD = '''
#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "policy/policy_constants.h"
#include "policy/proto/cloud_policy.pb.h"

using google::protobuf::RepeatedPtrField;

namespace policy {

namespace em = enterprise_management;

base::Value* DecodeIntegerValue(google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return NULL;
  }

  return base::Value::CreateIntegerValue(static_cast<int>(value));
}

base::ListValue* DecodeStringList(const em::StringList& string_list) {
  base::ListValue* list_value = new base::ListValue;
  RepeatedPtrField<std::string>::const_iterator entry;
  for (entry = string_list.entries().begin();
       entry != string_list.entries().end(); ++entry) {
    list_value->Append(base::Value::CreateStringValue(*entry));
  }
  return list_value;
}

base::Value* DecodeJson(const std::string& json) {
  scoped_ptr<base::Value> root(
      base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS));

  if (!root)
    LOG(WARNING) << "Invalid JSON string, ignoring: " << json;

  // Accept any Value type that parsed as JSON, and leave it to the handler to
  // convert and check the concrete type.
  return root.release();
}

void DecodePolicy(const em::CloudPolicySettings& policy,
                  base::WeakPtr<CloudExternalDataManager> external_data_manager,
                  PolicyMap* map) {
'''


CPP_FOOT = '''}

}  // namespace policy
'''


def _CreateValue(type, arg):
  if type == 'TYPE_BOOLEAN':
    return 'base::Value::CreateBooleanValue(%s)' % arg
  elif type == 'TYPE_INTEGER':
    return 'DecodeIntegerValue(%s)' % arg
  elif type == 'TYPE_STRING':
    return 'base::Value::CreateStringValue(%s)' % arg
  elif type == 'TYPE_LIST':
    return 'DecodeStringList(%s)' % arg
  elif type == 'TYPE_DICTIONARY' or type == 'TYPE_EXTERNAL':
    return 'DecodeJson(%s)' % arg
  else:
    raise NotImplementedError('Unknown type %s' % type)


def _CreateExternalDataFetcher(type, name):
  if type == 'TYPE_EXTERNAL':
    return 'new ExternalDataFetcher(external_data_manager, key::k%s)' % name
  return 'NULL'


def _WritePolicyCode(f, policy):
  membername = policy.name.lower()
  proto_type = '%sPolicyProto' % policy.policy_protobuf_type
  f.write('  if (policy.has_%s()) {\n' % membername)
  f.write('    const em::%s& policy_proto = policy.%s();\n' %
          (proto_type, membername))
  f.write('    if (policy_proto.has_value()) {\n')
  f.write('      PolicyLevel level = POLICY_LEVEL_MANDATORY;\n'
          '      bool do_set = true;\n'
          '      if (policy_proto.has_policy_options()) {\n'
          '        do_set = false;\n'
          '        switch(policy_proto.policy_options().mode()) {\n'
          '          case em::PolicyOptions::MANDATORY:\n'
          '            do_set = true;\n'
          '            level = POLICY_LEVEL_MANDATORY;\n'
          '            break;\n'
          '          case em::PolicyOptions::RECOMMENDED:\n'
          '            do_set = true;\n'
          '            level = POLICY_LEVEL_RECOMMENDED;\n'
          '            break;\n'
          '          case em::PolicyOptions::UNSET:\n'
          '            break;\n'
          '        }\n'
          '      }\n'
          '      if (do_set) {\n')
  f.write('        base::Value* value = %s;\n' %
          (_CreateValue(policy.policy_type, 'policy_proto.value()')))
  # TODO(bartfab): |value| == NULL indicates that the policy value could not be
  # parsed successfully. Surface such errors in the UI.
  f.write('        if (value) {\n')
  f.write('          ExternalDataFetcher* external_data_fetcher = %s;\n' %
          _CreateExternalDataFetcher(policy.policy_type, policy.name))
  f.write('          map->Set(key::k%s, level, POLICY_SCOPE_USER,\n' %
          policy.name)
  f.write('                   value, external_data_fetcher);\n'
          '        }\n'
          '      }\n'
          '    }\n'
          '  }\n')


def _WriteCloudPolicyDecoder(policies, os, f):
  f.write(CPP_HEAD)
  for policy in policies:
    if policy.is_supported and not policy.is_device_only:
      _WritePolicyCode(f, policy)
  f.write(CPP_FOOT)


if __name__ == '__main__':
  sys.exit(main())
