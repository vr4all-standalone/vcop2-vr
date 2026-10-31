package com.vrlightgunhub.vcop2;
import java.io.File;
public final class GameFilesHarness {
    public static void main(String[] args) throws Exception {
        File[] paths = new File[args.length-1];
        for (int i=1; i<args.length; i++) paths[i-1] = new File(args[i]);
        if (args[0].equals("real")) GameFiles.validate(paths[0], paths.length>1?paths[1]:null);
        else GameFiles.validateArchives(paths, new GameFiles.Rom[] {
            new GameFiles.Rom("game.rom",3,0x352441c2L,"a9993e364706816aba3e25717850c26c9cd0d89d"),
            new GameFiles.Rom("board.rom",3,0x352441c2L,"a9993e364706816aba3e25717850c26c9cd0d89d")
        });
    }
}
