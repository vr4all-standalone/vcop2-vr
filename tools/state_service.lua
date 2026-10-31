-- Original cabinet service navigation only; all game memory access is read-only.
local root=assert(os.getenv('VCOP2_RESEARCH_DIR'));local m=manager.machine
local mem=m.devices[':maincpu'].spaces.program;local ports=m.ioport.ports;local log=assert(io.open(root..'/events.txt','w'));local last='';local second=-1
local function set(tag,mask,value) for _,field in pairs(assert(ports[tag]).fields)do if field.mask==mask then field:set_value(value);return end end error(tag)end
local function press(mask,enabled)set(':IN0',mask,enabled and 1 or 0)end
state_service=emu.add_machine_frame_notifier(function()
 local t=m.time:as_double();local sec=math.floor(t)
 local key=string.format('mode=%d service=%d menu=%d cursor=%d phase=%d',mem:read_u8(0x500032),mem:read_u8(0x5d051d),mem:read_u8(0x5d0518),mem:read_u8(0x5d051c),mem:read_u32(0x5d0654))
 if key~=last then log:write(string.format('%.6f %s\n',t,key));log:flush();last=key end
 if sec~=second then second=sec
  if sec==25 or sec==28 or sec==31 or sec==41 then
   local out=assert(io.open(root..string.format('/workram-%03d.bin',sec),'wb'));out:write(mem:read_range(0x500000,0x5fffff,8));out:close()
   for _,screen in pairs(m.screens)do screen:snapshot(root..string.format('/frame-%03d.png',sec));break end
  end
 end
 press(4,(t>=6 and t<6.2) or (t>=23 and t<23.2) or (t>=27 and t<27.2) or (t>=30 and t<30.2) or (t>=40 and t<40.2))
 press(8,(t>=12 and t<21 and t-math.floor(t)<.2) or (t>=25 and t<25.2) or (t>=32 and t<39 and t-math.floor(t)<.2))
end)
