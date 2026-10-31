"""Retain native culling provenance for independent HMD view clipping."""
import argparse
import difflib
from pathlib import Path
import subprocess
from prepare_capture_patch import COMMIT, modify as diagnostic
from prepare_vr_patch import video, replace


def modify(source):
    source = replace(source,
        '        native.lod=object.texlod;native.culled=cull;native.object=vcop2::trace_object();',
        '''        native.lod=object.texlod;native.culled=cull;native.object=vcop2::trace_object();
        native.cull_reasons=vcop2::CullKnown;
        if(((attr>>17)&1)==0 && (raster->command_buffer[9]&0x00800000))native.cull_reasons|=vcop2::CullBackface;
        if(((attr>>8)&3)==0)native.cull_reasons|=vcop2::CullNonRendering;
        if(raster->master_z_clip!=0xff && int32_t(1.0/min_z)>raster->master_z_clip)native.cull_reasons|=vcop2::CullDepthLimit;
        if(max_z<0)native.cull_reasons|=vcop2::CullBehind;''')
    return replace(source,
        '        frame->complete=false;',
        '        frame->complete=false;frame->raster_only=m_render_test_mode;')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    relative = 'src/mame/sega/model2_v.cpp'
    original = subprocess.check_output(['git', '-C', str(args.source), 'show', f'{COMMIT}:{relative}'], text=True)
    base = video(diagnostic(original))
    base = replace(base, '\telse\n\t\trender_polygons(bitmap, cliprect);', '''\telse if(!vcop2::vr_requested() || std::getenv("VCOP2_CPU_RASTER") || std::getenv("VCOP2_SCENE_DIR"))
        // The VR host replays owned polygons. Retain the software rasterizer
        // for reference diagnostics; it does not participate in game logic.
\t\trender_polygons(bitmap, cliprect);''')
    patch = ''.join(difflib.unified_diff(base.splitlines(True), modify(base).splitlines(True), fromfile='a/'+relative, tofile='b/'+relative))
    args.output.write_text(patch, encoding='utf8', newline='\n')
