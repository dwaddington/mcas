## Establishing backing storage for hstore (or hstore-cc)

The MCAS server configuration, with backend hstore or hstore-cc, can use either devdax or fsdax for storage.
These stores are contained in "namespaces", which the ndctl command manages.

The MCAS build process builds but does not install ndecl.
The binary is under the build directory at ./src/lib/ndctl/ndctl-prefix/src/ndctl/ndctl/ndctl.

Recent versions of Fedora also provide the command, in package ncdtl:

```
sudo dnf install ndctl
```

Using ndctl, one can display all namespaces:

```
ndctl list --namespaces
```


## Establishing devdax storage

Devdax storage is visible (outside of ndctl) as character device files at /dev/dax\*.\*.
The MCAS server configuration uses those nmes directly.

iEstablish namespace 0 as a devdax namespace:

```
sudo ndctl create-namespace -m devdax -e namespace0.0 --align 2M --force
sudo chmod go+rw /dev/dax0.0
```


## Establishing fsdax storage

Fsdax storage is visible (outside ndctl) as block device files /dev/pmem\*.
The MCAS server configuration does not use these; you must format the files as a file system and mount them for MCAS use.
Since we have alreadly used namespace0 for devdax, this examples uses namespace1.
The mount point name need not be pmem1; we could have given it any name.

```
sudo ndctl create-namespace -e namespace1.0 --align 2M --force
sudo mkfs -t ext4 /dev/pmem1
sudo mkdir /mnt/pmem1
sudo mount /dev/pmem1 /mnt/pmem1
sudo chmod go+rw /mnt/pmem1
mkdir /mnt/pmem1/mystuff
```


## Referencing DAX storage in a configuration

In the JSON configuration, you may specify a devdax character special file or an fsdax-backed directory.
This sample iline from a configuration specifies one of each, although - absent special circumsstances - it would be best to
stick to an single type, probably fsdax.

```
"dax_config" : [{ "path": "/dev/dax0.0", "addr": "0x8000000000" }, { "path": "/mnt/pmem1/my-stuff", "addr": "0x9000000000" }],
```

Code in libnupm will expect fsdax if the path is a directory, will expect devdax if the path is a character device, and will throw an exception otherwise.
