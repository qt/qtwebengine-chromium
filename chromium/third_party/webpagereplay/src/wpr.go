// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Program wpr records and replays web traffic.
package main

import (
	"bytes"
	"crypto/tls"
	"errors"
	"fmt"
	"math"
	"net"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/urfave/cli/v2"
	"go.chromium.org/webpagereplay/src/webpagereplay"
	"golang.org/x/net/http2"
)

var Log = webpagereplay.Log

const longUsage = `
   %s [installroot|removeroot] [options]
   %s [record|replay] [options] archive_file

   Before: Install a test root CA.
     $ GOPATH=$PWD go run src/wpr.go installroot

   To record web pages:
     1. Start this program in record mode.
        $ GOPATH=$PWD go run src/wpr.go record archive.json
     2. Load the web pages you want to record in a web browser. It is important to
        clear browser caches before this so that all subresources are requested
        from the network.
     3. Kill the process to stop recording.

   To replay web pages:
     1. Start this program in replay mode with a previously recorded archive.
        $ GOPATH=$PWD go run src/wpr.go replay archive.json
     2. Load recorded pages in a web browser. A 404 will be served for any pages or
        resources not in the recorded archive.

   After: Remove the test root CA.
     $ GOPATH=$PWD go run src/wpr.go removeroot`

type CertConfig struct {
	// Flags common to all commands.
	certFile, keyFile, certType string
}

type CommonConfig struct {
	// Info about this command.
	cmd cli.Command

	// Flags common to RecordCommand and ReplayCommand.
	host                                     string
	httpPort, httpsPort, httpSecureProxyPort int
	logLevel                                 string
	relativeTimestamps                       bool
	certConfig                               CertConfig
	injectScripts                            string
	paramToIgnoreInURLPath                   string
	noArchiveCertificates                    bool
	constantMathRandomResult                 float64
	skipCertLoadingForTesting                bool
	htmlInjection                            bool
	jsInjection                              bool

	// Computed state.
	rootCerts    []tls.Certificate
	transformers []webpagereplay.ResponseTransformer
}

type RecordCommand struct {
	common CommonConfig
	cmd    cli.Command

	// Custom flags for record.
	enableExperimentalTimedChunk bool
}

type ReplayCommand struct {
	common CommonConfig
	cmd    cli.Command

	// Custom flags for replay.
	rulesFile                            string
	serveResponseInChronologicalSequence bool
	quietMode                            bool
	disableFuzzyURLMatching              bool
}

type RootCACommand struct {
	certConfig CertConfig
	installer  webpagereplay.Installer
	cmd        cli.Command
}

func (certCfg *CertConfig) Flags() []cli.Flag {
	return []cli.Flag{
		&cli.StringFlag{
			Name:        "https-cert-file",
			Value:       "",
			Usage:       "File containing 1 or more comma separated PEM-encoded X509 certificates to use with SSL.",
			Destination: &certCfg.certFile,
		},
		&cli.StringFlag{
			Name:        "https-key-file",
			Value:       "",
			Usage:       "File containing 1 or more comma separated PEM-encoded private keys to use with SSL.",
			Destination: &certCfg.keyFile,
		},
	}
}

func (common *CommonConfig) Flags() []cli.Flag {
	return append(common.certConfig.Flags(),
		&cli.StringFlag{
			Name:        "host",
			Value:       "localhost",
			Usage:       "IP address to bind all servers to. Defaults to localhost if not specified.",
			Destination: &common.host,
		},
		&cli.IntFlag{
			Name:        "http-port",
			Value:       -1,
			Usage:       "Port number to listen on for HTTP requests, 0 to use any port, or -1 to disable.",
			Destination: &common.httpPort,
		},
		&cli.IntFlag{
			Name:        "https-port",
			Value:       -1,
			Usage:       "Port number to listen on for HTTPS requests, 0 to use any port, or -1 to disable.",
			Destination: &common.httpsPort,
		},
		&cli.IntFlag{
			Name:        "https-to-http-port",
			Value:       -1,
			Usage:       "Port number to listen on for HTTP proxy requests over an HTTPS connection, 0 to use any port, or -1 to disable.",
			Destination: &common.httpSecureProxyPort,
		},
		&cli.StringFlag{
			Name:        "log-level",
			Value:       "INFO",
			Usage:       "Logging level (DEBUG, INFO, WARN, ERROR).",
			Destination: &common.logLevel,
		},
		&cli.BoolFlag{
			Name:        "relative-timestamps",
			Usage:       "Display relative timestamps in logs.",
			Destination: &common.relativeTimestamps,
		},
		&cli.StringFlag{
			Name:  "inject-scripts",
			Value: "deterministic.js",
			Usage: "A comma separated list of JavaScript sources to inject in all pages. " +
				"By default a script is injected that eliminates sources of entropy " +
				"such as Date() and Math.random() deterministic. " +
				"CAUTION: Without deterministic.js, many pages will not replay.",
			Destination: &common.injectScripts,
		},
		&cli.StringFlag{
			Name:  "param-to-ignore-in-url-path",
			Value: "",
			Usage: "When recording and replaying, ignore a specific query parameter in" +
				"a given url path. The input format is \"{Full URL path}::{parameter}\". " +
				"e.g. input \"https://example.com/path::param1\" means WPR would remove " +
				"param1 query key and values from when recording & replaying. Sequence " +
				"of other parameteres are preserved. Only one parameter in one URL path " +
				"is supported.",
			Destination: &common.paramToIgnoreInURLPath,
		},
		&cli.BoolFlag{
			Name: "no-archive-certificates",
			Usage: "By default, WPR stores certificates in the archive during " +
				"recording (minted from the root ones) and reads them during replay " +
				"Such certificates will expire eventually, so this setup is only " +
				"suitable when the client ignores TLS errors (e.g. due to " +
				"--ignore-certificate-errors-spki-list in a Chromium browser), or " +
				"for short-lived experiments. Otherwise, use this flag to prevent " +
				"WPR from reading/writing archive certificates. New certificates " +
				"will be generated on replay time, with caching by host.",
			Destination: &common.noArchiveCertificates,
		},
		&cli.Float64Flag{
			Name: "constant-math-random-result",
			Usage: "A float between 0.0 (inclusive) and 1.0 (exclusive) to use as " +
				"a constant Math.random() result. If not specified, a deterministic " +
				"sequence is used. Note that when using this sequence, invocations " +
				"from a given point in the code might still run into different " +
				"values across runs because the calls to Math.random() might get " +
				"reordered due to external factors.",
			Destination: &common.constantMathRandomResult,
		},
		&cli.BoolFlag{
			Name:        "html-injection",
			Value:       true,
			Usage:       "Inject scripts into HTML responses. Defaults to true.",
			Destination: &common.htmlInjection,
		},
		&cli.BoolFlag{
			Name:        "js-injection",
			Value:       false,
			Usage:       "Inject scripts into JavaScript responses. Defaults to false.",
			Destination: &common.jsInjection,
		},
	)
}

func isSet(c *cli.Context, name string) bool {
	return c.IsSet(name) || c.IsSet(strings.ReplaceAll(name, "-", "_"))
}

func (certCfg *CertConfig) CheckArgs(c *cli.Context) error {
	if certCfg.certFile == "" && certCfg.keyFile == "" {
		certCfg.certFile = "wpr_cert.pem,ecdsa_cert.pem"
		certCfg.keyFile = "wpr_key.pem,ecdsa_key.pem"
	}
	return nil
}

func (common *CommonConfig) CheckArgsAndSetLogLevel(c *cli.Context) error {
	if c.Args().Len() > 1 {
		return errors.New("too many args")
	}
	if c.Args().Len() != 1 {
		return errors.New("must specify archive_file")
	}
	if common.httpPort == -1 && common.httpsPort == -1 && common.httpSecureProxyPort == -1 {
		return errors.New("must specify at least one port flag")
	}

	if err := webpagereplay.SetLogLevel(common.logLevel); err != nil {
		return fmt.Errorf("Invalid log_level (%s): %v", common.logLevel, err)
	}
	webpagereplay.SetRelativeTimestamps(common.relativeTimestamps)

	if isSet(c, "constant-math-random-result") {
		val := common.constantMathRandomResult
		if math.IsNaN(val) || math.IsInf(val, 0) || val < 0.0 || val >= 1.0 {
			return fmt.Errorf("Invalid value (%v) for the flag --%v. "+
				"The permitted range  is [0, 1).",
				val, "constant-math-random-result")
		}
	}

	err := common.certConfig.CheckArgs(c)
	if err != nil {
		return err
	}

	if common.skipCertLoadingForTesting {
		return nil
	}

	// Load certFiles.
	certFiles := strings.Split(common.certConfig.certFile, ",")
	keyFiles := strings.Split(common.certConfig.keyFile, ",")
	if len(certFiles) != len(keyFiles) {
		return fmt.Errorf("list of cert files given should match list of key files")
	}
	for i := 0; i < len(certFiles); i++ {
		Log().Info("Loading cert", "path", certFiles[i])
		Log().Info("Loading key", "path", keyFiles[i])
		rootCert, err := tls.LoadX509KeyPair(certFiles[i], keyFiles[i])
		if err != nil {
			return fmt.Errorf("error opening cert or key files: %v", err)
		}
		common.rootCerts = append(common.rootCerts, rootCert)
	}
	return nil
}

func (common *CommonConfig) ProcessInjectedScriptsForRecording(c *cli.Context,
	archive *webpagereplay.Archive) error {
	// Determine the time seed.
	archive.DeterministicTimeSeedMs = 1000 * time.Now().Unix()

	// Determine the constant Math.random() result, if any.
	if isSet(c, "constant-math-random-result") {
		archive.ConstantMathRandomResult = &common.constantMathRandomResult
	}

	return common.processScripts(archive.InjectedScripts,
		archive.DeterministicTimeSeedMs, archive.ConstantMathRandomResult)
}

func (common *CommonConfig) ProcessInjectedScriptsForReplay(c *cli.Context,
	archive *webpagereplay.Archive) error {
	// Determine the time seed.
	var timeSeedMs int64
	if archive.DeterministicTimeSeedMs != 0 {
		timeSeedMs = archive.DeterministicTimeSeedMs
	} else {
		// Old archive, predating the addition of DeterministicTimeSeedMs.
		timeSeedMs = 1000 * time.Now().Unix()
	}

	var constantMathRandomResult *float64 = archive.ConstantMathRandomResult
	if isSet(c, "constant-math-random-result") {
		flagValue := &common.constantMathRandomResult

		// Warn if archive contains a value that differs what the user specifies.
		if archive.ConstantMathRandomResult != nil &&
			*flagValue != *archive.ConstantMathRandomResult {
			Log().Warn("constant-math-random-result flag differs from archive",
				"flag", *flagValue, "archive", *archive.ConstantMathRandomResult)
		}

		// Still respect the user's wishes.
		constantMathRandomResult = flagValue
	}

	injectArchiveScripts := c.String("inject-archive-scripts") == "true"

	// If the user didn't explicitly request a script, and the archive already
	// contains 'deterministic.js', we skip loading the default 'deterministic.js'
	// from the file system. This avoids duplicate script errors and ensures we
	// use the version stored in the archive, preserving replay fidelity.
	if !isSet(c, "inject-scripts") && common.injectScripts == "deterministic.js" {
		if _, ok := archive.InjectedScripts["deterministic.js"]; ok {
			Log().Info("Archive contains deterministic.js, skipping default injection")
			common.injectScripts = ""
		}
	}

	scriptsMap := make(map[string]string)
	if injectArchiveScripts && len(archive.InjectedScripts) > 0 {
		for name, contents := range archive.InjectedScripts {
			scriptsMap[name] = contents
			replacedContents := replaceConstants(
				name, []byte(contents), timeSeedMs, constantMathRandomResult)
			if err := common.addScriptInjector(replacedContents, name); err != nil {
				return fmt.Errorf("error processing injected script %s: %v", name, err)
			}
		}
	}

	// Process scripts that were added from the archive and/or from the command line using
	// the --inject-scripts flag.
	if err := common.processScripts(scriptsMap, timeSeedMs, constantMathRandomResult); err != nil {
		return err
	}

	// Specified through --rules-file.
	if err := common.processRulesFile(c); err != nil {
		return err
	}

	// Specified through --inject-script-by-url.
	if err := common.processInjectScriptsByUrl(c); err != nil {
		return err
	}

	return nil
}

var (
	ErrDuplicateScriptName = errors.New("Duplicate script name")
)

func (common *CommonConfig) processScripts(scripts map[string]string, timeSeedMs int64, constantMathRandomResult *float64) error {
	if common.injectScripts == "" {
		return nil
	}
	for _, scriptFile := range strings.Split(common.injectScripts, ",") {
		script, err := os.ReadFile(scriptFile)
		if err != nil {
			return fmt.Errorf("error opening script %s: %v", scriptFile, err)
		}
		name := filepath.Base(scriptFile)
		if scripts != nil {
			if _, ok := scripts[name]; ok {
				return fmt.Errorf("%w: %s", ErrDuplicateScriptName, name)
			}
			scripts[name] = string(script)
		}
		script = replaceConstants(name, script, timeSeedMs, constantMathRandomResult)
		if err := common.addScriptInjector(script, name); err != nil {
			return fmt.Errorf("error processing injected script %s: %v", name, err)
		}
	}
	return nil
}

func (common *CommonConfig) processRulesFile(c *cli.Context) error {
	rulesFile := c.String("rules-file")
	if rulesFile == "" {
		return nil
	}
	t, err := webpagereplay.NewRuleBasedTransformerFromFile(rulesFile)
	if err != nil {
		return err
	}
	common.transformers = append(common.transformers, t)
	Log().Info("Loaded replay rules", "path", rulesFile)
	return nil
}

func (common *CommonConfig) processInjectScriptsByUrl(c *cli.Context) error {
	byUrl := c.StringSlice("inject-script-by-url")
	if len(byUrl) == 0 {
		return nil
	}
	rules, err := parseInjectScriptsByUrl(byUrl)
	if err != nil {
		return err
	}
	t, err := webpagereplay.NewRuleBasedTransformer(rules)
	if err != nil {
		return err
	}
	common.transformers = append(common.transformers, t)
	return nil
}

func parseInjectScriptsByUrl(byUrl []string) ([]*webpagereplay.TransformerRule, error) {
	var rules []*webpagereplay.TransformerRule
	for _, s := range byUrl {
		// Use "::" as separator. Why it could appear in some file-path,
		// it's not very common, and the user can fix that issue by choosing
		// another path.
		parts := strings.SplitN(s, "::", 2)
		if len(parts) != 2 {
			return nil, fmt.Errorf("invalid inject-script-by-url format: %s", s)
		}
		scriptPath := parts[0] // Path validated later in the pipeline.
		urlStr := parts[1]

		rule := &webpagereplay.TransformerRule{
			InjectedScript: scriptPath,
		}
		rule.URLPattern = urlStr
		rules = append(rules, rule)
	}
	return rules, nil
}

func replaceConstants(
	filename string, script []byte, timeSeedMs int64, constantMathRandomResult *float64) []byte {

	randomResultStr := "null"
	if constantMathRandomResult != nil {
		randomResultStr = strconv.FormatFloat(*constantMathRandomResult, 'f', -1, 64)
	}

	timeSeedTimestamp := strconv.FormatInt(timeSeedMs, 10)
	// Legacy format, kept for backwards compatibility. These must be applied first,
	// since the new formats are substrings.
	script =
		bytes.Replace(script, []byte("{{WPR_TIME_SEED_TIMESTAMP}}"), []byte(timeSeedTimestamp), -1)
	script =
		bytes.Replace(script, []byte("{{WPR_CONSTANT_RANDOM_RESULT}}"), []byte(randomResultStr), -1)
	// New format.
	script = bytes.Replace(script, []byte("WPR_TIME_SEED_TIMESTAMP"), []byte(timeSeedTimestamp), -1)
	script = bytes.Replace(script, []byte("WPR_CONSTANT_RANDOM_RESULT"), []byte(randomResultStr), -1)
	return script
}

func (common *CommonConfig) addScriptInjector(script []byte, scriptFile string) error {
	Log().Info("Processing script", "path", scriptFile)
	si, err := webpagereplay.NewScriptInjector(script, webpagereplay.ScriptInjectorConfig{
		HtmlInjection: common.htmlInjection,
		JsInjection:   common.jsInjection,
	})
	if err != nil {
		return fmt.Errorf("error creating script injector for %s: %v", scriptFile, err)
	}
	common.transformers = append(common.transformers, si)
	return nil
}

func (r *RecordCommand) Flags() []cli.Flag {
	return append(r.common.Flags(),
		&cli.BoolFlag{
			Name: "enable-experimental-timed-chunk",
			Usage: "When specified, record the precise timings of receiving " +
				"response stream chunks.",
			Destination: &r.enableExperimentalTimedChunk,
		},
	)
}

func (r *RecordCommand) CheckArgsAndSetLogLevel(c *cli.Context) error {
	if err := r.common.CheckArgsAndSetLogLevel(c); err != nil {
		return err
	}

	if r.enableExperimentalTimedChunk {
		Log().Warn("Timed chunk recording is enabled. Note that the " +
			"implementation is highly experimental at the moment and the format " +
			"is subject to change.")
	}

	return nil
}

func (r *ReplayCommand) Flags() []cli.Flag {
	return append(r.common.Flags(),
		&cli.StringFlag{
			Name:  "inject-archive-scripts",
			Value: "false",
			Usage: "Inject scripts stored in the archive on replay. Defaults to false.",
		},
		&cli.StringSliceFlag{
			Name:  "inject-script-by-url",
			Usage: "Inject scripts by URL. Format: path/to/script::URL.",
		},
		&cli.StringFlag{
			Name:        "rules-file",
			Value:       "",
			Usage:       "File containing rules to apply to responses during replay",
			Destination: &r.rulesFile,
		},
		&cli.BoolFlag{
			Name: "serve-response-in-chronological-sequence",
			Usage: "When an incoming request matches multiple recorded " +
				"responses, serve response in chronological sequence. " +
				"I.e. wpr responds to the first request with the first " +
				"recorded response, and the second request with the " +
				"second recorded response.",
			Destination: &r.serveResponseInChronologicalSequence,
		},
		&cli.BoolFlag{
			Name:        "disable-fuzzy-url-matching",
			Usage:       "When doing playback, require URLs to match exactly.",
			Destination: &r.disableFuzzyURLMatching,
		},
		&cli.BoolFlag{
			Name: "quiet-mode",
			Usage: "quiets the logging output by not logging the " +
				"ServeHTTP url call and responses",
			Destination: &r.quietMode,
		},
	)
}

func (r *RootCACommand) Flags() []cli.Flag {
	return append(r.certConfig.Flags(),
		&cli.StringFlag{
			Name:        "android-device-id",
			Value:       "",
			Usage:       "Device id of an android device. Only relevant for Android",
			Destination: &r.installer.AndroidDeviceId,
		},
		&cli.StringFlag{
			Name:        "adb-binary-path",
			Value:       "adb",
			Usage:       "Path to adb binary. Only relevant for Android",
			Destination: &r.installer.AdbBinaryPath,
		},
		// Most desktop machines Google engineers use come with certutil installed.
		// In the chromium lab, desktop bots do not have certutil. Instead, desktop
		// bots deploy certutil binaries to <chromium src>/third_party/nss/certutil.
		// To accommodate chromium bots, the following flag accepts a custom path to
		// certutil. Otherwise WPR assumes that certutil resides in the PATH.
		&cli.StringFlag{
			Name:        "certutil-path",
			Value:       "certutil",
			Usage:       "Path to Network Security Services (NSS)'s certutil tool.",
			Destination: &r.installer.CertUtilBinaryPath,
		},
	)
}

func getListener(host string, port int) (net.Listener, error) {
	addr, err := net.ResolveTCPAddr("tcp", fmt.Sprintf("%v:%d", host, port))
	if err != nil {
		return nil, err
	}
	return net.ListenTCP("tcp", addr)
}

// Copied from https://golang.org/src/net/http/server.go.
// This is to make dead TCP connections to eventually go away.
type tcpKeepAliveListener struct {
	*net.TCPListener
}

func (ln tcpKeepAliveListener) Accept() (c net.Conn, err error) {
	tc, err := ln.AcceptTCP()
	if err != nil {
		return
	}
	tc.SetKeepAlive(true)
	tc.SetKeepAlivePeriod(3 * time.Minute)
	return tc, nil
}

func startServers(tlsconfig *tls.Config, httpHandler, httpsHandler http.Handler, common *CommonConfig) {
	type Server struct {
		Scheme string
		Host   string
		Port   int
		*http.Server
	}

	servers := []*Server{}

	if common.httpPort > -1 {
		servers = append(servers, &Server{
			Scheme: "http",
			Host:   common.host,
			Port:   common.httpPort,
			Server: &http.Server{
				Addr:    fmt.Sprintf("%v:%v", common.host, common.httpPort),
				Handler: httpHandler,
			},
		})
	}
	if common.httpsPort > -1 {
		servers = append(servers, &Server{
			Scheme: "https",
			Host:   common.host,
			Port:   common.httpsPort,
			Server: &http.Server{
				Addr:      fmt.Sprintf("%v:%v", common.host, common.httpsPort),
				Handler:   httpsHandler,
				TLSConfig: tlsconfig,
			},
		})
	}
	if common.httpSecureProxyPort > -1 {
		servers = append(servers, &Server{
			Scheme: "https",
			Host:   common.host,
			Port:   common.httpSecureProxyPort,
			Server: &http.Server{
				Addr:      fmt.Sprintf("%v:%v", common.host, common.httpSecureProxyPort),
				Handler:   httpHandler, // this server proxies HTTP requests over an HTTPS connection
				TLSConfig: nil,         // use the default since this is as a proxy, not a MITM server
			},
		})
	}

	for _, s := range servers {
		s := s
		go func() {
			var ln net.Listener
			var err error
			switch s.Scheme {
			case "http":
				ln, err = getListener(s.Host, s.Port)
				if err != nil {
					break
				}
				logServeStarted(s.Scheme, ln)
				err = s.Serve(tcpKeepAliveListener{ln.(*net.TCPListener)})
			case "https":
				ln, err = getListener(s.Host, s.Port)
				if err != nil {
					break
				}
				logServeStarted(s.Scheme, ln)
				http2.ConfigureServer(s.Server, &http2.Server{})
				tlsListener := tls.NewListener(tcpKeepAliveListener{ln.(*net.TCPListener)}, s.TLSConfig)
				err = s.Serve(tlsListener)
			default:
				panic(fmt.Sprintf("unknown s.Scheme: %s", s.Scheme))
			}
			if err != nil {
				Log().Error("Failed to start server", "scheme", s.Scheme, "addr", s.Addr, "error", err)
			}
		}()
	}

	fmt.Printf("Use Ctrl-C to exit\n")
	select {}
}

func logServeStarted(scheme string, ln net.Listener) {
	// DO NOT CHANGE: this line is parsed by downstream tools like catapult and crossbench.
	fmt.Printf("Starting server on %s://%s\n", scheme, ln.Addr().String())
}

func (r *RecordCommand) Run(c *cli.Context) error {
	archiveFileName := c.Args().First()
	archive, err := webpagereplay.OpenWritableArchive(archiveFileName)
	if err != nil {
		cli.ShowSubcommandHelp(c)
		os.Exit(1)
	}
	defer archive.Close()
	Log().Info("Opened archive", "path", archiveFileName)

	// Install a SIGINT handler to close the archive before shutting down.
	go func() {
		sigchan := make(chan os.Signal, 1)
		signal.Notify(sigchan, os.Interrupt)
		<-sigchan
		Log().Info("Shutting down")
		Log().Info("Writing archive", "path", archiveFileName)
		if err := archive.Close(); err != nil {
			Log().Error("Error flushing archive", "error", err)
		}
		os.Exit(0)
	}()

	if err := r.common.ProcessInjectedScriptsForRecording(c, &archive.Archive); err != nil {
		Log().Error("Error processing injected scripts", "error", err)
		os.Exit(1)
	}

	if r.enableExperimentalTimedChunk {
		Log().Error("NOTIMPLEMENTED: Experimental Timed Chunk recording support")
		os.Exit(1)
	}
	httpHandler := webpagereplay.NewRecordingProxy(archive, "http", r.common.transformers, r.common.paramToIgnoreInURLPath)
	httpsHandler := webpagereplay.NewRecordingProxy(archive, "https", r.common.transformers, r.common.paramToIgnoreInURLPath)
	tlsconfig, err := webpagereplay.RecordTLSConfig(r.common.rootCerts, archive, !r.common.noArchiveCertificates)
	if err != nil {
		Log().Error("Error creating TLSConfig", "error", err)
		os.Exit(1)
	}
	startServers(tlsconfig, httpHandler, httpsHandler, &r.common)
	return nil
}

func (r *ReplayCommand) Run(c *cli.Context) error {
	archiveFileName := c.Args().First()
	Log().Info("Loading archive", "path", archiveFileName)
	archive, err := webpagereplay.OpenArchive(archiveFileName)
	if err != nil {
		Log().Error("Error opening archive file", "error", err)
		os.Exit(1)
	}
	Log().Info("Opened archive", "path", archiveFileName)

	archive.ServeResponseInChronologicalSequence = r.serveResponseInChronologicalSequence
	archive.DisableFuzzyURLMatching = r.disableFuzzyURLMatching
	if archive.DisableFuzzyURLMatching {
		Log().Info("Disabling fuzzy URL matching")
	}

	if err := r.common.ProcessInjectedScriptsForReplay(c, archive); err != nil {
		Log().Error("Error processing injected scripts", "error", err)
		os.Exit(1)
	}

	// When recording, transformations are applied at request time, because that's
	// the only way. But here, when replaying, transformations are applied ahead
	// of requests, for performance reasons.
	transformedArchive := webpagereplay.Archive{
		Requests:                             make(map[string]map[string][]*webpagereplay.ArchivedRequest),
		Certs:                                archive.Certs,
		NegotiatedProtocol:                   archive.NegotiatedProtocol,
		DeterministicTimeSeedMs:              archive.DeterministicTimeSeedMs,
		ServeResponseInChronologicalSequence: archive.ServeResponseInChronologicalSequence,
		CurrentSessionId:                     archive.CurrentSessionId,
		DisableFuzzyURLMatching:              archive.DisableFuzzyURLMatching,
	}
	err = archive.ForEach(func(req *http.Request, resp *http.Response) error {
		for _, t := range r.common.transformers {
			t.Transform(req, resp)
		}
		return transformedArchive.AddArchivedRequest(req, resp, webpagereplay.AddModeAppend)
	})
	if err != nil {
		Log().Error("Error while creating transformed archive", "error", err)
	} else {
		archive = &transformedArchive
	}

	httpHandler := webpagereplay.NewReplayingProxy(archive, "http", r.quietMode, r.common.paramToIgnoreInURLPath)
	httpsHandler := webpagereplay.NewReplayingProxy(archive, "https", r.quietMode, r.common.paramToIgnoreInURLPath)
	tlsconfig, err := webpagereplay.ReplayTLSConfig(r.common.rootCerts, archive, !r.common.noArchiveCertificates)
	if err != nil {
		Log().Error("Error creating TLSConfig", "error", err)
		os.Exit(1)
	}
	startServers(tlsconfig, httpHandler, httpsHandler, &r.common)
	return nil
}

func (r *RootCACommand) Install(c *cli.Context) error {
	if err := r.installer.InstallRoot(
		r.certConfig.certFile, r.certConfig.keyFile); err != nil {
		Log().Error("Install root failed", "error", err)
		os.Exit(1)
	}
	return nil
}

func (r *RootCACommand) Remove(c *cli.Context) error {
	r.installer.RemoveRoot()
	return nil
}

func main() {
	progName := filepath.Base(os.Args[0])

	var record RecordCommand
	var replay ReplayCommand
	var installroot RootCACommand
	var removeroot RootCACommand

	record.cmd = cli.Command{
		Name:   "record",
		Usage:  "Record web pages to an archive",
		Flags:  record.Flags(),
		Before: record.CheckArgsAndSetLogLevel,
		Action: record.Run,
	}

	replay.cmd = cli.Command{
		Name:   "replay",
		Usage:  "Replay a previously-recorded web page archive",
		Flags:  replay.Flags(),
		Before: replay.common.CheckArgsAndSetLogLevel,
		Action: replay.Run,
	}

	installroot.cmd = cli.Command{
		Name:   "installroot",
		Usage:  "Install a test root CA",
		Flags:  installroot.Flags(),
		Before: installroot.certConfig.CheckArgs,
		Action: installroot.Install,
	}

	removeroot.cmd = cli.Command{
		Name:   "removeroot",
		Usage:  "Remove a test root CA",
		Flags:  removeroot.Flags(),
		Before: removeroot.certConfig.CheckArgs,
		Action: removeroot.Remove,
	}

	app := cli.NewApp()
	app.Commands = []*cli.Command{&record.cmd, &replay.cmd, &installroot.cmd, &removeroot.cmd}
	for _, cmd := range app.Commands {
		webpagereplay.AddLegacyAliases(&cmd.Flags)
	}
	app.Usage = "Web Page Replay"
	app.UsageText = fmt.Sprintf(longUsage, progName, progName)
	app.HideVersion = true
	app.Version = ""
	app.Writer = os.Stderr
	app.RunAndExitOnError()
}
