#! /opt/local/bin/ruby -w
# -*- mode:ruby; coding:utf-8 -*-

def for_arg(args)
  type,name = args
  case type
  when /uv_.*_t/
    "#{type} #{name}_;
  Data_Get_Struct(#{name}, #{type.gsub(/\*$/,"")}, #{name}_)"
  when "int"
    "int #{name}_ = NUM2INT(#{name})"
  when "unsigned int"
    "#{type} #{name}_n = NUM2UINT(#{name})"
  else
    "// #{type} #{name}"
  end
end

# UV_EXTERN int uv_tcp_init(uv_loop_t*, uv_tcp_t* handle);
ARGF.read.split(";").each do|func|
  func.gsub!("\n","")
  if func =~ /UV_EXTERN (\w+) ([^(]+)\((.*)\)/ then
    ret  = $1
    name = $2
    args =  $3.split(",").map{|s|
      xs = s.split(" ")
      [ xs[0..-2].join(" "), xs[-1] ]
    }

    puts <<END
// #{func}
static VALUE #{name.gsub(/^uv_/,'')}(VALUE self, #{args.map{|t,x| "VALUE #{x ? x : t.downcase}"}.join(", ")}) {
  #{args.map(&method(:for_arg)).join(";\n  ")};
  int retval = #{name}(#{args.map{|_,n| n+"_"}.join(", ")});
  return Qnil;
}

END
  end
end
