#!/usr/bin/env rake
require "bundler/gem_tasks"

task :default => :compile

task :compile do
  Dir.chdir File.expand_path("../ext/foolio", __FILE__)
  system "ruby extconf.rb"
  system "make"
end

task :clean do
  Dir.chdir File.expand_path("../ext/libuv", __FILE__)
  system "make clean"
  Dir.chdir File.expand_path("../ext/foolio", __FILE__)
  system "rm -f *.o *.bundle *.a Makefile"
end

task :irb => :compile do
  system "ruby -I./lib -I./ext -rfoolio -rirb -e 'IRB.start' -- --simple-prompt"
end
