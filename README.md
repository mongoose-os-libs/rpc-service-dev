# RPC Service - Storage devices

This service provides an ability to manage storage devices..

It provides wrappers for [VFS device interface methods](https://github.com/mongoose-os-libs/vfs-common/blob/385648d692685da59be1800d777839050403f39a/include/mgos_vfs_dev.h#L44-L60) 

## Examples

```
$ mos call Dev.Create '{"name": "sl1", "type": "part", "opts": "{\"dev\": \"sfl0\", \"size\": 262144, \"offset\": 0x108000}"}'

$ mos call Dev.Read '{"name": "sfl0", "offset": 2080768, "len": 128}' | jq -r .data | base64 -d | hexdump -C

```
