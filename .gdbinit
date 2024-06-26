file out.elf
set substitute-path /scratch .

define core0
  target extended-remote localhost:3333
end

document core0
  Attach to core 0.
end

define core1
  target extended-remote localhost:3333
end

document core1
  Attach to core 0.
end