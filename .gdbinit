define flash
file main.elf
load
end

define restart
run
end

define attach_swd
mon swdp_scan
attach 1
end

define pn
p test2
n
end

file main.elf
target extended-remote :4242

set mem inaccessible-by-default off
set print elements 1024
