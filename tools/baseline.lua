-- Original cabinet inputs only: no game RAM writes, patches or skipped boot checks.
local root=assert(os.getenv('VCOP2_RESEARCH_DIR'))
local mode=os.getenv('VCOP2_DIAGNOSTIC_MODE') or 'attract'
local log=assert(io.open(root..'/events.txt','w'))
local machine=manager.machine
local ports=machine.ioport.ports
for tag,port in pairs(ports) do
    for name,field in pairs(port.fields) do log:write(string.format('PORT %s | %s | %x\n',tag,name,field.mask)) end
end
log:flush()
local function set(tag,mask,value)
    local port=assert(ports[tag],'Missing port '..tag)
    for _,field in pairs(port.fields) do if field.mask==mask then field:set_value(value);return end end
    error('Missing field '..tag..' '..mask)
end
local function press(tag,mask,enabled) set(tag,mask,enabled and 1 or 0) end
local previous=-1
vcop2_diagnostic_subscription=emu.add_machine_frame_notifier(function()
    local t=machine.time:as_double();local second=math.floor(t)
    if second~=previous then
        previous=second
        log:write(string.format('TIME %.6f IP %08x IN0 %02x IN1 %02x\n',t,machine.devices[':maincpu'].state['ip'].value,ports[':IN0']:read(),ports[':IN1']:read()));log:flush()
        if second==1 or second==5 or second%5==0 then
            for _,screen in pairs(machine.screens) do screen:snapshot(root..string.format('/frame-%03d.png',second));break end
        end
        if second==5 or second==15 or second==25 or second==35 or second==45 then
            local out=assert(io.open(root..string.format('/workram-%03d.bin',second),'wb'))
            out:write(machine.devices[':maincpu'].spaces.program:read_range(0x500000,0x5fffff,8));out:close()
        end
    end
    if mode=='gameplay' then
        press(':IN0',0x01,t>=16 and t<16.2)
        press(':IN0',0x10,t>=16.5 and t<16.8)
        press(':IN0',0x02,t>=30 and t<30.2)
        press(':IN0',0x20,t>=30.5 and t<30.8)
        set(':P1_X',0x3ff,384);set(':P1_Y',0x3ff,230)
        set(':P2_X',0x3ff,381);set(':P2_Y',0x3ff,230)
        press(':IN1',0x01,t>=24 and math.floor(t*4)%2==0)
        press(':IN1',0x02,t>=32 and math.floor(t*4)%2==0)
    elseif mode=='service' then
        -- Native prompt: Service advances the row; Test selects it.
        -- Nine Service presses from default EXIT select GUN SETTING.
        local advance=t>=12 and t<21 and (t-math.floor(t))<.2
        press(':IN0',0x04,(t>=6 and t<6.2) or (t>=23 and t<23.2))
        press(':IN0',0x08,advance)
    end
end)
