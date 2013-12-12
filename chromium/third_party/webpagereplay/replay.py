#!/usr/bin/env python
# Copyright 2010 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Replays web pages under simulated network conditions.

Must be run as administrator (sudo).

To record web pages:
  1. Start the program in record mode.
     $ sudo ./replay.py --record archive.wpr
  2. Load the web pages you want to record in a web browser. It is important to
     clear browser caches before this so that all subresources are requested
     from the network.
  3. Kill the process to stop recording.

To replay web pages:
  1. Start the program in replay mode with a previously recorded archive.
     $ sudo ./replay.py archive.wpr
  2. Load recorded pages in a web browser. A 404 will be served for any pages or
     resources not in the recorded archive.

Network simulation examples:
  # 128KByte/s uplink bandwidth, 4Mbps/s downlink bandwidth with 100ms RTT time
  $ sudo ./replay.py --up 128KByte/s --down 4Mbit/s --delay_ms=100 archive.wpr

  # 1% packet loss rate
  $ sudo ./replay.py --packet_loss_rate=0.01 archive.wpr
"""

import json
import logging
import optparse
import os
import socket
import sys
import traceback

import cachemissarchive
import customhandlers
import dnsproxy
import httparchive
import httpclient
import httpproxy
import platformsettings
import replayspdyserver
import script_injector
import servermanager
import trafficshaper

if sys.version < '2.6':
  print 'Need Python 2.6 or greater.'
  sys.exit(1)


def configure_logging(log_level_name, log_file_name=None):
  """Configure logging level and format.

  Args:
    log_level_name: 'debug', 'info', 'warning', 'error', or 'critical'.
    log_file_name: a file name
  """
  if logging.root.handlers:
    logging.critical('A logging method (e.g. "logging.warn(...)")'
                     ' was called before logging was configured.')
  log_level = getattr(logging, log_level_name.upper())
  log_format = '%(asctime)s %(levelname)s %(message)s'
  logging.basicConfig(level=log_level, format=log_format)
  logger = logging.getLogger()
  if log_file_name:
    fh = logging.FileHandler(log_file_name)
    fh.setLevel(log_level)
    fh.setFormatter(logging.Formatter(log_format))
    logger.addHandler(fh)
  system_handler = platformsettings.get_system_logging_handler()
  if system_handler:
    logger.addHandler(system_handler)


def AddDnsForward(server_manager, host):
  """Forward DNS traffic."""
  server_manager.Append(platformsettings.set_temporary_primary_nameserver, host)


def AddDnsProxy(server_manager, options, host, real_dns_lookup, http_archive):
  dns_filters = []
  if options.dns_private_passthrough:
    private_filter = dnsproxy.PrivateIpFilter(real_dns_lookup, http_archive)
    dns_filters.append(private_filter)
    server_manager.AppendRecordCallback(private_filter.InitializeArchiveHosts)
    server_manager.AppendReplayCallback(private_filter.InitializeArchiveHosts)
  if options.shaping_dns:
    delay_filter = dnsproxy.DelayFilter(options.record, **options.shaping_dns)
    dns_filters.append(delay_filter)
    server_manager.AppendRecordCallback(delay_filter.SetRecordMode)
    server_manager.AppendReplayCallback(delay_filter.SetReplayMode)
  server_manager.Append(dnsproxy.DnsProxyServer, host,
                        dns_lookup=dnsproxy.ReplayDnsLookup(host, dns_filters))


def AddWebProxy(server_manager, options, host, real_dns_lookup, http_archive,
                cache_misses):
  inject_script = script_injector.GetInjectScript(options.inject_scripts)
  custom_handlers = customhandlers.CustomHandlers(options, http_archive)
  if options.spdy:
    assert not options.record, 'spdy cannot be used with --record.'
    archive_fetch = httpclient.ReplayHttpArchiveFetch(
        http_archive, real_dns_lookup,
        inject_script,
        options.diff_unknown_requests,
        cache_misses=cache_misses,
        use_closest_match=options.use_closest_match)
    server_manager.Append(
        replayspdyserver.ReplaySpdyServer, archive_fetch,
        custom_handlers, host=host, port=options.port,
        certfile=options.certfile)
  else:
    custom_handlers.add_server_manager_handler(server_manager)
    archive_fetch = httpclient.ControllableHttpArchiveFetch(
        http_archive, real_dns_lookup,
        inject_script,
        options.diff_unknown_requests, options.record,
        cache_misses=cache_misses, use_closest_match=options.use_closest_match)
    server_manager.AppendRecordCallback(archive_fetch.SetRecordMode)
    server_manager.AppendReplayCallback(archive_fetch.SetReplayMode)
    server_manager.Append(
        httpproxy.HttpProxyServer,
        archive_fetch, custom_handlers,
        host=host, port=options.port, **options.shaping_http)
    if options.ssl:
      server_manager.Append(
          httpproxy.HttpsProxyServer,
          archive_fetch, custom_handlers, options.certfile,
          host=host, port=options.ssl_port, **options.shaping_http)


def AddTrafficShaper(server_manager, options, host):
  if options.shaping_dummynet:
    ssl_port = options.ssl_shaping_port if options.ssl else None
    kwargs = dict(
        host=host, port=options.shaping_port, ssl_port=ssl_port,
        use_loopback=not options.server_mode and host == '127.0.0.1',
        **options.shaping_dummynet)
    if not options.dns_forwarding:
      kwargs['dns_port'] = None
    server_manager.Append(trafficshaper.TrafficShaper, **kwargs)


class OptionsWrapper(object):
  """Add checks, updates, and methods to option values.

  Example:
    options, args = option_parser.parse_args()
    options = OptionsWrapper(options, option_parser)  # run checks and updates
    if options.record and options.HasTrafficShaping():
       [...]
  """
  _TRAFFICSHAPING_OPTIONS = set(
      ['down', 'up', 'delay_ms', 'packet_loss_rate', 'init_cwnd', 'net'])
  _CONFLICTING_OPTIONS = (
      ('record', ('down', 'up', 'delay_ms', 'packet_loss_rate', 'net',
                  'spdy', 'use_server_delay')),
      ('append', ('down', 'up', 'delay_ms', 'packet_loss_rate', 'net',
                  'spdy', 'use_server_delay')),  # same as --record
      ('net', ('down', 'up', 'delay_ms')),
      ('server', ('server_mode',)),
  )
  # The --net values come from http://www.webpagetest.org/.
  # https://sites.google.com/a/webpagetest.org/docs/other-resources/2011-fcc-broadband-data
  _NET_CONFIGS = (
      # key           --down         --up  --delay_ms
      ('dsl',   ('1536Kbit/s', '384Kbit/s',       '50')),
      ('cable', (   '5Mbit/s',   '1Mbit/s',       '28')),
      ('fios',  (  '20Mbit/s',   '5Mbit/s',        '4')),
  )
  NET_CHOICES = [key for key, values in _NET_CONFIGS]

  def __init__(self, options, parser):
    self._options = options
    self._parser = parser
    self._nondefaults = set([
        name for name, value in parser.defaults.items()
        if getattr(options, name) != value])
    self._CheckConflicts()
    self._CheckValidIp('host')
    self._MassageValues()

  def _CheckConflicts(self):
    """Give an error if mutually exclusive options are used."""
    for option, bad_options in self._CONFLICTING_OPTIONS:
      if option in self._nondefaults:
        for bad_option in bad_options:
          if bad_option in self._nondefaults:
            self._parser.error('Option --%s cannot be used with --%s.' %
                                (bad_option, option))

  def _CheckValidIp(self, name):
    """Give an error if option |name| is not a valid IPv4 address."""
    value = getattr(self._options, name)
    if value:
      try:
        socket.inet_aton(value)
      except:
        self._parser.error('Option --%s must be a valid IPv4 address.' % name)

  def _ShapingKeywordArgs(self, shaping_key):
    """Return the shaping keyword args for |shaping_key|.

    Args:
      shaping_key: one of 'dummynet', 'dns', 'http'.
    Returns:
      {}  # if shaping_key does not apply, or options have default values.
      {k: v, ...}
    """
    kwargs = {}
    def AddItemIfSet(d, kw_key, opt_key=None):
      opt_key = opt_key or kw_key
      if opt_key in self._nondefaults:
        d[kw_key] = getattr(self, opt_key)
    if ((self.shaping_type == 'proxy' and shaping_key in ('dns', 'http')) or
        self.shaping_type == shaping_key):
      AddItemIfSet(kwargs, 'delay_ms')
      if shaping_key in ('dummynet', 'http'):
        AddItemIfSet(kwargs, 'down_bandwidth', opt_key='down')
        AddItemIfSet(kwargs, 'up_bandwidth', opt_key='up')
        if shaping_key == 'dummynet':
          AddItemIfSet(kwargs, 'packet_loss_rate')
          AddItemIfSet(kwargs, 'init_cwnd')
        elif self.shaping_type != 'none':
          if 'packet_loss_rate' in self._nondefaults:
            logging.warn('Shaping type, %s, ignores --packet_loss_rate=%s',
                         self.shaping_type, self.packet_loss_rate)
          if 'init_cwnd' in self._nondefaults:
            logging.warn('Shaping type, %s, ignores --init_cwnd=%s',
                         self.shaping_type, self.init_cwnd)
    return kwargs

  def _MassageValues(self):
    """Set options that depend on the values of other options."""
    if self.append and not self.record:
      self._options.record = True
    for net_choice, values in self._NET_CONFIGS:
      if net_choice == self.net:
        self._options.down, self._options.up, self._options.delay_ms = values
        self._nondefaults.update(['down', 'up', 'delay_ms'])
    if not self.shaping_port:
      self._options.shaping_port = self.port
    if not self.ssl_shaping_port:
      self._options.ssl_shaping_port = self.ssl_port
    if not self.ssl:
      self._options.certfile = None
    self.shaping_dns = self._ShapingKeywordArgs('dns')
    self.shaping_http = self._ShapingKeywordArgs('http')
    self.shaping_dummynet = self._ShapingKeywordArgs('dummynet')

  def __getattr__(self, name):
    """Make the original option values available."""
    return getattr(self._options, name)

  def __repr__(self):
    """Return a json representation of the original options dictionary."""
    return json.dumps(self._options.__dict__)

  def IsRootRequired(self):
    """Returns True iff the options require root access."""
    return (self.shaping_dummynet or
            self.dns_forwarding or
            self.port < 1024 or
            self.ssl_port < 1024) and self.admin_check


def replay(options, replay_filename):
  if options.IsRootRequired():
    platformsettings.rerun_as_administrator()
  configure_logging(options.log_level, options.log_file)
  server_manager = servermanager.ServerManager(options.record)
  cache_misses = None
  if options.cache_miss_file:
    if os.path.exists(options.cache_miss_file):
      logging.warning('Cache Miss Archive file %s already exists; '
                      'replay will load and append entries to archive file',
                      options.cache_miss_file)
      cache_misses = cachemissarchive.CacheMissArchive.Load(
          options.cache_miss_file)
    else:
      cache_misses = cachemissarchive.CacheMissArchive(
          options.cache_miss_file)
  if options.server:
    AddDnsForward(server_manager, options.server)
  else:
    host = options.host
    if not host:
      host = platformsettings.get_server_ip_address(options.server_mode)
    real_dns_lookup = dnsproxy.RealDnsLookup(
        name_servers=[platformsettings.get_original_primary_nameserver()])
    if options.record:
      httparchive.HttpArchive.AssertWritable(replay_filename)
      if options.append and os.path.exists(replay_filename):
        http_archive = httparchive.HttpArchive.Load(replay_filename)
        logging.info('Appending to %s (loaded %d existing responses)',
                     replay_filename, len(http_archive))
      else:
        http_archive = httparchive.HttpArchive()
    else:
      http_archive = httparchive.HttpArchive.Load(replay_filename)
      logging.info('Loaded %d responses from %s',
                   len(http_archive), replay_filename)
    server_manager.AppendRecordCallback(real_dns_lookup.ClearCache)
    server_manager.AppendRecordCallback(http_archive.clear)

    if options.dns_forwarding:
      if not options.server_mode and host == '127.0.0.1':
        AddDnsForward(server_manager, host)
      AddDnsProxy(server_manager, options, host, real_dns_lookup, http_archive)
    if options.ssl and options.certfile is None:
      options.certfile = os.path.join(os.path.dirname(__file__), 'wpr_cert.pem')
    http_proxy_address = options.host
    if not http_proxy_address:
      http_proxy_address = platformsettings.get_httpproxy_ip_address(
          options.server_mode)
    AddWebProxy(server_manager, options, http_proxy_address, real_dns_lookup,
                http_archive, cache_misses)
    AddTrafficShaper(server_manager, options, host)

  exit_status = 0
  try:
    server_manager.Run()
  except KeyboardInterrupt:
    logging.info('Shutting down.')
  except (dnsproxy.DnsProxyException,
          trafficshaper.TrafficShaperException,
          platformsettings.NotAdministratorError,
          platformsettings.DnsUpdateError) as e:
    logging.critical('%s: %s', e.__class__.__name__, e)
    exit_status = 1
  except:
    logging.critical(traceback.format_exc())
    exit_status = 2

  if options.record:
    http_archive.Persist(replay_filename)
    logging.info('Saved %d responses to %s', len(http_archive), replay_filename)
  if cache_misses:
    cache_misses.Persist()
    logging.info('Saved %d cache misses and %d requests to %s',
                 cache_misses.get_total_cache_misses(),
                 len(cache_misses.request_counts.keys()),
                 options.cache_miss_file)
  return exit_status


def GetOptionParser():
  class PlainHelpFormatter(optparse.IndentedHelpFormatter):
    def format_description(self, description):
      if description:
        return description + '\n'
      else:
        return ''
  option_parser = optparse.OptionParser(
      usage='%prog [options] replay_file',
      formatter=PlainHelpFormatter(),
      description=__doc__,
      epilog='http://code.google.com/p/web-page-replay/')

  option_parser.add_option('--spdy', default=False,
      action='store_true',
      help='Replay via SPDY. (Can be combined with --no-ssl).')
  option_parser.add_option('-r', '--record', default=False,
      action='store_true',
      help='Download real responses and record them to replay_file')
  option_parser.add_option('--append', default=False,
      action='store_true',
      help='Append responses to replay_file.')
  option_parser.add_option('-l', '--log_level', default='debug',
      action='store',
      type='choice',
      choices=('debug', 'info', 'warning', 'error', 'critical'),
      help='Minimum verbosity level to log')
  option_parser.add_option('-f', '--log_file', default=None,
      action='store',
      type='string',
      help='Log file to use in addition to writting logs to stderr.')
  option_parser.add_option('-e', '--cache_miss_file', default=None,
      action='store',
      dest='cache_miss_file',
      type='string',
      help='Archive file to record cache misses as pickled objects.'
           'Cache misses occur when a request cannot be served in replay mode.')

  network_group = optparse.OptionGroup(option_parser,
      'Network Simulation Options',
      'These options configure the network simulation in replay mode')
  network_group.add_option('-u', '--up', default='0',
      action='store',
      type='string',
      help='Upload Bandwidth in [K|M]{bit/s|Byte/s}. Zero means unlimited.')
  network_group.add_option('-d', '--down', default='0',
      action='store',
      type='string',
      help='Download Bandwidth in [K|M]{bit/s|Byte/s}. Zero means unlimited.')
  network_group.add_option('-m', '--delay_ms', default='0',
      action='store',
      type='string',
      help='Propagation delay (latency) in milliseconds. Zero means no delay.')
  network_group.add_option('-p', '--packet_loss_rate', default='0',
      action='store',
      type='string',
      help='Packet loss rate in range [0..1]. Zero means no loss.')
  network_group.add_option('-w', '--init_cwnd', default='0',
      action='store',
      type='string',
      help='Set initial cwnd (linux only, requires kernel patch)')
  network_group.add_option('--net', default=None,
      action='store',
      type='choice',
      choices=OptionsWrapper.NET_CHOICES,
      help='Select a set of network options: %s.' % ', '.join(
          OptionsWrapper.NET_CHOICES))
  network_group.add_option('--shaping_type', default='dummynet',
      action='store',
      choices=('dummynet', 'proxy'),
      help='When shaping is configured (i.e. --up, --down, etc.) decides '
           'whether to use |dummynet| (default), or |proxy| servers.')
  option_parser.add_option_group(network_group)

  harness_group = optparse.OptionGroup(option_parser,
      'Replay Harness Options',
      'These advanced options configure various aspects of the replay harness')
  harness_group.add_option('-S', '--server', default=None,
      action='store',
      type='string',
      help='IP address of host running "replay.py --server_mode". '
           'This only changes the primary DNS nameserver to use the given IP.')
  harness_group.add_option('-M', '--server_mode', default=False,
      action='store_true',
      help='Run replay DNS & http proxies, and trafficshaping on --port '
           'without changing the primary DNS nameserver. '
           'Other hosts may connect to this using "replay.py --server" '
           'or by pointing their DNS to this server.')
  harness_group.add_option('-i', '--inject_scripts', default='deterministic.js',
      action='store',
      dest='inject_scripts',
      help='A comma separated list of JavaScript sources to inject in all '
           'pages. By default a script is injected that eliminates sources '
           'of entropy such as Date() and Math.random() deterministic. '
           'CAUTION: Without deterministic.js, many pages will not replay.')
  harness_group.add_option('-D', '--no-diff_unknown_requests', default=True,
      action='store_false',
      dest='diff_unknown_requests',
      help='During replay, do not show a diff of unknown requests against '
           'their nearest match in the archive.')
  harness_group.add_option('-C', '--use_closest_match', default=False,
      action='store_true',
      dest='use_closest_match',
      help='During replay, if a request is not found, serve the closest match'
           'in the archive instead of giving a 404.')
  harness_group.add_option('-U', '--use_server_delay', default=False,
      action='store_true',
      dest='use_server_delay',
      help='During replay, simulate server delay by delaying response time to'
           'requests.')
  harness_group.add_option('-I', '--screenshot_dir', default=None,
      action='store',
      type='string',
      help='Save PNG images of the loaded page in the given directory.')
  harness_group.add_option('-P', '--no-dns_private_passthrough', default=True,
      action='store_false',
      dest='dns_private_passthrough',
      help='Don\'t forward DNS requests that resolve to private network '
           'addresses. CAUTION: With this option important services like '
           'Kerberos will resolve to the HTTP proxy address.')
  harness_group.add_option('-x', '--no-dns_forwarding', default=True,
      action='store_false',
      dest='dns_forwarding',
      help='Don\'t forward DNS requests to the local replay server. '
           'CAUTION: With this option an external mechanism must be used to '
           'forward traffic to the replay server.')
  harness_group.add_option('--host', default=None,
      action='store',
      type='str',
      help='The IP address to bind all servers to. Defaults to 0.0.0.0 or '
           '127.0.0.1, depending on --server_mode and platform.')
  harness_group.add_option('-o', '--port', default=80,
      action='store',
      type='int',
      help='Port number to listen on.')
  harness_group.add_option('--ssl_port', default=443,
      action='store',
      type='int',
      help='SSL port number to listen on.')
  harness_group.add_option('--shaping_port', default=None,
      action='store',
      type='int',
      help='Port on which to apply traffic shaping.  Defaults to the '
           'listen port (--port)')
  harness_group.add_option('--ssl_shaping_port', default=None,
      action='store',
      type='int',
      help='SSL port on which to apply traffic shaping.  Defaults to the '
           'SSL listen port (--ssl_port)')
  harness_group.add_option('-c', '--certfile', default=None,
      action='store',
      type='string',
      help='Certificate file to use with SSL (gets auto-generated if needed).')
  harness_group.add_option('--no-ssl', default=True,
      action='store_false',
      dest='ssl',
      help='Do not setup an SSL proxy.')
  option_parser.add_option_group(harness_group)
  harness_group.add_option('--no-admin-check', default=True,
      action='store_false',
      dest='admin_check',
      help='Do not check if administrator access is needed.')
  return option_parser


def main():
  option_parser = GetOptionParser()
  options, args = option_parser.parse_args()
  options = OptionsWrapper(options, option_parser)

  if options.server:
    replay_filename = None
  elif len(args) != 1:
    option_parser.error('Must specify a replay_file')
  else:
    replay_filename = args[0]

  return replay(options, replay_filename)


if __name__ == '__main__':
  sys.exit(main())
