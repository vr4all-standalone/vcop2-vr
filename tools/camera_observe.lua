-- Read-only camera provenance. Cabinet inputs are the only writes made here.
local root=assert(os.getenv('VCOP2_RESEARCH_DIR'))
local m=manager.machine
local cpu=m.devices[':maincpu']
local mem=cpu.spaces.program
local out=assert(io.open(root..'/camera-events.txt','w'))
local taps={}
local counts={}
for _,address in ipairs({0x506140,0x506044}) do
 taps[#taps+1]=mem:install_write_tap(address,address+3,'camera-original-writer',function(a,d,mask)
  counts[a]=(counts[a] or 0)+1
  if counts[a]<=12 then
   out:write(string.format('WRITE %.6f IP %08x ADDRESS %08x DATA %08x MASK %08x\n',m.time:as_double(),cpu.state['ip'].value,a,d,mask));out:flush()
  end
 end)
end
vcop2_camera_taps=taps
local ports=m.ioport.ports
local function set(tag,mask,value)
 for _,field in pairs(assert(ports[tag]).fields)do if field.mask==mask then field:set_value(value);return end end
 error('Missing cabinet field '..tag)
end
local function press(tag,mask,enabled)set(tag,mask,enabled and 1 or 0)end
local function number(address)return string.unpack('<f',string.pack('<I4',mem:read_u32(address)))end
local last=-1
vcop2_camera_subscription=emu.add_machine_frame_notifier(function()
 local t=m.time:as_double();local sec=math.floor(t)
 if sec~=last then
  last=sec
  out:write(string.format('CAMERA %.6f MODE %d SUBMODE %d SERVICE %d FOV %.9g FOCAL %.9g TANGENT %.9g PRODUCT %.9g\n',
   t,mem:read_u8(0x500032),mem:read_u8(0x500038),mem:read_u8(0x5d051d),number(0x506034),number(0x506044),number(0x506140),number(0x506044)*number(0x506140)))
  out:flush()
 end
 press(':IN0',1,t>=16 and t<16.2)
 press(':IN0',0x10,t>=16.5 and t<16.8)
 press(':IN0',2,t>=30 and t<30.2)
 press(':IN0',0x20,t>=30.5 and t<30.8)
 set(':P1_X',0x3ff,384);set(':P1_Y',0x3ff,230)
 set(':P2_X',0x3ff,381);set(':P2_Y',0x3ff,230)
 press(':IN1',1,t>=32 and math.floor(t*2)%2==0)
 press(':IN1',2,t>=36 and math.floor(t*2)%2==0)
end)
