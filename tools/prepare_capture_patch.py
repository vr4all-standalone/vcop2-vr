"""Export source-pinned, noninvasive Model 2 diagnostic capture hooks."""
import argparse
import difflib
from pathlib import Path
import subprocess

COMMIT = 'f34f02505e32c1993c6a782b6814232cbfc74e36'
RELATIVE = 'src/mame/sega/model2_v.cpp'


def modify(source):
    def replace(old, new):
        nonlocal source
        if source.count(old) != 1:
            raise ValueError('Capture hook anchor must match exactly once: ' + old[:80])
        source = source.replace(old, new)

    replace('#include "model2rd.ipp"', '#include "model2rd.ipp"\n#include "../../lib/vcop2/model2_trace.h"')
    replace('void model2_state::render_frame_start()\n{', '''void model2_state::render_frame_start()
{
    if (std::string_view(machine().system().name) == "vcop2") {
        vcop2::trace_begin(m_screen->frame_number(), machine().time().as_double());
    }''')
    replace('\traster->polygon_z = zvalue;', '''\traster->polygon_z = zvalue;
    if (vcop2::trace_active()) {
        vcop2::NativePolygon native;
        native.count=NumVerts;native.attr=attr;native.window=raster->cur_window;
        native.sort=float_to_zval(zvalue,raster->z_adjust);native.luma=object.luma;
        native.lod=object.texlod;native.culled=cull;native.object=vcop2::trace_object();
        native.focus={m_geo->focus.x,m_geo->focus.y};
        native.center={float(raster->center[raster->center_sel][0]),float(raster->center[raster->center_sel][1])};
        for(unsigned i=0;i<4;i++){native.header[i]=object.texheader[i];native.viewport[i]=raster->viewport[i];}
        for(unsigned i=0;i<12;i++)native.matrix[i]=m_geo->matrix[i];
        for(unsigned i=0;i<NumVerts;i++){const auto& v=object.v[i];native.vertices[i]={v.x,v.y,v.p[0],v.p[1],v.p[2]};}
        vcop2::trace_capture().append(native);
    }''')
    replace('\tu32  obc = *input++;     /* Object Count */', '\tu32  obc = *input++;     /* Object Count */\n\tif(vcop2::trace_active())vcop2::trace_object()=oba;')
    replace('u32 *model2_state::geo_direct_data(geo_state *geo, u32 opcode, u32 *input)\n{', '''u32 *model2_state::geo_direct_data(geo_state *geo, u32 opcode, u32 *input)
{
    if(vcop2::trace_active())vcop2::trace_object()=0xffffffff;''')
    replace('\t/* tell the rasterizer we\'re starting a frame */', '''    if(vcop2::trace_geometry_pending()) {
        // Freeze resources at the actual native draw, after vblank IRQ writes.
        // Raw pre-clipping polygons have been retained since geo_parse.
        try {
            auto& capture=vcop2::trace_capture();
            capture.display(m_crtc_xoffset,m_crtc_yoffset,m_screen->visible_area().width(),m_screen->visible_area().height());
            capture.bytes("texture0",&m_textureram0[0],m_textureram0.bytes());
            capture.bytes("texture1",&m_textureram1[0],m_textureram1.bytes());
            capture.bytes("palette",m_palram.get(),0x4000);
            capture.bytes("colorxlat",m_colorxlat.get(),0xc000);
            capture.bytes("luma",m_lumaram.get(),0x8000);
            capture.bytes("gamma",m_gamma_table,sizeof(m_gamma_table));
            capture.bytes("workram",&m_workram[0],m_workram.bytes());
            vcop2::write_trace(*capture.finish(),std::getenv("VCOP2_SCENE_DIR"));
        } catch(const std::exception& error){osd_printf_error("VCOP2 capture failed: %s\\n",error.what());}
        vcop2::trace_geometry_pending()=false;
    }
    if(!vcop2::trace_pending_raster().empty()) {
        try {vcop2::trace_raster("background",m_screen->visible_area().width(),m_screen->visible_area().height(),[&](int x,int y){return uint32_t(bitmap.pix(y,x));});}
        catch(const std::exception& error){osd_printf_error("VCOP2 capture failed: %s\\n",error.what());}
    }
\t/* tell the rasterizer we're starting a frame */''')
    replace('\tcopybitmap_trans(bitmap, m_sys24_bitmap, 0, 0, 0, 0, cliprect, 0);\n\n\treturn 0;', '''\tcopybitmap_trans(bitmap, m_sys24_bitmap, 0, 0, 0, 0, cliprect, 0);
    if(!vcop2::trace_pending_raster().empty()) {
        try {
            const int width=m_screen->visible_area().width(),height=m_screen->visible_area().height();
            vcop2::trace_raster("foreground",width,height,[&](int x,int y){return uint32_t(m_sys24_bitmap.pix(y,x));});
            vcop2::trace_raster("native",width,height,[&](int x,int y){return uint32_t(bitmap.pix(y,x));});
            vcop2::commit_trace_rasters();
        } catch(const std::exception& error){osd_printf_error("VCOP2 capture failed: %s\\n",error.what());}
        vcop2::trace_pending_raster().clear();
    }
\n\treturn 0;''')
    replace('\t\tinput = geo_process_command(m_geo.get(), opcode, input, &end_code);\n\t}\n}', '''\t\tinput = geo_process_command(m_geo.get(), opcode, input, &end_code);
\t}
    if(vcop2::trace_active()) {
        vcop2::trace_geometry_pending()=true;
        vcop2::trace_active()=false;
    }
}''')
    return source


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    original = subprocess.check_output(['git', '-C', str(args.source), 'show', f'{COMMIT}:{RELATIVE}'], text=True)
    patch = ''.join(difflib.unified_diff(original.splitlines(keepends=True), modify(original).splitlines(keepends=True), fromfile='a/' + RELATIVE, tofile='b/' + RELATIVE))
    args.output.write_text(patch, encoding='utf-8', newline='\n')
