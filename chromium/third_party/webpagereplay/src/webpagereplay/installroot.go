// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package webpagereplay

import (
	"crypto/tls"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
)

func getCAName() string {
	return "wpr-local"
}
func getDbPath() string {
	return "sql:" + filepath.Join(os.Getenv("HOME"), ".pki/nssdb")
}

// TODO: Implement root CA installation for platforms other than Linux and Android.
func (i *Installer) InstallRoot(certFile string, keyFile string) error {
	if runtime.GOOS != "linux" {
		Log().Info("Root certificate is skipped", "os", runtime.GOOS)
		return nil
	}
	if i.AndroidDeviceId != "" {
		if runtime.GOOS != "linux" {
			return fmt.Errorf("test root CA for Android is only supported on a Linux host machine")
		}
		Log().Info("Installing test root CA on Android")
		return i.AdbInstallRoot(certFile)
	}
	Log().Info("Loading cert", "path", certFile)
	Log().Info("Loading key", "path", keyFile)
	rootCert, err := tls.LoadX509KeyPair(certFile, keyFile)
	if err != nil {
		return fmt.Errorf("error opening cert or key files: %v", err)
	}
	derBytes := rootCert.Certificate[0]
	CAName := getCAName()
	dbPath := getDbPath()

	Log().Info("Attempting to install root certificate", "path", dbPath)

	i.RemoveRoot()
	cmd := exec.Command(i.CertUtilBinaryPath, "-d", dbPath, "-A", "-n", CAName, "-t", "C,p,p")
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	stdin, err := cmd.StdinPipe()
	if err != nil {
		return err
	}
	if err := cmd.Start(); err != nil {
		return err
	}
	if _, err := stdin.Write(derBytes); err != nil {
		return err
	}
	stdin.Close()
	if err := cmd.Wait(); err != nil {
		return fmt.Errorf("NSS certutil failed: %s\n", err)
	}

	Log().Info("Root certificate should now be installed for NSS (i.e. Chrome)")
	return err
}

func (i *Installer) RemoveRoot() {
	if runtime.GOOS != "linux" {
		Log().Info("Root certificate is skipped", "os", runtime.GOOS)
		return
	}
	if i.AndroidDeviceId != "" {
		if runtime.GOOS != "linux" {
			Log().Info("test root CA for Android is only supported on a Linux host machine")
			return
		}
		Log().Info("Uninstalling test root CA on Android")
		err := i.AdbUninstallRoot()
		if err != nil {
			Log().Error("Remove test root CA on android device failed", "error", err)
		}
		return
	}
	Log().Info("Removing root certificate from NSS (i.e. Chrome)", "name", getCAName())
	// Try to delete any existing certificate. We ignore failures since the
	// root might not yet exist.
	cmd := exec.Command(i.CertUtilBinaryPath, "-d", getDbPath(), "-D", "-n", getCAName())
	cmd.Run()
}
