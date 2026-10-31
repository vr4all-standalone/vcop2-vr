"""Widen the original reciprocal camera coefficients before native consumers."""
import argparse
import difflib
from pathlib import Path
import subprocess
from prepare_capture_patch import COMMIT, modify as diagnostic
from prepare_vr_patch import driver, video, replace
from prepare_immersive_patch import modify as immersive


def camera_driver(source):
    return replace(source, 'void model2_state::machine_start()\n{', '''void model2_state::machine_start()
{
    if(vcop2::vr_requested() && std::string_view(machine().system().name)=="vcop2") {
        auto& space=m_maincpu->space(AS_PROGRAM);
        space.install_write_tap(0x506044,0x506047,"vcop2_vr_camera_focus",
            [this](offs_t, u32& data, u32 mask) {
                data=vcop2::camera_write(vcop2::CameraFocusAddress,data,mask,
                    [&](uint32_t address){return m_maincpu->space(AS_PROGRAM).read_dword(address);},
                    vcop2::shared_runtime().camera_fov.load());
            });
        space.install_write_tap(0x506140,0x506143,"vcop2_vr_camera_tangent",
            [this](offs_t, u32& data, u32 mask) {
                data=vcop2::camera_write(vcop2::CameraTangentAddress,data,mask,
                    [&](uint32_t address){return m_maincpu->space(AS_PROGRAM).read_dword(address);},
                    vcop2::shared_runtime().camera_fov.load());
            });
    }''')


def panels(source):
    source=replace(source,'frame->raster_only=m_render_test_mode;',
                   'frame->raster_only=m_render_test_mode||vcop2::native_panel_requested();')
    return replace(source,
        'std::getenv("VCOP2_CPU_RASTER") || std::getenv("VCOP2_SCENE_DIR"))',
        'std::getenv("VCOP2_CPU_RASTER") || std::getenv("VCOP2_SCENE_DIR") || vcop2::native_panel_requested())')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();patch=''
    for relative in ['src/mame/sega/model2.cpp','src/mame/sega/model2_v.cpp']:
        base=subprocess.check_output(['git','-C',str(args.source),'show',f'{COMMIT}:{relative}'],text=True)
        if relative.endswith('model2.cpp'):old=driver(base);new=camera_driver(old)
        else:
            old=video(diagnostic(base))
            old=replace(old,'\telse\n\t\trender_polygons(bitmap, cliprect);','''\telse if(!vcop2::vr_requested() || std::getenv("VCOP2_CPU_RASTER") || std::getenv("VCOP2_SCENE_DIR"))
        // The VR host replays owned polygons. Retain the software rasterizer
        // for reference diagnostics; it does not participate in game logic.
\t\trender_polygons(bitmap, cliprect);''')
            old=immersive(old);new=panels(old)
        patch+=''.join(difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile='a/'+relative,tofile='b/'+relative))
    args.output.write_text(patch,encoding='utf8',newline='\n')
