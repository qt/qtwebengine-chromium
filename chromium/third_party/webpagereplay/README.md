# Web Page Replay
Web Page Replay (WprGo) is a performance testing tool written in Golang for
recording and replaying web pages. WprGo is currently used in Telemetry for
Chrome benchmarking purposes. This requires go 1.8 and above. This has not been
tested with earlier versions of go. It is supported on Windows, MacOS and Linux.

For performance tests, this tool is generally not used directly. Instead, we use [these instructions](https://source.chromium.org/chromium/chromium/src/+/main:tools/perf/recording_benchmarks.md) to record, and [these instructions](https://chromium.googlesource.com/catapult.git/+/HEAD/telemetry/docs/run_benchmarks_locally.md) to replay.

## Getting the code

The first way is to use `go get`. This will use your default GOPATH, which is
typically `$HOME/go`:

```shell
go get go.chromium.org/webpagereplay
```

The second way is to clone the repository.

```shell
git clone https://chromium.googlesource.com/webpagereplay
```

## Sample usage

### Record mode
* Terminal 1:

  Start wpr in record mode. By default wpr uses both rsa and ecdsa certificates,
  or provide your certificate directly with --https_cert_file and --https_key_file
  parameters.

  ```shell
  cd path/to/webpagereplay
  go run src/wpr.go record --http_port=8080 --https_port=8081 /tmp/archive.wprgo
  ```
  ...

  Ctrl-C

* Terminal 2:

  ```shell
  google-chrome-beta --user-data-dir=$foo \
   --host-resolver-rules="MAP *:80 127.0.0.1:8080,MAP *:443 127.0.0.1:8081,EXCLUDE localhost"
   --ignore-certificate-errors-spki-list=PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=,2HcXCSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ=
  ```
  ... wait for record servers to start

### Replay mode
* Terminal 1:

  Start wpr in replay mode.
  ```shell
  cd path/to/webpagereplay
  go run src/wpr.go replay --http_port=8080 --https_port=8081 /tmp/archive.wprgo
  ```

* Terminal 2:
  ```shell
  google-chrome-beta --user-data-dir=$bar \
   --host-resolver-rules="MAP *:80 127.0.0.1:8080,MAP *:443 127.0.0.1:8081,EXCLUDE localhost"
   --ignore-certificate-errors-spki-list=PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=,2HcXCSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ=
  ```
  ... wait for replay servers to start

  load the page

## Running on Android

You will need a Linux host machine and an android device.

* Set up reverse port forwarding

```shell
adb reverse tcp:8080 tcp:8080
adb reverse tcp:8081 tcp:8081
```

* Set up command line arguments

```shell
build/android/adb_chrome_public_command_line --host-resolver-rules="MAP *:80 127.0.0.1:8080,MAP *:443 127.0.0.1:8081,EXCLUDE localhost" \
  --ignore-certificate-errors-spki-list=PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=,2HcXCSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ=
```

* Run wpr.go as usual on the linux machine

### (Optional) Installing test root CA

WebPageReplay uses self signed certificates for Https requests. To make Chrome
trust these certificates, you can use --ignore-certificate-errors-spki-list
like above. If that doesn't work, you may try installing a test certificate
authority as a local trust anchor. **Note:** Please do this with care because
installing the test root CA compromises your machine. This is currently only
supported on Linux and Android.

Installing the test CA. Specify a `--android_device_id` if you'd like to install
the root CA on an android device.
```shell
cd path/to/webpagereplay
go run src/wpr.go installroot
```
Uninstall the test CA. Specify a `--android_device_id` if you'd like to remove
the root CA from an android device.

```shell
cd path/to/webpagereplay
go run src/wpr.go removeroot
```

## Other use cases

### Http-to-http2 proxy:

* Terminal 1:
```shell
cd path/to/webpagereplay
go run src/wpr.go replay --https_port=8081 --https_to_http_port=8082 \
  /tmp/archive.wprgo
```

* Terminal 2:
```shell
google-chrome-beta --user-data-dir=$foo \
  --host-resolver-rules="MAP *:443 127.0.0.1:8081,EXCLUDE localhost" \
  --ignore-certificate-errors-spki-list=PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=,2HcXCSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ= \
  --proxy-server=http=https://127.0.0.1:8082 \
  --trusted-spdy-proxy=127.0.0.1:8082
```

## Inspecting an archive

httparchive.go is a convenient script to inspect a wprgo archive. Use `ls`,`cat`
and `edit`. Options are available to specify request url host (`--host`) and
path (`--full-path`).

E.g.

```shell
cd path/to/webpagereplay
go run src/httparchive.go ls /tmp/archive.wprgo --host=example.com --full-path=/index.html
```

## Altering an archive

httparchive.go also provides methods to alter a wprgo archive. Use commands such
as `add`, `merge` and `trim`. These will add new traffic, merge two archives to
create a third, or trim request response pairs by host (`--host`) or path
(`--full-path`). See `--help` for more information.

E.g.

```shell
cd path/to/webpagereplay
go run src/httparchive.go trim /tmp/archive.wprgo --host=example.com  /tmp/trimmed.wprgo
```

## Running unit tests
Run all tests in a specific file. Use '-v' flag to show results.
Note: proxy_test requires more includes than just proxy.go.
```shell
cd path/to/webpagereplay/src/webpagereplay
go test archive_test.go archive.go
go test transformers_test.go transformers.go
go test proxy_test.go proxy.go transformers.go archive.go
```

Run all tests in `webpagereplay` module.
```shell
cd path/to/webpagereplay/src/webpagereplay
go test -run ''
```
Or
```shell
cd path/to/webpagereplay
go test -v go.chromium.org/webpagereplay/src/webpagereplay
```

## Generate public key hash for --ignore-certificate-errors-spki-list
wpr_public_hash.txt is generated from wpr_cert.pem using the command below.
```shell
openssl x509 -noout -pubkey -in wpr_cert.pem | \
openssl pkey -pubin -outform der | \
openssl dgst -sha256 -binary | \
base64
```

## Debugging WPR
The run_benchmark and record_wpr tools will build and invoke WPR from this directory if they
are run with the --use-local-wpr flag.

## Contribute

You can file bugs [here](https://g-issues.chromium.org/issues/new?component=1456169).
Patches welcome!
Please run scripts/upload_new_binaries.py after making changes to go code.
