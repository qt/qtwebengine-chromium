// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package webpagereplay

import (
	"bytes"
	"compress/gzip"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"testing"

	"github.com/kylelemons/godebug/pretty"
)

const injectedScript string = "var foo=1"

// Regression test for https://github.com/catapult-project/catapult/issues/3726
func TestInjectScript(t *testing.T) {
	transformer, err := NewScriptInjector([]byte(injectedScript), DefaultScriptInjectorConfig())
	if err != nil {
		t.Fatal(err)
	}
	req := http.Request{}
	responseHeader := http.Header{
		"Content-Type": []string{"text/html"}}
	resp := http.Response{
		StatusCode: 200,
		Header:     responseHeader,
		Request:    &req,
		Body: ioutil.NopCloser(bytes.NewReader([]byte("<html><head><script>" +
			"document.write('<head></head>');</script></head></html>")))}
	transformer.Transform(&req, &resp)
	body, err := ioutil.ReadAll(resp.Body)
	resp.Body.Close()
	if err != nil {
		t.Fatal(err)
	}
	expectedContent := []byte("<html><head><script>" + injectedScript +
		"</script><script>document.write('<head></head>');</script>" +
		"</head></html>")
	if !bytes.Equal(expectedContent, body) {
		t.Fatal(
			fmt.Errorf("expected : %s \n actual: %s \n", expectedContent, body))
	}
}

func TestNoTagFound(t *testing.T) {
	transformer, err := NewScriptInjector([]byte(injectedScript), DefaultScriptInjectorConfig())
	if err != nil {
		t.Fatal(err)
	}
	req := http.Request{}
	responseHeader := http.Header{
		"Content-Type": []string{"text/html"}}
	resp := http.Response{
		StatusCode: 200,
		Header:     responseHeader,
		Request:    &req,
		Body: ioutil.NopCloser(bytes.NewReader(
			[]byte("no tag random content")))}
	resp.Request = &req
	transformer.Transform(&req, &resp)
	body, err := ioutil.ReadAll(resp.Body)
	resp.Body.Close()
	if err != nil {
		t.Fatal(err)
	}
	expectedContent := []byte(fmt.Sprintf("no tag random content"))
	if !bytes.Equal(expectedContent, body) {
		t.Fatal(
			fmt.Errorf("expected : %s \n actual: %s \n", expectedContent, body))
	}
}

func TestInjectScriptToGzipResponse(t *testing.T) {
	transformer, minifyErr := NewScriptInjector([]byte(injectedScript), DefaultScriptInjectorConfig())
	if minifyErr != nil {
		t.Fatal(minifyErr)
	}
	req := http.Request{}
	responseHeader := http.Header{
		"Content-Type":     []string{"text/html"},
		"Content-Encoding": []string{"gzip"}}
	var gzippedBody bytes.Buffer
	gz := gzip.NewWriter(&gzippedBody)
	if _, err := gz.Write([]byte("<html></html>")); err != nil {
		t.Fatal(err)
	}
	if err := gz.Close(); err != nil {
		t.Fatal(err)
	}
	resp := http.Response{
		StatusCode: 200,
		Header:     responseHeader,
		Request:    &req,
		Body:       ioutil.NopCloser(bytes.NewReader(gzippedBody.Bytes()))}
	transformer.Transform(&req, &resp)
	var reader io.ReadCloser
	var err error
	if reader, err = gzip.NewReader(resp.Body); err != nil {
		t.Fatal(err)
	}
	var body []byte
	if body, err = ioutil.ReadAll(reader); err != nil {
		t.Fatal(err)
	}
	reader.Close()
	expectedContent := []byte("<html><script>" + injectedScript + "</script></html>")
	if !bytes.Equal(expectedContent, body) {
		t.Fatal(
			fmt.Errorf("expected : %s \n actual: %s \n", expectedContent, body))
	}
}

func transform(t *testing.T, input, contentType string, config ScriptInjectorConfig) string {
	t.Helper()
	transformer, err := NewScriptInjector([]byte(injectedScript), config)
	if err != nil {
		t.Fatal(err)
	}
	req := http.Request{}
	responseHeader := http.Header{"Content-Type": []string{contentType}}

	resp := http.Response{
		StatusCode: 200,
		Header:     responseHeader,
		Request:    &req,
		Body:       ioutil.NopCloser(bytes.NewReader([]byte(input)))}

	transformer.Transform(&req, &resp)
	body, err := ioutil.ReadAll(resp.Body)
	resp.Body.Close()
	if err != nil {
		t.Fatal(err)
	}

	return string(body)
}

func testHTMLInjection(t *testing.T, config ScriptInjectorConfig, input, expectedOutput string) {
	t.Helper()
	contentTypes := []string{
		"text/html",
		"TEXT/HTML",
		"Text/Html",
	}

	for _, contentType := range contentTypes {
		for _, suffix := range []string{"", "; charset=utf-8"} {
			result := transform(t, input, contentType+suffix, config)
			if result != expectedOutput {
				t.Errorf("For %s:\nExpected: %s\nActual: %s",
					contentType+suffix, expectedOutput, result)
			}
		}
	}
}

func TestInjectScriptToHTMLMimeType(t *testing.T) {
	const originalHTML string = "<html><head></head></html>"
	expectedHTML := "<html><head><script>" + injectedScript + "</script></head></html>"
	testHTMLInjection(t, ScriptInjectorConfig{HtmlInjection: true, JsInjection: false},
		originalHTML, expectedHTML)
}

func TestJsInjectionDoesNotAffectHTML(t *testing.T) {
	const originalHTML string = "<html><head></head></html>"
	testHTMLInjection(t, ScriptInjectorConfig{HtmlInjection: false, JsInjection: true},
		originalHTML, originalHTML)
}

func testJSInjection(t *testing.T, config ScriptInjectorConfig, input, expectedOutput string) {
	t.Helper()
	contentTypes := []string{
		// Standard MIME types.
		"application/javascript",
		"text/javascript",
		"application/x-javascript",
		// Non-standard but seen in the wild.
		"javascript",
		// Case insensitivity.
		"Application/JavaScript",
		"TEXT/JAVASCRIPT",
		"application/x-JAVASCRIPT",
		"JavaScript",
	}

	for _, contentType := range contentTypes {
		for _, suffix := range []string{"", "; charset=utf-8"} {
			result := transform(t, input, contentType+suffix, config)
			if result != expectedOutput {
				t.Errorf("For %s:\nExpected: %s\nActual: %s",
					contentType+suffix, expectedOutput, result)
			}
		}
	}
}

func TestInjectScriptToJSMimeType(t *testing.T) {
	const originalJS string = "console.log('hello');"
	const expectedJS string = injectedScript + ";\n" + originalJS
	testJSInjection(t, ScriptInjectorConfig{HtmlInjection: false, JsInjection: true},
		originalJS, expectedJS)
}

func TestHtmlInjectionDoesNotAffectJS(t *testing.T) {
	const originalJS string = "console.log('hello');"
	testJSInjection(t, ScriptInjectorConfig{HtmlInjection: true, JsInjection: false},
		originalJS, originalJS)
}

func TestInjectScriptToNonJSMimeType(t *testing.T) {
	const originalJS string = "console.log('hello');"

	// Values that don't trigger injection.
	contentTypes := []string{
		"text/css",
		"application/msword",
		"made/up",
		"gar-ba-ge",
	}

	// This value would normally trigger injection, but because the original
	// content is not an HTML file, the place to inject is not found.
	contentTypes = append(contentTypes, "text/html")

	for _, contentType := range contentTypes {
		for _, suffix := range []string{"", "; charset=utf-8"} {
			transformationResult := transform(t, originalJS, contentType+suffix,
				DefaultScriptInjectorConfig())
			if transformationResult != originalJS {
				t.Errorf("For %s:\nExpected: %s\nActual: %s",
					contentType+suffix, originalJS, string(transformationResult))
			}
		}
	}
}

func TestAlreadyInjected(t *testing.T) {
	const originalJS string = "console.log('hello');"
	const expectedJS string = injectedScript + ";\n" + originalJS
	const contentType string = "application/javascript"

	// The first injection is impactful.
	result1 := transform(t, originalJS, contentType,
		ScriptInjectorConfig{HtmlInjection: true, JsInjection: true})
	if result1 != expectedJS {
		t.Errorf("Expected: %s\nActual:   %s", expectedJS, result1)
	}

	// The second injection is no-op.
	result2 := transform(t, result1, contentType,
		ScriptInjectorConfig{HtmlInjection: true, JsInjection: true})
	if result2 != result1 {
		t.Errorf("Expected: %s\nActual:   %s", result1, result2)
	}
}

func TestDefensiveSemicolon(t *testing.T) {
	// Many minified JS files start with a parenthesis.
	const originalJS string = "(function(){})();"
	const expectedJS string = injectedScript + ";\n" + originalJS
	const contentType string = "application/javascript"

	result := transform(t, originalJS, contentType,
		ScriptInjectorConfig{HtmlInjection: true, JsInjection: true})
	if result != expectedJS {
		t.Errorf("Defensive semicolon missing or incorrect.\nExpected: %s\nActual: %s",
			expectedJS, result)
	}
}

func TestInjectScriptToResponse(t *testing.T) {
	tests := []struct {
		desc  string
		input []string
		want  string
	}{
		{
			desc:  "With CSP Nonce script-src",
			input: []string{"script-src 'strict-dynamic' 'nonce-2726c7f26c'"},
			want: "<html><head><script nonce=\"2726c7f26c\">" + injectedScript + "</script>" +
				"<script>document.write('<head></head>');</script></head></html>",
		},
		{
			desc:  "With CSP Nonce default-src",
			input: []string{"default-src 'strict-dynamic' 'nonce-2726c7f26c'"},
			want: "<html><head><script nonce=\"2726c7f26c\">" + injectedScript + "</script>" +
				"<script>document.write('<head></head>');</script></head></html>",
		},
		{
			desc:  "With CSP Nonce and both Default and Script",
			input: []string{"default-src 'self' https://foo.com;script-src 'strict-dynamic' 'nonce-2726cf26c'"},
			want: "<html><head><script nonce=\"2726cf26c\">" + injectedScript + "</script>" +
				"<script>document.write('<head></head>');</script></head></html>",
		},
		{
			desc:  "With CSP Nonce and both Default and Script override",
			input: []string{"default-src 'self' 'nonce-99999cf26c';script-src 'strict-dynamic' 'nonce-2726cf26c'"},
			want: "<html><head><script nonce=\"2726cf26c\">" + injectedScript + "</script>" +
				"<script>document.write('<head></head>');</script></head></html>",
		},
		{
			desc:  "With two CSP headers",
			input: []string{"useless", "script-src 'strict-dynamic' 'nonce-12345'"},
			want: "<html><head><script nonce=\"12345\">" + injectedScript + "</script>" +
				"<script>document.write('<head></head>');</script></head></html>",
		},
	}

	for _, tc := range tests {
		transformer, err := NewScriptInjector([]byte(injectedScript), DefaultScriptInjectorConfig())
		if err != nil {
			t.Fatal(err)
		}
		req := http.Request{}
		responseHeader := http.Header{
			"Content-Type": []string{"text/html"}}
		for _, input := range tc.input {
			responseHeader.Add("Content-Security-Policy", input)
		}
		resp := http.Response{
			StatusCode: 200,
			Header:     responseHeader,
			Request:    &req,
			Body: ioutil.NopCloser(bytes.NewReader([]byte("<html><head><script>" +
				"document.write('<head></head>');</script></head></html>")))}
		transformer.Transform(&req, &resp)
		body, err := ioutil.ReadAll(resp.Body)
		resp.Body.Close()
		if err != nil {
			t.Fatal(err)
		}
		if diff := pretty.Compare(tc.want, string(body)); diff != "" {
			t.Errorf("TestInjectScript scenario `%s`\nreturned diff (-want +got):\n%s",
				tc.desc, diff)
		}
	}
}

func TestInjectScriptToResponseWithCspHash(t *testing.T) {
	transformer, err := NewScriptInjector([]byte(injectedScript), DefaultScriptInjectorConfig())
	if err != nil {
		t.Fatal(err)
	}
	req := http.Request{}
	responseHeader := http.Header{
		"Content-Type": []string{"text/html"},
		"Content-Security-Policy": []string{
			"script-src 'strict-dynamic' " +
				"'sha256-pwltXkdHyMvChFSLNauyy5WItOFOm+iDDsgqRTr8peI='"}}
	resp := http.Response{
		StatusCode: 200,
		Header:     responseHeader,
		Request:    &req,
		Body: ioutil.NopCloser(bytes.NewReader([]byte("<html><head><script>" +
			"document.write('<head></head>');</script></head></html>")))}
	transformer.Transform(&req, &resp)
	assertEquals(t,
		resp.Header.Get("Content-Security-Policy"),
		"script-src 'strict-dynamic' "+
			"'sha256--NKAhNB_ewpUL916YVnpQuR_yWRHBmV6sThatA-5nK8=' "+
			"'sha256-pwltXkdHyMvChFSLNauyy5WItOFOm+iDDsgqRTr8peI=' ")
}

func TestTransformCsp(t *testing.T) {
	tests := []struct {
		desc     string
		input    string
		inputSha string
		want     string
	}{
		{
			desc:  "Just Script",
			input: "script-src 'self' https://foo.com;",
			want:  "script-src 'self' https://foo.com 'unsafe-inline';",
		},
		{
			desc:  "Just Default",
			input: "default-src 'self' https://foo.com;",
			want:  "default-src 'self' https://foo.com 'unsafe-inline';",
		},
		{
			desc:  "Both Script and Default Src",
			input: "default-src 'self' https://foo.com ; script-src 'self' 'nonce-2726c7f26c'",
			want:  "default-src 'self' https://foo.com ; script-src 'self' 'nonce-2726c7f26c'",
		},
		{
			desc:  "Both Script and Default Src No Nonce",
			input: "default-src 'self' https://foo.com ; script-src 'self'",
			want:  "default-src 'self' https://foo.com ; script-src 'self' 'unsafe-inline'",
		},
		{
			desc:     "Sha repeats",
			input:    "script-src 'self' blob: https://foo.com 'sha256-XXX' 'sha384-XXX' https://foo2.com 'sha512-XXX', 'sha256-XX';",
			inputSha: "NEW",
			want:     "script-src 'self' blob: https://foo.com 'sha256-NEW' 'sha256-XXX' 'sha384-XXX' https://foo2.com 'sha256-NEW' 'sha512-XXX', 'sha256-XX' ;",
		},
	}

	for _, tc := range tests {
		responseHeader := http.Header{"Content-Security-Policy": {tc.input}}
		transformCSPHeader(responseHeader, tc.inputSha)
		got := responseHeader.Get("Content-Security-Policy")
		if diff := pretty.Compare(tc.want, got); diff != "" {
			t.Errorf("TransformCsp scenario `%s`\n[input(%s)]\n returned diff (-want +got):\n%s",
				tc.desc, tc.input, diff)
		}
	}
}

func TestTransformMultipleCspEntries(t *testing.T) {
	tests := []struct {
		desc     string
		input    []string
		inputSha string
		want     []string
	}{
		{
			desc:     "CSP single entry",
			input:    []string{"script-src 'self' blob: https://foo.com 'sha256-XX1';"},
			inputSha: "NEW",
			want:     []string{"script-src 'self' blob: https://foo.com 'sha256-NEW' 'sha256-XX1' ;"},
		},
		{
			desc:     "CSP first entry relevant",
			input:    []string{"script-src 'self' blob: https://foo.com 'sha256-XX1';", "some other data"},
			inputSha: "NEW",
			want:     []string{"script-src 'self' blob: https://foo.com 'sha256-NEW' 'sha256-XX1' ;", "some other data"},
		},
		{
			desc:     "Sha second entry relevant",
			input:    []string{"some other data", "script-src 'self' blob: https://foo.com 'sha256-XX1';"},
			inputSha: "NEW",
			want:     []string{"some other data", "script-src 'self' blob: https://foo.com 'sha256-NEW' 'sha256-XX1' ;"},
		},
		{
			desc:     "no CSP entry",
			input:    []string{},
			inputSha: "NEW",
			want:     []string{},
		},
	}

	for _, tc := range tests {
		responseHeader := http.Header{"Content-Security-Policy": tc.input}
		transformCSPHeader(responseHeader, tc.inputSha)
		got := responseHeader.Values("Content-Security-Policy")
		if diff := pretty.Compare(tc.want, got); diff != "" {
			t.Errorf("TransformCsp scenario `%s`\n[input(%s)]\n returned diff (-want +got):\n%s",
				tc.desc, tc.input, diff)
		}
	}
}

func assertEquals(t *testing.T, actual, expected string) {
	if expected != actual {
		t.Errorf("Expected \"%s\" but was \"%s\"", expected, actual)
	}
}
