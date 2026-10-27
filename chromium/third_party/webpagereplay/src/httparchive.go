// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Program httparchive prints information about archives saved by record.
package main

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/urfave/cli/v2"
	"go.chromium.org/webpagereplay/src/webpagereplay"
)

var Log = webpagereplay.Log

const usage = "%s [ls|cat|edit|merge|add|add-all|trim|inject|" +
	"read-metadata|write-metadata|edit-metadata] [options] archive_file " +
	"[output_file] [url]"

func requestEnabled(cfg *webpagereplay.HttpArchiveConfig, req *http.Request, resp *http.Response) bool {
	if cfg.Method != "" && strings.ToUpper(cfg.Method) != req.Method {
		return false
	}
	if cfg.Host != "" && cfg.Host != req.Host {
		return false
	}
	if cfg.FullPath != "" && cfg.FullPath != req.URL.Path {
		return false
	}
	if cfg.StatusCode != 0 && cfg.StatusCode != resp.StatusCode {
		return false
	}
	return true
}

func list(cfg *webpagereplay.HttpArchiveConfig, a *webpagereplay.Archive, printFull bool) error {
	return a.ForEach(func(req *http.Request, resp *http.Response) error {
		if !requestEnabled(cfg, req, resp) {
			return nil
		}
		if printFull {
			fmt.Printf("----------------------------------------\n")
			req.Write(os.Stdout)
			fmt.Printf("\n")
			err := webpagereplay.DecompressResponse(resp)
			if err != nil {
				return fmt.Errorf("Unable to decompress body:\n%v", err)
			}
			resp.Write(os.Stdout)
			fmt.Printf("\n")
		} else {
			fmt.Printf("%s %s %s %s\n", req.Method, req.Host, req.URL, resp.Status)
		}
		return nil
	})
}

func readMetadata(a *webpagereplay.Archive) error {
	fmt.Print(a.Metadata)
	if a.Metadata != "" && !strings.HasSuffix(a.Metadata, "\n") {
		fmt.Printf("\n")
	}
	return nil
}

func writeMetadata(a *webpagereplay.Archive, outfile, metadata string) error {
	a.Metadata = metadata
	return writeArchive(a, outfile)
}

func editMetadata(a *webpagereplay.Archive, outfile string) error {
	// Determine which editor to use.
	editor := os.Getenv("EDITOR")
	if editor == "" {
		Log().Warn("EDITOR not specified, defaulting to vi.")
		editor = "vi"
	}

	// Set up a temporary file to use.
	tmpf, err := ioutil.TempFile("", "httparchive_edit_metadata")
	if err != nil {
		return err
	}
	tmpname := tmpf.Name()
	defer os.Remove(tmpname)
	if _, err := tmpf.WriteString(a.Metadata); err != nil {
		tmpf.Close()
		return err
	}
	if err := tmpf.Close(); err != nil {
		return err
	}

	// Launch the editor; block until the user quits it. (Note that this
	// would fail to block on GUI editors if EDITOR does not specify something
	// like `code --wait` or `subl -w` or some equivalent.)
	cmd := exec.Command(editor, tmpname)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("Error running %s %s: %v", editor, tmpname, err)
	}

	// After the user quits the editor, read the results back and write them to
	// the target archive.
	newMetadata, err := ioutil.ReadFile(tmpname)
	if err != nil {
		return err
	}
	a.Metadata = string(newMetadata)
	return writeArchive(a, outfile)
}

func trim(cfg *webpagereplay.HttpArchiveConfig, a *webpagereplay.Archive, outfile string) error {
	newA, err := a.Trim(func(req *http.Request, resp *http.Response) (bool, error) {
		// If req matches and invertMatch -> keep match
		// If req doesn't match and !invertMatch -> keep match
		// Otherwise, trim match
		if requestEnabled(cfg, req, resp) == cfg.InvertMatch {
			Log().Warn("Keeping request", "host", req.Host, "uri", req.URL.String())
			return false, nil
		} else {
			Log().Info("Trimming request", "host", req.Host, "uri", req.URL.String())
			return true, nil
		}
	})
	if err != nil {
		return fmt.Errorf("error editing archive:\n%v", err)
	}
	return writeArchive(newA, outfile)
}

func edit(cfg *webpagereplay.HttpArchiveConfig, a *webpagereplay.Archive, outfile string) error {
	editorFields := strings.Fields(os.Getenv("EDITOR"))
	if len(editorFields) == 0 {
		Log().Warn("EDITOR not specified, using default.")
		editorFields = []string{"vi"}
	}

	marshalForEdit := func(w io.Writer, req *http.Request, resp *http.Response) error {
		// Since req.Body can be read only once, we will restore it after use to
		// keep the req object in a valid state.
		body, err := ioutil.ReadAll(req.Body)
		if err != nil {
			return err
		}
		req.Body.Close()
		req.Body = ioutil.NopCloser(bytes.NewReader(body))

		// WriteProxy writes absolute URI in the Start line including the
		// scheme and host. It is necessary for unmarshaling later.
		if err := req.WriteProxy(w); err != nil {
			return err
		}
		// Restore the body for later use.
		req.Body = ioutil.NopCloser(bytes.NewReader(body))

		if cfg.DecodeResponseBody {
			if err := webpagereplay.DecompressResponse(resp); err != nil {
				return fmt.Errorf("couldn't decompress body: %v", err)
			}
		}
		return resp.Write(w)
	}

	unmarshalAfterEdit := func(r io.Reader) (*http.Request, *http.Response, error) {
		br := bufio.NewReader(r)
		req, err := http.ReadRequest(br)
		if err != nil {
			return nil, nil, fmt.Errorf("couldn't unmarshal request: %v", err)
		}

		// Ensure the request body is fully read if it exists, otherwise ReadResponse
		// might start reading from the middle of the request body if it wasn't fully
		// consumed by ReadRequest.
		reqBody, err := ioutil.ReadAll(req.Body)
		if err != nil {
			return nil, nil, fmt.Errorf("couldn't consume request body: %v", err)
		}
		req.Body.Close()
		// Reset the body to the read content so that ReadResponse sees the correct
		// body size if it checks.
		req.Body = ioutil.NopCloser(bytes.NewReader(reqBody))

		resp, err := http.ReadResponse(br, req)
		if err != nil {
			return nil, nil, fmt.Errorf("couldn't unmarshal response: %v", err)
		}

		originalContentLength := resp.ContentLength

		// The user might have edited the response body, changing its length,
		// without manually updating the Content-Length header.
		// So we ignore resp.Body (which relies on Content-Length) and just read
		// the rest of the file directly.
		actualBody, err := ioutil.ReadAll(br)
		if err != nil {
			return nil, nil, fmt.Errorf("couldn't read actual response body: %v", err)
		}
		resp.Body.Close()
		resp.Body = ioutil.NopCloser(bytes.NewReader(actualBody))

		// Chunked responses have Content-Length -1 and those should be preserved.
		if originalContentLength >= 0 {
			resp.ContentLength = int64(len(actualBody))
			resp.Header.Set("Content-Length", strconv.Itoa(len(actualBody)))
		}

		if cfg.DecodeResponseBody {
			// Compress body back according to Content-Encoding
			if err := compressResponse(resp); err != nil {
				return nil, nil, fmt.Errorf("couldn't compress response: %v", err)
			}
		}
		// Read resp.Body into a buffer since the tmpfile is about to be deleted.
		body, err := ioutil.ReadAll(resp.Body)
		resp.Body.Close()
		if err != nil {
			return nil, nil, fmt.Errorf("couldn't unmarshal response body: %v", err)
		}

		resp.Body = ioutil.NopCloser(bytes.NewReader(body))
		if originalContentLength >= 0 {
			resp.ContentLength = int64(len(body))
			resp.Header.Set("Content-Length", strconv.Itoa(len(body)))
		}
		return req, resp, nil
	}

	newA, err := a.Edit(func(req *http.Request, resp *http.Response) (*http.Request, *http.Response, error) {
		if !requestEnabled(cfg, req, resp) {
			return req, resp, nil
		}
		Log().Info("Editing request", "host", req.Host, "uri", req.URL.String())
		// Serialize the req/resp to a temporary file, let the user edit that file, then
		// de-serialize and return the result. Repeat until de-serialization succeeds.
		for {
			tmpf, err := ioutil.TempFile("", "httparchive_edit_request")
			if err != nil {
				return nil, nil, err
			}
			tmpname := tmpf.Name()
			defer os.Remove(tmpname)
			if err := marshalForEdit(tmpf, req, resp); err != nil {
				tmpf.Close()
				return nil, nil, err
			}
			if err := tmpf.Close(); err != nil {
				return nil, nil, err
			}
			// Edit this file.
			cmd := exec.Command(editorFields[0], append(editorFields[1:], tmpname)...)
			cmd.Stdin = os.Stdin
			cmd.Stdout = os.Stdout
			cmd.Stderr = os.Stderr
			if err := cmd.Run(); err != nil {
				return nil, nil, fmt.Errorf("Error running %s %s: %v", editorFields, tmpname, err)
			}
			// Reload.
			tmpf, err = os.Open(tmpname)
			if err != nil {
				return nil, nil, err
			}
			defer tmpf.Close()
			newReq, newResp, err := unmarshalAfterEdit(tmpf)
			if err != nil {
				Log().Error("Error while editing request", "error", err)
				continue
			}
			return newReq, newResp, nil
		}
	})
	if err != nil {
		return fmt.Errorf("error editing archive:\n%v", err)
	}

	return writeArchive(newA, outfile)
}

func writeArchive(archive *webpagereplay.Archive, outfile string) error {
	outf, err := os.OpenFile(outfile, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, os.FileMode(0660))
	if err != nil {
		return fmt.Errorf("error opening output file %s:\n%v", outfile, err)
	}
	err0 := archive.Serialize(outf)
	err1 := outf.Close()
	if err0 != nil || err1 != nil {
		if err0 == nil {
			err0 = err1
		}
		return fmt.Errorf("error writing edited archive to %s:\n%v", outfile, err0)
	}
	Log().Info("Wrote edited archive", "file", outfile)
	return nil
}

func merge(cfg *webpagereplay.HttpArchiveConfig, archive *webpagereplay.Archive, input *webpagereplay.Archive, outfile string, keepDuplicates bool) error {
	if err := archive.Merge(input, keepDuplicates); err != nil {
		return fmt.Errorf("Merge archives failed: %v", err)
	}

	return writeArchive(archive, outfile)
}

func addUrl(cfg *webpagereplay.HttpArchiveConfig, archive *webpagereplay.Archive, urlString string) error {
	addMode := webpagereplay.AddModeAppend
	if cfg.SkipExisting {
		addMode = webpagereplay.AddModeSkipExisting
	} else if cfg.OverwriteExisting {
		addMode = webpagereplay.AddModeOverwriteExisting
	}
	if err := archive.Add("GET", urlString, addMode); err != nil {
		return fmt.Errorf("Error adding request: %v", err)
	}
	return nil
}

func add(cfg *webpagereplay.HttpArchiveConfig, archive *webpagereplay.Archive, outfile string, urls []string) error {
	for _, urlString := range urls {
		if err := addUrl(cfg, archive, urlString); err != nil {
			return err
		}
	}
	return writeArchive(archive, outfile)
}

func addAll(cfg *webpagereplay.HttpArchiveConfig, archive *webpagereplay.Archive, outfile string, inputFilePath string) error {
	f, err := os.OpenFile(inputFilePath, os.O_RDONLY, os.ModePerm)
	if err != nil {
		return fmt.Errorf("open file error: %v", err)
	}
	defer f.Close()

	sc := bufio.NewScanner(f)
	for sc.Scan() {
		urlString := sc.Text() // GET the line string
		if err := addUrl(cfg, archive, urlString); err != nil {
			return err
		}
	}
	if err := sc.Err(); err != nil {
		return fmt.Errorf("scan file error: %v", err)
	}

	return writeArchive(archive, outfile)
}

func inject(cfg *webpagereplay.HttpArchiveConfig, a *webpagereplay.Archive, outfile string, scriptFile string) error {
	si, err := webpagereplay.NewScriptInjectorFromFile(scriptFile)
	if err != nil {
		return fmt.Errorf("Error opening script %s: %v", scriptFile, err)
	}

	err = a.ForEach(func(req *http.Request, resp *http.Response) error {
		if requestEnabled(cfg, req, resp) {
			si.Transform(req, resp)
		}
		a.AddArchivedRequest(req, resp, webpagereplay.AddModeOverwriteExisting)
		return nil
	})
	if err != nil {
		return fmt.Errorf("Error editing archive: %v", err)
	}

	return writeArchive(a, outfile)
}

// compressResponse compresses resp.Body in place according to resp's Content-Encoding header.
// The caller is responsible for setting Content-Length.
func compressResponse(resp *http.Response) error {
	ce := strings.ToLower(resp.Header.Get("Content-Encoding"))
	if ce == "" {
		return nil
	}
	body, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	resp.Body.Close()

	body, newCE, err := webpagereplay.CompressBody(ce, body)
	if err != nil {
		return err
	}
	if ce != newCE {
		return fmt.Errorf("can't compress body to '%s' received Content-Encoding: '%s'", ce, newCE)
	}
	resp.Body = ioutil.NopCloser(bytes.NewReader(body))
	return nil
}

func main() {
	progName := filepath.Base(os.Args[0])
	cfg := &webpagereplay.HttpArchiveConfig{
		LogLevel: "INFO",
	}

	fail := func(c *cli.Context, err error) {
		Log().Error("An error occurred", "error", err)
		cli.ShowSubcommandHelp(c)
		os.Exit(1)
	}

	checkArgs := func(wantArgs int) func(*cli.Context) error {
		return func(c *cli.Context) error {
			if c.Args().Len() != wantArgs {
				return fmt.Errorf("Expected %d arguments but got %d", wantArgs, c.Args().Len())
			}
			return nil
		}
	}
	loadArchiveOrDie := func(c *cli.Context, arg int) *webpagereplay.Archive {
		archive, err := webpagereplay.OpenArchive(c.Args().Get(arg))
		if err != nil {
			fail(c, err)
		}
		return archive
	}

	app := cli.NewApp()
	app.Commands = []*cli.Command{
		&cli.Command{
			Name:      "ls",
			Usage:     "List the requests in an archive",
			ArgsUsage: "archive",
			Flags:     cfg.DefaultFlags(),
			Before:    checkArgs(1),
			Action: func(c *cli.Context) error {
				return list(cfg, loadArchiveOrDie(c, 0), false)
			},
		},
		&cli.Command{
			Name:      "cat",
			Usage:     "Dump the requests/responses in an archive",
			ArgsUsage: "archive",
			Flags:     cfg.DefaultFlags(),
			Before:    checkArgs(1),
			Action: func(c *cli.Context) error {
				return list(cfg, loadArchiveOrDie(c, 0), true)
			},
		},
		&cli.Command{
			Name:      "edit",
			Usage:     "Edit the requests/responses in an archive",
			ArgsUsage: "input_archive output_archive",
			Flags:     cfg.DefaultFlags(),
			Before:    checkArgs(2),
			Action: func(c *cli.Context) error {
				return edit(cfg, loadArchiveOrDie(c, 0), c.Args().Get(1))
			},
		},
		&cli.Command{
			Name:      "merge",
			Usage:     "Merge the requests/responses of two archives",
			ArgsUsage: "base_archive input_archive output_archive",
			Flags:     cfg.MergeFlags(),
			Before:    checkArgs(3),
			Action: func(c *cli.Context) error {
				return merge(cfg, loadArchiveOrDie(c, 0), loadArchiveOrDie(c, 1),
					c.Args().Get(2), cfg.KeepDuplicates)
			},
		},
		&cli.Command{
			Name:      "add",
			Usage:     "Add a simple GET request from the network to the archive",
			ArgsUsage: "input_archive output_archive [urls...]",
			Flags:     cfg.AddFlags(),
			Before: func(c *cli.Context) error {
				if c.Args().Len() < 3 {
					return fmt.Errorf("Expected at least 3 arguments but got %d", c.Args().Len())
				}
				return nil
			},
			Action: func(c *cli.Context) error {
				return add(cfg, loadArchiveOrDie(c, 0), c.Args().Get(1), c.Args().Tail())
			},
		},
		&cli.Command{
			Name:      "add-all",
			Usage:     "Add a simple GET request from the network to the archive",
			ArgsUsage: "input_archive output_archive urls_file",
			Flags:     cfg.AddFlags(),
			Before:    checkArgs(3),
			Action: func(c *cli.Context) error {
				return addAll(cfg, loadArchiveOrDie(c, 0), c.Args().Get(1), c.Args().Get(2))
			},
		},
		&cli.Command{
			Name:      "trim",
			Usage:     "Trim the requests/responses in an archive",
			ArgsUsage: "input_archive output_archive",
			Flags:     cfg.TrimFlags(),
			Before:    checkArgs(2),
			Action: func(c *cli.Context) error {
				return trim(cfg, loadArchiveOrDie(c, 0), c.Args().Get(1))
			},
		},
		&cli.Command{
			Name:      "inject",
			Usage:     "Inject a script into the selected responses of an archive",
			ArgsUsage: "input_archive output_archive script",
			Flags:     cfg.RequestFilterFlags(),
			Before:    checkArgs(3),
			Action: func(c *cli.Context) error {
				return inject(cfg, loadArchiveOrDie(c, 0), c.Args().Get(1), c.Args().Get(2))
			},
		},
		&cli.Command{
			Name:      "read-metadata",
			Usage:     "Read metadata from an archive and print to stdout",
			ArgsUsage: "archive",
			Before:    checkArgs(1),
			Action: func(c *cli.Context) error {
				return readMetadata(loadArchiveOrDie(c, 0))
			},
		},
		&cli.Command{
			Name:      "write-metadata",
			Usage:     "Write metadata string to a copy of an archive",
			ArgsUsage: "input_archive output_archive metadata_string",
			Before:    checkArgs(3),
			Action: func(c *cli.Context) error {
				return writeMetadata(loadArchiveOrDie(c, 0), c.Args().Get(1), c.Args().Get(2))
			},
		},
		&cli.Command{
			Name:      "edit-metadata",
			Usage:     "Edit metadata from a copy of an archive using $EDITOR",
			ArgsUsage: "input_archive output_archive",
			Before:    checkArgs(2),
			Action: func(c *cli.Context) error {
				return editMetadata(loadArchiveOrDie(c, 0), c.Args().Get(1))
			},
		},
		&cli.Command{
			Name:      "ls-scripts",
			Usage:     "List scripts embedded in an archive by name",
			ArgsUsage: "archive",
			Before:    checkArgs(1),
			Action: func(c *cli.Context) error {
				archive := loadArchiveOrDie(c, 0)
				for name := range archive.InjectedScripts {
					fmt.Println(name)
				}
				return nil
			},
		},
	}
	for _, cmd := range app.Commands {
		webpagereplay.AddLegacyAliases(&cmd.Flags)
	}
	app.Usage = "HTTP Archive Utils"
	app.UsageText = fmt.Sprintf(usage, progName)
	app.HideVersion = true
	app.Version = ""
	app.Writer = os.Stderr
	app.Before = func(c *cli.Context) error {
		if err := webpagereplay.SetLogLevel(cfg.LogLevel); err != nil {
			return fmt.Errorf("Invalid log_level (%s): %v", cfg.LogLevel, err)
		}
		webpagereplay.SetRelativeTimestamps(cfg.RelativeTimestamps)
		return nil
	}
	err := app.Run(os.Args)
	if err != nil {
		Log().Error("Error encountered", "error", err)
		os.Exit(1)
	}
}
