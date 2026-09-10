#!/usr/bin/env python3
"""Small dependency-free GPT + FAT32 image builder for NexusOS.

It deliberately supports the subset NexusOS needs: 512-byte sectors, FAT32,
short 8.3 names, regular files and directories. It creates real GPT
partitions and real FAT32 volumes; it is not a folder-image mock.
"""
import argparse, os, struct, zlib

SECTOR=512
BOOT_MB=64
SYSTEM_MB=64
ALIGN=2048
BOOT_GUID=bytes.fromhex('C12A7328F81F11D2BA4B00A0C93EC93B')
SYSTEM_GUID=bytes.fromhex('4E5853595354454D5041525430303100')
USER_GUID=bytes.fromhex('4E585355534552444154413030303100')

def guid_le(g):
    # GPT GUID on disk: first 3 fields little endian.
    b=bytearray(g); b[0:4]=reversed(b[0:4]); b[4:6]=reversed(b[4:6]); b[6:8]=reversed(b[6:8]); return bytes(b)

def align(n,a=ALIGN): return (n+a-1)//a*a

def short_name(name):
    base, dot, ext = name.partition('.')
    base=''.join(c for c in base.upper() if c.isalnum() or c in '_-$~')[:8]
    ext=''.join(c for c in ext.upper() if c.isalnum() or c in '_-$~')[:3]
    return (base.ljust(8)+ext.ljust(3)).encode('ascii')

def write_at(f,lba,data):
    f.seek(lba*SECTOR); f.write(data)

def make_gpt(f,total,parts):
    entries=bytearray(128*128)
    for i,p in enumerate(parts):
        off=i*128
        entries[off:off+16]=guid_le(p['type'])
        # deterministic but unique enough GUIDs for local images
        ug=bytearray(16); struct.pack_into('<I',ug,0,0x4E585300+i+1); ug[4:]=os.urandom(12)
        entries[off+16:off+32]=bytes(ug)
        struct.pack_into('<QQQ',entries,off+32,p['first'],p['last'],0)
        name=p['name'].encode('utf-16le')[:72]
        entries[off+56:off+56+len(name)]=name
    crc=zlib.crc32(entries)&0xffffffff
    first_usable=34; backup_header=total-1; backup_entries=total-33; last_usable=backup_entries-1
    def header(my,alt,entry_lba):
        h=bytearray(SECTOR); h[:8]=b'EFI PART'; struct.pack_into('<II',h,8,0x00010000,92)
        struct.pack_into('<I',h,16,0); struct.pack_into('<I',h,20,0)
        struct.pack_into('<QQQQ',h,24,my,alt,first_usable,last_usable)
        disk_guid=b'NEXUSGPT-0000001'[:16]; h[56:72]=disk_guid
        struct.pack_into('<QIII',h,72,entry_lba,128,128,crc)
        struct.pack_into('<I',h,16,0); hc=zlib.crc32(h[:92])&0xffffffff; struct.pack_into('<I',h,16,hc)
        return bytes(h)
    # protective MBR
    m=bytearray(SECTOR); m[446:462]=b'\x00\x00\x02\x00\xEE\xff\xff\xff\x01\x00\x00\x00'+struct.pack('<I',min(total-1,0xffffffff))
    m[510:512]=b'\x55\xaa'; write_at(f,0,m)
    write_at(f,2,entries); write_at(f,backup_entries,entries)
    write_at(f,1,header(1,backup_header,2)); write_at(f,backup_header,header(backup_header,1,backup_entries))

def format_fat32(f,start,sectors,label,files):
    reserved=32; fats=2
    # One sector per cluster gives >65525 clusters for the fixed 64 MiB volumes.
    spc=1
    # Pick a FAT size that is guaranteed to contain an entry for every
    # possible data cluster. Using equality can oscillate by one sector on
    # some volume sizes, so we converge by the safe >= relation instead.
    fat=max(1,(sectors-reserved)//(spc*128))
    while True:
        clusters=(sectors-reserved-fats*fat)//spc
        need=((clusters+2)*4 + SECTOR-1)//SECTOR
        if fat >= need: break
        fat=need
    data_start=reserved+fats*fat
    clusters=(sectors-data_start)//spc
    if clusters < 65525: raise RuntimeError(f'{label}: FAT32 needs >=65525 clusters, got {clusters}')
    # BPB
    b=bytearray(SECTOR); b[0:3]=b'\xeb\x58\x90'; b[3:11]=b'NEXUSOS '
    struct.pack_into('<HBHBHHBHHHII',b,11,512,spc,reserved,fats,0,0,0xF8,0,0,0,0,sectors if sectors<65536 else 0)
    struct.pack_into('<I',b,32,sectors if sectors>=65536 else 0)
    struct.pack_into('<I',b,36,fat); struct.pack_into('<HHI',b,40,0,0,2)
    struct.pack_into('<HHB',b,48,1,6,0); b[64]=0x80; b[66]=0x29
    struct.pack_into('<I',b,67,0x4E58534F); b[71:82]=label.encode('ascii')[:11].ljust(11,b' '); b[82:90]=b'FAT32   '
    b[510:512]=b'\x55\xaa'; write_at(f,start,b)
    # FSInfo and backup boot sector.
    fs=bytearray(SECTOR); struct.pack_into('<I',fs,0,0x41615252); struct.pack_into('<I',fs,484,0x61417272); struct.pack_into('<I',fs,488,0xffffffff); struct.pack_into('<I',fs,492,max(0,clusters-1)); fs[510:512]=b'\x55\xaa'; write_at(f,start+1,fs); write_at(f,start+6,b)
    # FATs
    fatbuf=bytearray(fat*SECTOR); struct.pack_into('<III',fatbuf,0,0x0FFFFFF8,0x0FFFFFFF,0x0FFFFFFF)
    # Build directories and files, allocate one cluster chain per object.
    next_cluster=3
    dirs={'':2}
    objects=[]
    for path,data in files.items():
        comps=[c for c in path.replace('\\','/').split('/') if c]
        cur='';
        for c in comps[:-1]:
            nxt=(cur+'/'+c).strip('/')
            if nxt not in dirs: dirs[nxt]=next_cluster; next_cluster+=1
            cur=nxt
        objects.append((path,data))
    next_cluster = max(dirs.values()) + 1
    # Directory data buffers keyed by cluster.
    dirbuf={c:bytearray(SECTOR) for c in dirs.values()}
    for dpath,c in dirs.items():
        if dpath=='': continue
        parent='/'.join(dpath.split('/')[:-1]); name=dpath.split('/')[-1]
        pc=dirs[parent]; buf=dirbuf[pc]; pos=next((i for i in range(0,SECTOR,32) if buf[i]==0),0); buf[pos:pos+11]=short_name(name); buf[pos+11]=0x10; struct.pack_into('<H',buf,pos+20,(c>>16)&0xffff); struct.pack_into('<H',buf,pos+26,c&0xffff)
    for path,data in objects:
        parent='/'.join(path.split('/')[:-1]); name=path.split('/')[-1]; pc=dirs[parent]; clusters_needed=max(1,(len(data)+SECTOR-1)//SECTOR); chain=list(range(next_cluster,next_cluster+clusters_needed)); next_cluster+=clusters_needed
        for a,bn in zip(chain,chain[1:]): struct.pack_into('<I',fatbuf,a*4,bn)
        struct.pack_into('<I',fatbuf,chain[-1]*4,0x0FFFFFFF)
        buf=dirbuf[pc]; pos=next((i for i in range(0,SECTOR,32) if buf[i]==0),0); buf[pos:pos+11]=short_name(name); buf[pos+11]=0x20; struct.pack_into('<H',buf,pos+20,(chain[0]>>16)&0xffff); struct.pack_into('<H',buf,pos+26,chain[0]&0xffff); struct.pack_into('<I',buf,pos+28,len(data))
        for i,c in enumerate(chain): write_at(f,start+data_start+(c-2),data[i*SECTOR:(i+1)*SECTOR].ljust(SECTOR,b'\0'))
    for c,buf in dirbuf.items(): write_at(f,start+data_start+(c-2),bytes(buf))
    for base in (start+reserved,start+reserved+fat): write_at(f,base,bytes(fatbuf))

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--out',required=True); ap.add_argument('--userdata-sectors',type=int,required=True); ap.add_argument('--bootloader',required=True); ap.add_argument('--kernel',required=True); ap.add_argument('--config',required=True); args=ap.parse_args()
    boot_sec=BOOT_MB*1024*1024//SECTOR; sys_sec=SYSTEM_MB*1024*1024//SECTOR; user_start=align(ALIGN+boot_sec+sys_sec); total=user_start+args.userdata_sectors+34
    parts=[{'name':'BOOT','type':BOOT_GUID,'first':ALIGN,'last':ALIGN+boot_sec-1},{'name':'SYSTEM','type':SYSTEM_GUID,'first':ALIGN+boot_sec,'last':ALIGN+boot_sec+sys_sec-1},{'name':'USERDATA','type':USER_GUID,'first':user_start,'last':user_start+args.userdata_sectors-1}]
    os.makedirs(os.path.dirname(os.path.abspath(args.out)),exist_ok=True)
    with open(args.out,'wb') as f:
        f.truncate(total*SECTOR); make_gpt(f,total,parts)
        boot_files={'EFI/BOOT/BOOTX64.EFI':open(args.bootloader,'rb').read()}
        sys_files={'KERNEL/KERNEL.ELF':open(args.kernel,'rb').read(),'SYSTEM/CONFIG/SYSTEM.CFG':open(args.config,'rb').read(),
                   'SYSTEM/APPS/README.MD':b'NexusOS .nx package location.\r\n', 'SYSTEM/SERVICES/PKGMGR.MD':b'Nexus Package Manager metadata/discovery foundation.\r\n'}
        user_files={'USERS/README.MD':b'NexusOS users\r\n','HOME/README.MD':b'User home\r\n','APPS/README.MD':b'User applications\r\n','PACKAGES/README.MD':b'User .nx packages\r\n','DOWNLOADS/README.MD':b'Downloads\r\n','DOCUMENTS/README.MD':b'Documents\r\n','CONFIG/README.MD':b'User configuration\r\n'}
        format_fat32(f,parts[0]['first'],boot_sec,'NEXUSBOOT',boot_files); format_fat32(f,parts[1]['first'],sys_sec,'NEXUSSYSTEM',sys_files); format_fat32(f,parts[2]['first'],args.userdata_sectors,'NEXUSUSER',user_files)
    print(args.out); print(f'BOOT=64 MiB SYSTEM=64 MiB USERDATA={args.userdata_sectors*SECTOR/1024/1024:.0f} MiB IMAGE={total*SECTOR/1024/1024:.0f} MiB')
if __name__=='__main__': main()
