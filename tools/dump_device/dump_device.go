//
// Copyright (c) 2021 Deomid "rojer" Ryabkov
// All rights reserved
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// A utility to dump raw device data
// Dump an entire device:
//   go run github.com/mongoose-os-libs/rpc-service-dev/tools/dump_device sfl0 dump.bin
// Dump an entire device to stdout:
//   go run github.com/mongoose-os-libs/rpc-service-dev/tools/dump_device sfl0 -
// Dump first 1K:
//   go run github.com/mongoose-os-libs/rpc-service-dev/tools/dump_device sfl0 0 1024 -
//
// Can use the same --port settings as accepted by mos, for example:
//   o run github.com/mongoose-os-libs/rpc-service-dev/tools/dump_device --port ws://192.168.11.86/rpc sfl0 0 1024 dump.bin

package main

import (
	"context"
	"encoding/base64"
	"fmt"
	"os"
	"strconv"
	"time"

	"github.com/juju/errors"
	flag "github.com/spf13/pflag"
	glog "k8s.io/klog/v2"

	"github.com/mongoose-os/mos/cli/devutil"
	"github.com/mongoose-os/mos/cli/flags"
)

func main() {
	glog.InitFlags(nil)
	flag.Parse()
	if err := DumpDevice(context.Background(), flag.Args()); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %s\n", errors.ErrorStack(err))
		os.Exit(1)
	}
}

func DumpDevice(ctx context.Context, args []string) error {
	var err error
	var offset, length int64
	var dev, fname string
	switch len(args) {
	case 0:
		fallthrough
	case 1:
		return errors.Errorf("usage: dump_device name [offset length] output_file")
	case 2:
		// Nothing, will auto-detect the size and read entire flash.
		dev = args[0]
		fname = args[1]
	case 4:
		dev = args[0]
		offset, err = strconv.ParseInt(args[1], 0, 64)
		if err != nil {
			return errors.Annotatef(err, "invalid address")
		}
		length, err = strconv.ParseInt(args[2], 0, 64)
		if err != nil {
			return errors.Annotatef(err, "invalid length")
		}
		fname = args[3]
	default:
		return errors.Errorf("invalid arguments")
	}

	devConn, err := devutil.CreateDevConnFromFlags(ctx)
	if err != nil {
		return errors.Trace(err)
	}

	if length == 0 {
		getInfoArgs := struct {
			Name string `json:"name"`
		}{Name: dev}
		getInfoResp := struct {
			Size int64 `json:"size"`
		}{}
		if err := devConn.Call(ctx, "Dev.GetInfo", &getInfoArgs, &getInfoResp); err != nil {
			return errors.Annotatef(err, "unable to get size of the device")
		}
		length = getInfoResp.Size
	}

	glog.Infof("%s %d %d %s", dev, offset, length, fname)

	ow := os.Stdout
	if fname != "-" {
		ow, err = os.OpenFile(fname, os.O_RDWR|os.O_CREATE|os.O_TRUNC, 0644)
		if err != nil {
			return errors.Annotatef(err, "failed to open output file")
		}
	}
	lastReport := time.Now()
	for numRead := int64(0); numRead < length; {
		readLen := length - numRead
		if readLen > int64(*flags.ChunkSize) {
			readLen = int64(*flags.ChunkSize)
		}
		readArgs := struct {
			Name   string `json:"name"`
			Offset int64  `json:"offset"`
			Len    int64  `json:"len"`
		}{
			Name:   dev,
			Offset: offset,
			Len:    readLen,
		}
		readResp := struct {
			Data string `json:"data"`
		}{}
		if err := devConn.Call(ctx, "Dev.Read", &readArgs, &readResp); err != nil {
			return errors.Annotatef(err, "failed to read %q %d @ %d", dev, readLen, offset)
		}
		decoded, err := base64.StdEncoding.DecodeString(readResp.Data)
		if err != nil {
			return errors.Annotatef(err, "failed to decode %q %d @ %d", dev, readLen, offset)
		}
		if _, err = ow.Write(decoded); err != nil {
			return errors.Annotatef(err, "failed to write")
		}
		offset += readLen
		numRead += readLen
		if numRead%65536 == 0 || time.Since(lastReport) > 5*time.Second {
			glog.Infof("%d of %d (%.2f%%)", numRead, length, float64(numRead)*100.0/float64(length))
			lastReport = time.Now()
		}
	}
	glog.Infof("Done")
	return nil
}
