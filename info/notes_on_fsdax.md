hstore can be used with devdax, and possibly with fsdax.

ndctl, to create or destroy a devdax namespace, is built but not installed by MCAS build.
The binary is in mcas/build/src/lib/ndctl/ndctl-prefix/src/ndctl/ndctl/ndctl.

Fedora 31 also provides ndctl:

```
sudo dnf install ndctl
```

```
sudo ndctl create-namespace -m devdax --align 2M --force
sudo ndctl create-namespace -m devdax -e namespace0.0 --align 2M --force
sudo chmod ugo+rw /dev/dax0.0
sudo ndctl destroy-namespace namespace0.0 --force
```

ndctl can also create/destroy an fsdax namespace

```
sudo ndctl create-namespace --align 2M --force
sudo ndctl create-namespace -e namespace0.0 --align 2M --force
```

Following the fsdax namespace create, a "post" at https://pmem.io/2018/05/15/using_persistent_memory_devices_with_the_linux_device_mapper.html proposes (but does not justify the parameters for)

```
sudo mkfs.ext4 -b 4096 -E stride=512 -F /dev/pmem0
```

This also seems to work:

```
sudo mkfs -t ext4 /dev/pmem0
```

Then mount, to expose files:

```
sudo mkdir /mnt/pmem0
sudo mount /dev/pmem0 /mnt/pmem0
sudo chmod go+w /mnt/pmem0
```

Allocate a file of suitable size (in this case, 16 GiB):
```
dd if=/dev/zero of=/mnt/pmem0/hstore-test bs=1048576 count=16384
```

In the JSON configuration, specify a file backed by fsdax, rather than a /dev/dax character device, as the "path:"

'''
"dax_config" : [{ "region_id": 0, "path": "/mnt/pmem0/hstore-test", "addr": "0x9000000000" }],
'''

Code in libnupm will expect fsdax if the path is a regular file, will expect devdax if the path is a character device, and will throw an exception otherwise.
