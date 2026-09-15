#!/usr/bin/env python3
"""Create a bootable UEFI El Torito ISO for NexusOS.

Dependency-free: creates a FAT16 EFI System Partition image as the El Torito
boot image and a minimal ISO9660 filesystem containing the bootloader/kernel.
"""
from __future__ import annotations
import argparse, math, struct
from pathlib import Path

SECTOR=2048
FAT_SECTOR=512
ESP_MIB=16
ESP_SECTORS=ESP_MIB*1024*1024//FAT_SECTOR
BOOT_CATALOG_LBA=18
PVD_LBA=16
BOOT_RECORD_LBA=17
ROOT_DIR_LBA=19


def die(msg): raise SystemExit(f"ERROR: {msg}")

def put16(b,o,v): struct.pack_into('<H',b,o,v)
def put32(b,o,v): struct.pack_into('<I',b,o,v)

def fat16_image(bootloader: bytes, kernel: bytes) -> bytes:
    sectors=ESP_SECTORS
    reserved=1; fats=2; root_entries=512
    root_secs=(root_entries*32 + FAT_SECTOR-1)//FAT_SECTOR
    fat_secs=128
    data_start=reserved + 2*fat_secs + root_secs
    clusters=sectors-data_start
    if clusters >= 65525: die('ESP is too large for this FAT16 layout')
    img=bytearray(sectors*FAT_SECTOR)
    # BPB
    b=img[:FAT_SECTOR]
    b[0:3]=b'\xEB\x3C\x90'; b[3:11]=b'NEXUSOS '
    put16(b,11,512); b[13]=1; put16(b,14,reserved); b[16]=2
    put16(b,17,root_entries); put16(b,19,sectors); b[21]=0xF8
    put16(b,22,fat_secs); put16(b,24,63); put16(b,26,255); put32(b,28,0)
    put32(b,32,0); b[36]=0; b[38]=0x29; put32(b,39,0x4E58534F)
    b[43:54]=b'NEXUS EFI  '; b[54:62]=b'FAT16   '; b[510:512]=b'\x55\xAA'
    # FAT tables, reserved clusters 0/1
    fat_off=reserved*FAT_SECTOR
    for fi in range(2):
        off=fat_off+fi*fat_secs*FAT_SECTOR
        struct.pack_into('<HH',img,off,0xFFF8,0xFFFF)
    next_cluster=2
    dirs={'EFI': None, 'EFI/BOOT': None}
    for d in list(dirs):
        dirs[d]=next_cluster; next_cluster+=1
    files=[('EFI/BOOT/BOOTX64.EFI',bootloader),('KERNEL.ELF',kernel)]
    allocations=[]
    for path,data in files:
        need=max(1,math.ceil(len(data)/FAT_SECTOR))
        chain=list(range(next_cluster,next_cluster+need)); next_cluster+=need
        allocations.append((path,data,chain))
    # directory clusters
    dir_chains={d:[c] for d,c in dirs.items()}
    # link FAT entries
    def fat_set(c,v):
        for fi in range(2):
            o=fat_off+fi*fat_secs*FAT_SECTOR+c*2
            struct.pack_into('<H',img,o,v & 0xFFFF)
    for d,c in dirs.items(): fat_set(c,0xFFFF)
    for _,data,chain in allocations:
        for a,n in zip(chain,chain[1:]): fat_set(a,n)
        fat_set(chain[-1],0xFFFF)
    def short(name):
        base,_,ext=name.upper().partition('.')
        return (base[:8].ljust(8)+ext[:3].ljust(3)).encode('ascii')
    def entry(name,attr,cluster,size=0):
        e=bytearray(32); e[:11]=short(name); e[11]=attr
        struct.pack_into('<H',e,26,cluster); struct.pack_into('<I',e,28,size); return e
    def dir_write(cluster, entries):
        off=(data_start+(cluster-2))*FAT_SECTOR
        blob=b''.join(entries)+b'\x00'*FAT_SECTOR
        img[off:off+FAT_SECTOR]=blob[:FAT_SECTOR]
    dir_write(dirs['EFI'], [entry('.',0x10,dirs['EFI']),entry('..',0x10,0),entry('BOOT',0x10,dirs['EFI/BOOT'])])
    boot_file=allocations[0]; kernel_file=allocations[1]
    dir_write(dirs['EFI/BOOT'], [entry('.',0x10,dirs['EFI/BOOT']),entry('..',0x10,dirs['EFI']),entry('BOOTX64.EFI',0x20,boot_file[2][0],len(boot_file[1]))])
    # root directory lives in fixed root area
    root_off=(reserved+2*fat_secs)*FAT_SECTOR
    root=entry('NEXUSOS',0x08,0)+entry('EFI',0x10,dirs['EFI'])+entry('KERNEL.ELF',0x20,kernel_file[2][0],len(kernel_file[1]))+b'\x00'*32
    img[root_off:root_off+len(root)]=root
    for _,data,chain in allocations:
        for i,c in enumerate(chain):
            off=(data_start+(c-2))*FAT_SECTOR
            chunk=data[i*FAT_SECTOR:(i+1)*FAT_SECTOR]
            img[off:off+len(chunk)]=chunk
    return bytes(img)

def both_endian32(v): return struct.pack('>I',v)+struct.pack('<I',v)
def both_endian16(v): return struct.pack('>H',v)+struct.pack('<H',v)
def dir_record(extent,size,flags,name,dt=b'\x00'*7):
    nb=name if isinstance(name,bytes) else name.encode('ascii')
    nlen=len(nb); length=33+nlen+(nlen%2==0)
    r=bytearray(length); r[0]=length; r[1]=0; r[2:10]=both_endian32(extent); r[10:18]=both_endian32(size)
    r[18:25]=dt; r[25]=flags; r[26]=0; r[27]=0; r[28:32]=struct.pack('<HH',1,1); r[32]=nlen; r[33:33+nlen]=nb
    return bytes(r)

def make_iso(bootloader: bytes, kernel: bytes, out: Path):
    esp=fat16_image(bootloader,kernel)
    esp_sectors_2048=math.ceil(len(esp)/SECTOR)
    # Put ESP image after the boot catalog, aligned on a 2048-byte boundary.
    esp_lba=32
    # ISO data files after directories.
    root_lba=34
    efi_lba=35
    bootdir_lba=36
    kernel_lba=37
    boot_lba=kernel_lba+math.ceil(len(kernel)/SECTOR)
    total=max(boot_lba+math.ceil(len(bootloader)/SECTOR), esp_lba+esp_sectors_2048)+1
    iso=bytearray(total*SECTOR)
    # Primary Volume Descriptor
    p=memoryview(iso)[PVD_LBA*SECTOR:(PVD_LBA+1)*SECTOR]
    p[0]=1; p[1:6]=b'CD001'; p[6]=1; p[8:40]=b'NEXUSOS'.ljust(32,b' ')
    p[40:72]=b'NEXUSOS UEFI BOOT'.ljust(32,b' '); p[80:88]=both_endian32(total)
    p[120:124]=both_endian16(1); p[124:128]=both_endian16(1)
    p[128:132]=both_endian16(SECTOR); p[132:140]=both_endian32(0)
    p[140:144]=struct.pack('<I',0); p[144:148]=struct.pack('<I',0)
    p[156:190]=dir_record(root_lba,SECTOR,2,b'\x00')
    p[190:318]=b' ' * 128
    # Boot Record
    br=memoryview(iso)[BOOT_RECORD_LBA*SECTOR:(BOOT_RECORD_LBA+1)*SECTOR]
    br[0]=0; br[1:6]=b'CD001'; br[6]=1; br[7:39]=b'EL TORITO SPECIFICATION'.ljust(32,b' '); put32(br,71,BOOT_CATALOG_LBA)
    # Boot catalog validation entry (EFI platform)
    cat=bytearray(SECTOR); cat[0]=1; cat[1]=0xEF; cat[4:28]=b'NEXUSOS EFI BOOT CATALOG'[:24].ljust(24,b' ')
    # checksum over first 32 bytes as little-endian words
    cat[30:32]=b'\x55\xAA'
    s=sum(struct.unpack_from('<16H',cat,0)) & 0xFFFF
    struct.pack_into('<H',cat,28,(-s)&0xFFFF)
    # Default no-emulation boot entry; sector count is in 512-byte units.
    cat[32]=0x88; cat[33]=0; put16(cat,38,len(esp)//512); put32(cat,40,esp_lba)
    iso[BOOT_CATALOG_LBA*SECTOR:(BOOT_CATALOG_LBA+1)*SECTOR]=cat
    # Directories
    def write_dir(lba, records):
        blob=b''.join(records)+b'\x00'; iso[lba*SECTOR:lba*SECTOR+len(blob)]=blob
    dot=dir_record(root_lba,SECTOR,2,b'\x00'); dotdot=dir_record(root_lba,SECTOR,2,b'\x01')
    write_dir(root_lba,[dot,dotdot,dir_record(efi_lba,SECTOR,2,b'EFI'),dir_record(kernel_lba,len(kernel),0,b'KERNEL.ELF')])
    write_dir(efi_lba,[dir_record(efi_lba,SECTOR,2,b'\x00'),dir_record(root_lba,SECTOR,2,b'\x01'),dir_record(bootdir_lba,SECTOR,2,b'BOOT')])
    write_dir(bootdir_lba,[dir_record(bootdir_lba,SECTOR,2,b'\x00'),dir_record(efi_lba,SECTOR,2,b'\x01'),dir_record(boot_lba,len(bootloader),0,b'BOOTX64.EFI')])
    iso[kernel_lba*SECTOR:kernel_lba*SECTOR+len(kernel)]=kernel
    iso[boot_lba*SECTOR:boot_lba*SECTOR+len(bootloader)]=bootloader
    # Store the complete FAT16 ESP image in the ISO as the El Torito boot image.
    off=esp_lba*SECTOR; iso[off:off+len(esp)]=esp
    # Volume descriptor terminator
    term=memoryview(iso)[19*SECTOR:20*SECTOR]; term[0]=255; term[1:6]=b'CD001'; term[6]=1
    out.parent.mkdir(parents=True,exist_ok=True); out.write_bytes(iso)
    # Basic structural checks.
    data=out.read_bytes()
    if data[PVD_LBA*SECTOR+1:PVD_LBA*SECTOR+6] != b'CD001': die('invalid ISO9660 primary volume descriptor')
    if data[BOOT_RECORD_LBA*SECTOR+1:BOOT_RECORD_LBA*SECTOR+6] != b'CD001': die('invalid El Torito boot record')
    if struct.unpack_from('<I',data,BOOT_RECORD_LBA*SECTOR+71)[0] != BOOT_CATALOG_LBA: die('invalid boot catalog pointer')
    if data[BOOT_CATALOG_LBA*SECTOR+0] != 1 or data[BOOT_CATALOG_LBA*SECTOR+1] != 0xEF: die('invalid EFI boot catalog validation entry')
    if data[BOOT_CATALOG_LBA*SECTOR+32] != 0x88: die('invalid boot entry')
    if struct.unpack_from('<H',data,BOOT_CATALOG_LBA*SECTOR+38)[0] != len(esp)//512: die('boot image sector count mismatch')
    if struct.unpack_from('<I',data,BOOT_CATALOG_LBA*SECTOR+40)[0] != esp_lba: die('boot image LBA mismatch')
    print(f'created {out} ({len(data)} bytes)')
    print(f'EFI boot image: FAT16 {ESP_MIB} MiB @ ISO LBA {esp_lba}')
    print('ISO9660: root/EFI/BOOT/BOOTX64.EFI + root/KERNEL.ELF')

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--bootloader',required=True); ap.add_argument('--kernel',required=True); ap.add_argument('--out',required=True)
    a=ap.parse_args(); bl=Path(a.bootloader); ke=Path(a.kernel)
    if not bl.is_file() or bl.stat().st_size==0: die(f'missing bootloader: {bl}')
    if not ke.is_file() or ke.stat().st_size==0: die(f'missing kernel: {ke}')
    make_iso(bl.read_bytes(),ke.read_bytes(),Path(a.out))
if __name__=='__main__': main()
