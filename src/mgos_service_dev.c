/*
 * Copyright (c) 2014-2020 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>

#include "mgos.h"
#include "mgos_rpc.h"
#include "mgos_vfs.h"
#include "mgos_vfs_dev.h"

static void rpc_dev_create_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                   struct mg_rpc_frame_info *fi,
                                   struct mg_str args) {
  char *name = NULL, *type = NULL, *opts = NULL;

  json_scanf(args.p, args.len, ri->args_fmt, &name, &type, &opts);

  if (name == NULL || type == NULL) {
    mg_rpc_send_errorf(ri, 400, "name and type are required");
    ri = NULL;
    goto clean;
  }

  if (!mgos_vfs_dev_create_and_register(type, (opts ? opts : ""), name)) {
    mg_rpc_send_errorf(ri, 500, "dev creation failed");
    ri = NULL;
    goto clean;
  }

  mg_rpc_send_responsef(ri, NULL);
  ri = NULL;

clean:
  free(name);
  free(type);
  free(opts);
  (void) cb_arg;
  (void) fi;
}

static void rpc_dev_read_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                 struct mg_rpc_frame_info *fi,
                                 struct mg_str args) {
  char *dev_name = NULL, *data = NULL;
  enum mgos_vfs_dev_err r;
  unsigned long offset = 0, len = 0;
  struct mgos_vfs_dev *dev = NULL;

  json_scanf(args.p, args.len, ri->args_fmt, &dev_name, &offset, &len);

  if (dev_name == NULL || len == 0) {
    mg_rpc_send_errorf(ri, 400, "name and len are required");
    goto clean;
  }

  if ((dev = mgos_vfs_dev_open(dev_name)) == NULL) {
    mg_rpc_send_errorf(ri, 500, "dev open failed");
    goto clean;
  }

  if ((data = malloc(len)) == NULL) {
    mg_rpc_send_errorf(ri, 500, "out of memory");
    goto clean;
  }

  if ((r = mgos_vfs_dev_read(dev, offset, len, data)) != 0) {
    mg_rpc_send_errorf(ri, 500, "read error: %d", r);
    goto clean;
  }

  mg_rpc_send_responsef(ri, "{data: %V}", data, len);

clean:
  mgos_vfs_dev_close(dev);
  free(dev_name);
  free(data);
  (void) cb_arg;
  (void) fi;
}

static void rpc_dev_write_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                  struct mg_rpc_frame_info *fi,
                                  struct mg_str args) {
  char *dev_name = NULL, *data = NULL;
  enum mgos_vfs_dev_err r;
  unsigned long offset = 0, erase_len = 0;
  int len = 0;
  struct mgos_vfs_dev *dev = NULL;

  json_scanf(args.p, args.len, ri->args_fmt, &dev_name, &offset, &data, &len,
             &erase_len);

  if (dev_name == NULL || data == NULL) {
    mg_rpc_send_errorf(ri, 400, "name and data are required");
    goto clean;
  }

  if ((dev = mgos_vfs_dev_open(dev_name)) == NULL) {
    mg_rpc_send_errorf(ri, 500, "dev open failed");
    goto clean;
  }

  if (erase_len > 0) {
    if ((r = mgos_vfs_dev_erase(dev, offset, erase_len)) != 0) {
      mg_rpc_send_errorf(ri, 500, "erase error: %d", r);
      goto clean;
    }
  }

  if ((r = mgos_vfs_dev_write(dev, offset, len, data)) != 0) {
    mg_rpc_send_errorf(ri, 500, "write error: %d", r);
    goto clean;
  }

  mg_rpc_send_responsef(ri, NULL);

clean:
  mgos_vfs_dev_close(dev);
  free(dev_name);
  free(data);
  (void) cb_arg;
  (void) fi;
}

static void rpc_dev_erase_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                  struct mg_rpc_frame_info *fi,
                                  struct mg_str args) {
  char *dev_name = NULL;
  enum mgos_vfs_dev_err r;
  unsigned long offset = 0, len = 0;
  struct mgos_vfs_dev *dev = NULL;

  json_scanf(args.p, args.len, ri->args_fmt, &dev_name, &offset, &len);

  if (dev_name == NULL || len == 0) {
    mg_rpc_send_errorf(ri, 400, "name and len are required");
    goto clean;
  }

  if ((dev = mgos_vfs_dev_open(dev_name)) == NULL) {
    mg_rpc_send_errorf(ri, 500, "dev open failed");
    goto clean;
  }

  if ((r = mgos_vfs_dev_erase(dev, offset, len)) != 0) {
    mg_rpc_send_errorf(ri, 500, "erase error: %d", r);
    goto clean;
  }

  mg_rpc_send_responsef(ri, NULL);

clean:
  mgos_vfs_dev_close(dev);
  free(dev_name);
  (void) cb_arg;
  (void) fi;
}

static void rpc_dev_remove_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                   struct mg_rpc_frame_info *fi,
                                   struct mg_str args) {
  char *name = NULL;

  json_scanf(args.p, args.len, ri->args_fmt, &name);

  if (name == NULL) {
    mg_rpc_send_errorf(ri, 400, "name is required");
    ri = NULL;
    goto clean;
  }

  if (!mgos_vfs_dev_unregister(name)) {
    mg_rpc_send_errorf(ri, 500, "dev removal failed");
    ri = NULL;
    goto clean;
  }

  mg_rpc_send_responsef(ri, NULL);
  ri = NULL;

clean:
  free(name);
  (void) cb_arg;
  (void) fi;
}

static void rpc_dev_get_info_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                     struct mg_rpc_frame_info *fi,
                                     struct mg_str args) {
  size_t size;
  struct mbuf mb;
  char *name = NULL;
  mbuf_init(&mb, 0);
  struct json_out out = JSON_OUT_MBUF(&mb);
  struct mgos_vfs_dev *dev = NULL;
  size_t erase_sizes[MGOS_VFS_DEV_NUM_ERASE_SIZES] = {0};

  json_scanf(args.p, args.len, ri->args_fmt, &name);

  if (name == NULL) {
    mg_rpc_send_errorf(ri, 400, "name is required");
    ri = NULL;
    goto clean;
  }

  if ((dev = mgos_vfs_dev_open(name)) == NULL) {
    mg_rpc_send_errorf(ri, 500, "dev open failed");
    goto clean;
  }

  size = mgos_vfs_dev_get_size(dev);
  json_printf(&out, "{size: %llu", (unsigned long long) size);

  if (mgos_vfs_dev_get_erase_sizes(dev, erase_sizes) == MGOS_VFS_DEV_ERR_NONE) {
    json_printf(&out, ", erase_sizes: [");
    for (int i = 0; i < MGOS_VFS_DEV_NUM_ERASE_SIZES; i++) {
      if (erase_sizes[i] == 0) break;
      json_printf(&out, "%s%d", (i == 0 ? "" : ", "), (int) erase_sizes[i]);
    }
    json_printf(&out, "]");
  }
  json_printf(&out, "}");

  mg_rpc_send_responsef(ri, "%.*s", (int) mb.len, mb.buf);

clean:
  mgos_vfs_dev_close(dev);
  mbuf_free(&mb);
  free(name);
  (void) cb_arg;
  (void) fi;
}

bool mgos_rpc_service_dev_init(void) {
  struct mg_rpc *c = mgos_rpc_get_global();
  mg_rpc_add_handler(c, "Dev.Create", "{name: %Q, type: %Q, opts: %Q}",
                     rpc_dev_create_handler, NULL);
  mg_rpc_add_handler(c, "Dev.Read", "{name: %Q, offset: %lu, len: %lu}",
                     rpc_dev_read_handler, NULL);
  mg_rpc_add_handler(c, "Dev.Write",
                     "{name: %Q, offset: %lu, data: %V, erase_len: %lu}",
                     rpc_dev_write_handler, NULL);
  mg_rpc_add_handler(c, "Dev.Erase", "{name: %Q, offset: %lu, len: %lu}",
                     rpc_dev_erase_handler, NULL);
  mg_rpc_add_handler(c, "Dev.Remove", "{name: %Q}", rpc_dev_remove_handler,
                     NULL);
  mg_rpc_add_handler(c, "Dev.GetInfo", "{name: %Q}", rpc_dev_get_info_handler,
                     NULL);
  return true;
}
