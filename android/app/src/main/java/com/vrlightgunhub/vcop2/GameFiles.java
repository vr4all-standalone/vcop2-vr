// SPDX-License-Identifier: GPL-2.0-or-later
package com.vrlightgunhub.vcop2;
import java.io.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;

/** Validate original packed ROMs in place; never unpack game data into app state. */
public final class GameFiles {
    static final class Rom {
        final String name, sha1; final long size, crc;
        Rom(String name, long size, long crc, String sha1) {
            this.name=name; this.size=size; this.crc=crc; this.sha1=sha1;
        }
    }
    private static final Rom[] REQUIRED = {
        new Rom("epr-18524.12", 524288, 0x1858988bL, "2979f8470cc31e6c5c32c6fec1a87dbd29b52309"),
        new Rom("epr-18525.13", 524288, 0x0c13df3fL, "6b4188f04aad80b89f1826e8ca47cff763980410"),
        new Rom("epr-18518.14", 524288, 0x7842951bL, "bed4ec9a5e59807d17e5e602bdaf3c68fcba08b6"),
        new Rom("epr-18519.15", 524288, 0x31a30edcL, "caf3c2676508a2ed032d3657ac640a257f04bdd4"),
        new Rom("mpr-18516.10", 2097152, 0xa3928ff0L, "5a9695fb5eda394a1111a05ee5fb9cce29970e91"),
        new Rom("mpr-18517.11", 2097152, 0x4bd73da4L, "a4434bce019729e2148a95e3a6dea38de7f789c1"),
        new Rom("mpr-18514.8", 2097152, 0x791283c5L, "006fb22eefdd9205ede9a74fe53cbffe8c8fd45b"),
        new Rom("mpr-18515.9", 2097152, 0x6ba1ffecL, "70f493aa4eb93edce8dd5b7b532d1f50f81069ce"),
        new Rom("mpr-18522.6", 2097152, 0x61d18536L, "cc467cb26a8fccc48837d000fe9e1c41b0c0f4f9"),
        new Rom("mpr-18523.7", 2097152, 0x61d08dc4L, "40d8231d184582c0fc01ad874371aaec7dfcc337"),
        new Rom("epr-18520.4", 524288, 0x1d4ec5e8L, "44c4b5560d150909342e4182496f136c8c5e2edb"),
        new Rom("epr-18521.5", 524288, 0xb8b3781cL, "11956fe912c34d6a86a6b91d55987f6bead73473"),
        new Rom("mpr-18513.16", 2097152, 0x777a3633L, "edc2798c4d88975ce67b54fc0db008e7d24db6ef"),
        new Rom("mpr-18510.20", 2097152, 0xe83de997L, "8a8597aa31609663869e584cc5fad6e4b84f7dbe"),
        new Rom("mpr-18511.24", 2097152, 0xcae77a4fL, "f21474486f0dc4092cbad4566deea8a952862ab7"),
        new Rom("mpr-18512.25", 2097152, 0xd9bc7e71L, "774eba886083b0dad9a47519c5801e44346312cf"),
        new Rom("epr-18530.30", 524288, 0xac9c8357L, "ad297c7fecaa9b877f0dd31e859983816947e437"),
        new Rom("mpr-18529.31", 2097152, 0xf76715b1L, "258418c1cb37338a694e48f3b48fadfae5f40239"),
        new Rom("mpr-18528.32", 2097152, 0x287a2f9aL, "78ba93ab90322152efc37f7130073b0dc516ef5d"),
        new Rom("mpr-18527.36", 2097152, 0xe6a49314L, "26563f425f2f0906ae9278fe5de02955653d49fe"),
        new Rom("mpr-18526.37", 2097152, 0x6516d9b5L, "8f13cb02c76f7b7cd11f3c3772ff13302d55e9c3"),
        new Rom("opr-14742a.45", 131072, 0x90c6b117L, "f46429fffcee17d056f56d5fe035a33f1fd6c27e"),
        new Rom("opr-14743a.46", 131072, 0xae7f446bL, "5b9f1fc47caf21e061e930c0d72804e4ec8c7bca"),
        new Rom("opr-14744.58", 131072, 0x730ea9e0L, "651f1db4089a400d073b19ada299b4b08b08f372"),
        new Rom("opr-14745.59", 131072, 0x4c934d96L, "e3349ece0e47f684d61ad11bfea4a90602287350"),
        new Rom("opr-14746.62", 131072, 0x2a266cbdL, "34e047a93459406c22acf4c25089d1a4955f94ca"),
        new Rom("opr-14747.63", 131072, 0xa4ad5e19L, "7d7ec300eeb9a8de1590011e37108688c092f329"),
        new Rom("mpr-16310.15", 524288, 0xc078a780L, "0ad5b49774172743e2708b7ca4c061acfe10957a"),
        new Rom("mpr-16311.16", 524288, 0x452a492bL, "88c2f6c2dbfd0c1b39a7bf15c74455fb68c7274e"),
        new Rom("mpr-16312.14", 524288, 0xa25fef5bL, "c6a37856b97f5bc4996cb6b66209f47af392cc38"),
        new Rom("epr-18022.ic2", 65536, 0x0ca70f80L, "edf5ade72d9fa2f4d5f83f9f89e6cecfadd77f56"),
    };
    private static final long MAX_ARCHIVE_BYTES=128L*1024*1024;
    private static final int MAX_MEMBERS=4096;
    private GameFiles() {}

    public static void validate(File game, File billboard) throws IOException {
        if(game==null || !game.isFile()) throw new IOException("Missing Download/vcop2.zip. Copy your owned original arcade ROM archive to Download.");
        File[] paths=billboard!=null && billboard.isFile()?new File[]{game,billboard}:new File[]{game};
        validateArchives(paths, REQUIRED);
    }

    static void validateArchives(File[] paths, Rom[] requirements) throws IOException {
        Map<String,Rom> wanted=new HashMap<>();
        for(Rom rom:requirements) wanted.put(rom.name,rom);
        Set<String> found=new HashSet<>();
        for(File path:paths) {
            if(!path.isFile() || !path.canRead()) throw new IOException("Cannot read "+path.getName()+". Allow game file access and retry.");
            if(path.length()>MAX_ARCHIVE_BYTES) throw new IOException("ROM archive is too large: "+path.getName());
            try(ZipFile zip=new ZipFile(path)) {
                if(zip.size()>MAX_MEMBERS) throw new IOException("Too many ZIP members in "+path.getName());
                Set<String> seen=new HashSet<>();
                Enumeration<? extends ZipEntry> entries=zip.entries();
                while(entries.hasMoreElements()) {
                    ZipEntry entry=entries.nextElement(); String name=entry.getName();
                    if(name.isEmpty() || name.equals(".") || name.equals("..") || name.indexOf('/')>=0 || name.indexOf('\\')>=0 || name.indexOf(':')>=0 || name.indexOf('\0')>=0)
                        throw new IOException("Unsafe ZIP member: "+name);
                    String key=name.toLowerCase(Locale.ROOT);
                    if(!seen.add(key)) throw new IOException("Duplicate ZIP member: "+name);
                    Rom rom=wanted.get(key);
                    if(rom==null) continue;
                    if(entry.isDirectory() || entry.getSize()!=rom.size) throw new IOException("Wrong ROM size: "+rom.name);
                    MessageDigest sha;
                    try { sha=MessageDigest.getInstance("SHA-1"); }
                    catch(NoSuchAlgorithmException error) { throw new IOException("SHA-1 validation is unavailable",error); }
                    CRC32 crc=new CRC32(); byte[] buffer=new byte[16384]; long count=0;
                    try(InputStream in=zip.getInputStream(entry)) {
                        int bytes;
                        while((bytes=in.read(buffer,0,(int)Math.min(buffer.length,rom.size-count+1)))!=-1) {
                            if(bytes==0) continue;
                            count+=bytes;
                            if(count>rom.size) throw new IOException("Oversized ROM payload: "+rom.name);
                            sha.update(buffer,0,bytes); crc.update(buffer,0,bytes);
                        }
                    }
                    if(count!=rom.size || crc.getValue()!=rom.crc || !hex(sha.digest()).equals(rom.sha1)) throw new IOException("ROM checksum mismatch: "+rom.name+". Copy a complete, unmodified archive.");
                    found.add(key);
                }
            } catch(ZipException error) {
                throw new IOException("Cannot read "+path.getName()+": invalid, corrupt or encrypted ZIP ("+error.getMessage()+").",error);
            }
        }
        for(Rom rom:requirements) if(!found.contains(rom.name)) {
            String source=rom.name.equals("epr-18022.ic2")?"Download/segabill.zip (billboard device firmware)":"Download/vcop2.zip";
            throw new IOException("Missing ROM: "+rom.name+". Required in "+source+" or a complete vcop2.zip.");
        }
    }
    private static String hex(byte[] bytes) {
        char[] symbols="0123456789abcdef".toCharArray(); char[] result=new char[bytes.length*2];
        for(int i=0;i<bytes.length;i++) { result[i*2]=symbols[(bytes[i]&255)>>>4]; result[i*2+1]=symbols[bytes[i]&15]; }
        return new String(result);
    }
}
