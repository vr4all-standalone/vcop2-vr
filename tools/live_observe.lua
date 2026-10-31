-- Read-only original state observer for live VR input integration.
local root=assert(os.getenv('VCOP2_RESEARCH_DIR'))
local machine=manager.machine
local space=machine.devices[':maincpu'].spaces.program
local out=assert(io.open(root..'/native-state.csv','w'))
out:write('time,mode,submode,p1,p2,ammo1,ammo2,shots1,shots2,service,gunmenu,busy\n')
local previous=-1
vcop2_live_observer=emu.add_machine_frame_notifier(function()
 local t=machine.time:as_double();local tick=math.floor(t*2)
 if tick~=previous then
  previous=tick
  out:write(string.format('%.6f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n',t,space:read_u8(0x500032),space:read_u8(0x500038),space:read_u32(0x502088),space:read_u32(0x50208c),space:read_u32(0x507260),space:read_u32(0x507264),space:read_u32(0x50a2a4),space:read_u32(0x50a2a8),space:read_u8(0x5d051d),space:read_u8(0x5d0518),space:read_u32(0x5d0654)));out:flush()
 end
end)
