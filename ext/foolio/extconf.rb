require "mkmf"

$CFLAGS << " -std=gnu99 -fPIC"

$DLDFLAGS << " -fPIC"
$warnflags.gsub!("-Wdeclaration-after-statement","")

if RUBY_PLATFORM =~ /darwin/
  $DLDFLAGS << " -framework CoreServices"
end

libuv_dir = File.expand_path("../../libuv", __FILE__)
foolio_dir = File.expand_path("../", __FILE__)

system "cd '#{libuv_dir}'; CFLAGS='-fPIC' make; cd '#{foolio_dir}'; cp #{libuv_dir}/uv.a #{foolio_dir}/libuv.a"

dir_config "uv", "#{libuv_dir}/include", foolio_dir
have_library "uv"

create_makefile "foolio/foolio_ext"
