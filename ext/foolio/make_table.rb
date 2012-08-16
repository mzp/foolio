# -*- mode:ruby; coding:utf-8 -*-

%x(grep foolio_ foolio_ext.c | grep '^VALUE' | grep -v '__').each_line do|func|
  if func =~ /VALUE ([^(]+)\((.*)\)/ then
    name = $1
    args =  $2.split(",").map{|s|
      xs = s.split(" ")
      [ xs[0..-2].join(" "), xs[-1] ]
    }
    puts "Method(#{name.gsub(/^foolio_/,'')}, #{args.size-1});"
  end
end
