# -*- mode:ruby; coding:utf-8 -*-
require 'foolio'

loop  = Foolio::UV.default_loop
timer = Foolio::UV.timer_init loop

count = 0
Foolio::UV.timer_start(timer,lambda{|handle, status|
                         p count
                         count += 1
                       },2000,2000)

Thread.start {
  Foolio::UV.run(loop)
}

loop {
  puts "main thread"
  sleep 1
}
