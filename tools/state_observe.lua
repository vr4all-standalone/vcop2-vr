-- Observational diagnostics: only cabinet fields are written, memory is read-only.
local root=assert(os.getenv('VCOP2_RESEARCH_DIR'))
local m=manager.machine;local cpu=m.devices[':maincpu'];local mem=cpu.spaces.program
local f=assert(io.open(root..'/events.txt','w'))
local ports=m.ioport.ports
local function set(tag,mask,value) for _,field in pairs(assert(ports[tag]).fields)do if field.mask==mask then field:set_value(value);return end end error(tag) end
local function press(tag,mask,enabled)set(tag,mask,enabled and 1 or 0)end
local last=-1;local sec=-1
state_diagnostic=emu.add_machine_frame_notifier(function()
 local t=m.time:as_double();local tick=math.floor(t*10)
 if tick~=last then last=tick
  if t>=14 and t<45 then local out=assert(io.open(root..string.format('/ram-%04d.bin',tick),'wb'));out:write(mem:read_range(0x500000,0x5fffff,8));out:close()end
 end
 local s=math.floor(t)
 if s~=sec then sec=s;f:write(string.format('TIME %.6f IP %08x\n',t,cpu.state['ip'].value));f:flush();if s>=14 then for _,screen in pairs(m.screens)do screen:snapshot(root..string.format('/frame-%03d.png',s));break end end end
 press(':IN0',1,t>=16 and t<16.2);press(':IN0',0x10,t>=16.5 and t<16.8)
 press(':IN0',2,t>=30 and t<30.2);press(':IN0',0x20,t>=30.5 and t<30.8)
 local off=t>=39 and t<40
 set(':P1_X',0x3ff,off and 0 or 384);set(':P1_Y',0x3ff,off and 0 or 230)
 set(':P2_X',0x3ff,381);set(':P2_Y',0x3ff,230)
 press(':IN1',1,t>=32 and (math.floor(t*2)%2==0))
 press(':IN1',2,t>=36 and (math.floor(t*2)%2==0))
end)
